#define SYS_INIT_PROC init
#define SYS_LOOP_PROC loop
#define SYS_QUIT_PROC quit
#define SYS_OPENGL
#define SYS_OPENGL_MAJOR 1
#define SYS_OPENGL_MINOR 2
#define SYS_OPENGL_COMBATIBILITY
#define SYS_IMPLEMENTATION
#include "sys.h"

#define LINEAR_ALGEBRA_IMPLEMENTATION
#include "linear_algebra.h"

#include "stb_easy_font.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <math.h>
#include <string.h>

#define SPRITE_SHEET_NAME "sprites.png"
#define MAP_GRID_SIZE 1024
#define MAX_ARMY_SIZE 256
#define MAP_SEED 314159268
#define GRID_SIZE 32
#define LOG_FILE "log.txt"
//=============================================================================
//
//
//  DRAWING STRINGS
//
//
//=============================================================================
void draw_string(float x, float y, char *text)
{
  static char buffer[99999]; // ~500 chars
  int num_quads;

  num_quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));

  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 16, buffer);
  glDrawArrays(GL_QUADS, 0, num_quads*4);
  glDisableClientState(GL_VERTEX_ARRAY);
}

void left_string(float x, float y, float size, char *text) {
    glPushMatrix();
    glScalef(size, size, 1.0f);
    draw_string(x/size, y/size, text);
    glPopMatrix();
}

void centered_string(float x, float y, float size, char *text) {
    glPushMatrix();
    int shift = stb_easy_font_width(text) / 2.0f;
    glScalef(size, size, 1.0f);
    draw_string(x/size - shift, y/size, text);
    glPopMatrix();
}

void right_string(float x, float y, float size, char *text) {
    glPushMatrix();
    int shift = stb_easy_font_width(text);
    glScalef(size, size, 1.0f);
    draw_string(x/size - shift, y/size, text);
    glPopMatrix();
}



//=============================================================================
//
//
//  SYSTEM
//
//
//=============================================================================

typedef enum Cooldown {
    COOLDOWN_ATTACK,
    COOLDOWN_EAT,
    COOLDOWN_HEAL,
    COOLDOWN_PILL,
    COOLDOWN_MAX
} Cooldown;


// 24 bytes * 512 total units = 12 KB
typedef struct Unit { 
    float x, y;
    int type;
    int hp;
    int resource;
    float cooldowns[COOLDOWN_MAX];
} Unit;

// 2 bytes * 1024 * 1024 = 2 MB 
typedef struct Tile {
    unsigned char type;
    unsigned char resource;
} Tile;

typedef struct Sprite_Sheet {
    unsigned int id;
    int width, height;
} Sprite_Sheet;

typedef struct Game_State {
    Sprite_Sheet sprite_sheet;
    Vec2 mouse_released;
    Vec2 mouse_pressed;
    Vec2 camera;
    Tile tile[MAP_GRID_SIZE][MAP_GRID_SIZE];
    Unit ally[MAX_ARMY_SIZE];
    Unit enemy[MAX_ARMY_SIZE];
} Game_State;

Sys_Config init(int argc, char **argv) {
    freopen(LOG_FILE, "w", stdout);

    Sys_Config cfg = { 0 };
    cfg.width = 1024; // let's do 4:3 for the classic starcraft vibe
    cfg.height = 768;
    cfg.memory = sys_alloc(1024 * 1024 * 4, 0); // 4 megabytes memory
    cfg.title = "I have no idea what I am doing";

    Game_State *state = (Game_State *)cfg.memory.ptr;

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    
    glGenTextures(1, &state->sprite_sheet.id);
    sys_assert(state->sprite_sheet.id);
    glBindTexture(GL_TEXTURE_2D, state->sprite_sheet.id); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    int channels;
    stbi_set_flip_vertically_on_load(1);
    unsigned char *data = stbi_load(SPRITE_SHEET_NAME, &state->sprite_sheet.width, &state->sprite_sheet.height, &channels, 0);
    sys_assert(channels == 4);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, state->sprite_sheet.width, state->sprite_sheet.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0); 


    srand(MAP_SEED);
    // /TODO(rayalan): fill the map type information with random values between
    //  0 and 16?
    printf("random map:\n");
    for(int i = 0; i < MAP_GRID_SIZE; i++) {
        for(int j = 0; j < MAP_GRID_SIZE; j++) {
            // this means that the type can indicate any texture from the bottom row of the sprite sheet
            state->tile[i][j].type = rand() % 16;
            printf("%i ", state->tile[i][j].type );
        }
        printf("\n");
    }
    printf("\n");
    state->camera.x = 512;
    state->camera.y = 512;
    return cfg;
}

inline void init_gl(int w, int h) 
{
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity(); 
    glViewport(0, 0, w, h);
    glOrtho(0.0f, w, h, 0.0f, 0.0f, 1.0f);
}

#define color_pink 1.0f, 0.0f, 0.5f
#define color_red 1.0f, 0.0f, 0.0f
#define color_selection_box 0.2f, 0.7f, 0.2f, 0.4f
#define color_white 1.0f, 1.0f, 1.0f

void loop(Sys_State *sys) {
    double frame_start = sys_time_now();
    Game_State *state = (Game_State *)sys->memory.ptr;
    init_gl(sys->width, sys->height); 

    if(sys_key_pressed(SYS_MOUSE_LEFT)) {
        state->mouse_pressed = vec2(sys->mouse.x, sys->mouse.y);
    }
    if(sys_key_released(SYS_MOUSE_LEFT)) {
        state->mouse_released = vec2(sys->mouse.x, sys->mouse.y);
        // TODO(rayalan): this is where you select units
    }

    if(sys_key_pressed('W')) {
        state->camera.y--;
    }
    if(sys_key_pressed('S')) {
        state->camera.y++;
    }
    if(sys_key_pressed('A')) {
        state->camera.x--;
    }
    if(sys_key_pressed('D')) {
        state->camera.x++;
    }
    
    // DRAW map
    float render_size = 1.0f/ GRID_SIZE;
    float tile_size = 0.0625; // 1 / (128/8)

    // NOTE(rayalan): the camera position is the top left most square
    glBindTexture(GL_TEXTURE_2D, state->sprite_sheet.id);
    glColor3f(color_white);
    glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
        glBegin(GL_QUADS);
            for(int i = 1; i <= GRID_SIZE; i++) {
                for(int j = 1; j <= GRID_SIZE; j++) {
                    int tx = i + state->camera.x;
                    int ty = j + state->camera.y;
                    // what texture index is at tile
                    // TODO(rayalan): this you should just feed this an index
                    //      what it should support any index in the sprite sheet
                    int t = state->tile[tx][ty].type;
                    glTexCoord2f(t * tile_size, 0.0f);
                    glVertex2f((i-1)*render_size, j*render_size);
                    glTexCoord2f((t+1) * tile_size, 0);
                    glVertex2f(i*render_size, j*render_size);
                    glTexCoord2f((t+1) * tile_size, tile_size);
                    glVertex2f(i*render_size, (j-1)*render_size);
                    glTexCoord2f(t * tile_size, tile_size);
                    glVertex2f((i-1)*render_size, (j-1)*render_size);        
                }
            }
    glEnd();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);


    // DRAW selection box
    if(sys_key_down(SYS_MOUSE_LEFT)) {
        glColor4f(color_selection_box);
        glBegin(GL_QUADS);
            glVertex2f(state->mouse_pressed.x, sys->mouse.y);
            glVertex2f(sys->mouse.x, sys->mouse.y);
            glVertex2f(sys->mouse.x, state->mouse_pressed.y);
            glVertex2f(state->mouse_pressed.x, state->mouse_pressed.y);
        glEnd();
    }

    
    // DRAW play area text here

#if 0 // NOTE(rayalan): drawing long string is super expensive
    glColor3f(color_white);
    left_string(50, 50, 2.0f, "this string is on the left");

    centered_string(sys->width/2.0f, sys->height/2.0f, 2.0f, "hahahah");
    right_string(sys->width - 50, sys->height * 0.1f, 2.0f, "resources");
    centered_string(sys->width/2.0f, sys->height * 0.7f, 1.0f, "you must construct additional pylons");
#endif
    
    // draw minimap
    //glColor3f(color_pink);
    glBindTexture(GL_TEXTURE_2D, state->sprite_sheet.id);
    glColor3f(1.0f, 1.0f, 1.0f);
    glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(0.0f, 1.0f);
            glTexCoord2f(0.0625f, 0.0f);
            glVertex2f(1.0f, 1.0f);
            glTexCoord2f(0.0625f, 0.0625f);
            glVertex2f(1.0f, 0.8f);
            glTexCoord2i(0.0f, 0.0625f);
            glVertex2f(0.0f, 0.8f);
        glEnd();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);

    //glFlush();
}



void quit(Sys_State *sys) {
    // NOTE(rayalan): idk if I want the user to be require to do this for sys.h
    sys_free(sys->memory);
    fclose(stdout);
}

/* TODO(rayalan): 
    [x] draw selection box
    [x] draw strings with justification

    what does the sprite sheet look like? 
    how big is the map?

    max units 1024? 
        data per unit
        hp int
        unit type int
        resource (could be hunger or mana or whatever depending on unit) u16
        cooldown[4]  double q w e r attach, 

    map size 1024 * 1024 
        type int
        resource amount int
        units on tile?
        flags -> no need just use extra tile_type like tile_type_walkable if(tile.type > TILE_TYPE_WALKABLE) { // you can walk here }
            -> will be blank spaces in sprite sheet at these values
            walkable?
            swimable?
            
        

    [kind of] draw static textured rectangles 
    draw animated textured rectangles 
*/
