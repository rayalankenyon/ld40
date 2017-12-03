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
#include <inttypes.h>

#define SPRITE_SHEET_NAME "sprites.png"
#define MAP_GRID_SIZE 1024
#define MAX_ARMY_SIZE 256
#define GRID_SIZE 16
#define LOG_FILE "log.txt"
#define NOISE_STEP 1.0f

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


typedef enum Tile_Type {
    TILE_TYPE_WATER,
    TILE_TYPE_GRASS,
    TILE_TYPE_TREE,
    TILE_TYPE_TREE_RED,
    TILE_TYPE_TREE_ORANGE,
    TILE_TYPE_ROCK,
    TILE_TYPE_SHRUB,
    TILE_TYPE_SHRUB_PRUPLE,
    TILE_TYPE_MAX
} Tile_Type;

typedef enum Unit_Type {
    UNIT_TYPE_NONE,
    UNIT_TYPE_MALE,
    UNIT_TYPE_FEMALE,
    UNIT_TYPE_ENEMY,
    UNIT_TYPE_MAX
} Unit_Type;

#define UNIT_FLAG_MOVING    0x00000001
#define UNIT_FLAG_SWIMMING  0x00000002

typedef struct Unit { 
    float x, y, look_x, look_y;
    int type;
    int hp;
    int resource;
    int flags; // is_moving / etc
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
    int ally_count;
    Unit enemy[MAX_ARMY_SIZE];
    int enemy_count;
    Unit *selection[MAX_ARMY_SIZE];
    int selection_count;
} Game_State;

Sys_Config init(int argc, char **argv) {
    freopen(LOG_FILE, "w", stdout);

    Sys_Config cfg = { 0 };
    cfg.width = 1024; // let's do 4:3 for the classic starcraft vibe
    cfg.height = 768;
    cfg.memory = sys_alloc(1024 * 1024 * 8, 0); //
    cfg.title = "I have no idea what I am doing";
    // NOTE(rayalan): sys.h fails if you start fullscreen adn alt+enter

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


    int map_seed = (int)sys_time_now() ^ (int)(&cfg);
    srand(map_seed);

    // MAP GENERATION
    // ========================================================================
    for(int i = 0; i < MAP_GRID_SIZE; i++) {
        for(int j = 0; j < MAP_GRID_SIZE; j++) {
            // this means that the type can indicate any texture from the bottom row of the sprite sheet
            int k = (int)rand() % 100;
            int t = 0;
            if(k <= 15) { t = TILE_TYPE_WATER; }
            else if (k <= 85) { t = TILE_TYPE_GRASS; }
            else if (k <= 93) { t = TILE_TYPE_TREE; }
            else if (k <= 96) { t = TILE_TYPE_TREE_RED; }
            else if (k <= 97) { t = TILE_TYPE_TREE_ORANGE; }
            else { t = TILE_TYPE_ROCK; }
            state->tile[i][j].type = t;
        }
    }

    state->camera.x = 512;
    state->camera.y = 512;

    // SPAWN UNITS
    // ========================================================================
    state->ally_count = 0;
    state->enemy_count = 0;
    for(int i = state->camera.x; i < state->camera.x + GRID_SIZE; i++) {
        for(int j = state->camera.y; j < state->camera.y + GRID_SIZE; j++) {
            if(state->tile[i][j].type == TILE_TYPE_GRASS) {
                unsigned int k = rand() % 100;
                if(k <= 5 && state->ally_count < MAX_ARMY_SIZE) {
                    state->ally[state->ally_count].x = (float)i + 0.5f;
                    state->ally[state->ally_count].y = (float)j + 0.5f;
                    state->ally[state->ally_count].type = UNIT_TYPE_MALE;
                    state->ally[state->ally_count].hp = 100;
                    state->ally[state->ally_count].resource = 100;
                    state->ally_count++;
                }    
            }
        }
    }

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
    Game_State *state = (Game_State *)sys->memory.ptr;
    init_gl(sys->width, sys->height); 

    if(sys_key_pressed(SYS_MOUSE_LEFT)) {
        state->mouse_pressed = vec2(sys->mouse.x, sys->mouse.y);
    }
    if(sys_key_released(SYS_MOUSE_LEFT)) {
        state->mouse_released = vec2(sys->mouse.x, sys->mouse.y);
        // TODO(rayalan): this is where you select units
        //
        int pressed_x = state->camera.x + (int)(state->mouse_pressed.x * GRID_SIZE / sys->width);
        int pressed_y = state->camera.y + (int)(state->mouse_pressed.y * GRID_SIZE / sys->height);
        int released_x = state->camera.x + (int)(sys->mouse.x * GRID_SIZE / sys->width);
        int released_y = state->camera.y + (int)(sys->mouse.y * GRID_SIZE / sys->height);
        int box_left = (pressed_x <= released_x) ? pressed_x : released_x;
        int box_right = (pressed_x >= released_x) ? pressed_x : released_x;
        int box_top = (pressed_y <= released_y) ? pressed_y : released_y;
        int box_bottom = (pressed_y >= released_y) ? pressed_y : released_y;

        state->selection_count = 0;

        for(int i = 0; i < MAX_ARMY_SIZE; i++) {
            if(state->ally[i].type > UNIT_TYPE_NONE) {
                int x = (int)state->ally[i].x;
                int y = (int)state->ally[i].y;
                if(x <= box_right && x >= box_left && y <= box_bottom && y >= box_top) {
                    state->selection[state->selection_count] = &state->ally[i];
                    state->selection_count++;
                }
            }
        }
 
    }
    if(sys_key_pressed(SYS_MOUSE_RIGHT)) {
        if(state->selection_count) {
            for(int i = 0; i < state->selection_count; i++) {
                state->selection[i]->look_x = (float)state->camera.x + ((float)sys->mouse.x * GRID_SIZE / sys->width);
                state->selection[i]->look_y =  (float)state->camera.y + ((float)sys->mouse.y * GRID_SIZE / sys->height);
                state->selection[i]->flags |= UNIT_FLAG_MOVING; 
            }
        }
    }

    if(sys_key_down('O')) {
        state->camera.y--;
    }
    if(sys_key_down('L')) {
        state->camera.y++;
    }
    if(sys_key_down('K')) {
        state->camera.x--;
    }
    if(sys_key_down(SYS_KEY_SEMICOLON)) {
        state->camera.x++;
    }
    
    if(sys_key_pressed(SYS_KEY_F2)) { // select all I guess?
        state->selection_count = 0;
       for(int i = 0; i < MAX_ARMY_SIZE; i++) {
           if(state->ally[i].type > UNIT_TYPE_NONE) {
               state->selection[state->selection_count] = &state->ally[i];
               state->selection_count++;
           }
       }
    }
    if(sys_key_pressed(SYS_KEY_F3)) { // select all on screen I guess?
        state->selection_count = 0;
        for(int i = 0; i < MAX_ARMY_SIZE; i++) {
            if(state->ally[i].type > UNIT_TYPE_NONE) {
                if(state->ally[i].x >= state->camera.x && state->ally[i].x <= state->camera.x + GRID_SIZE
                   && state->ally[i].y >= state->camera.y && state->ally[i].y <= state->camera.y + GRID_SIZE) {
                    state->selection[state->selection_count] = &state->ally[i];
                    state->selection_count++; 
                }
            }
        }
    }
    if(sys_key_pressed(SYS_KEY_ESC)) { // select none
        state->selection_count = 0;
    }


#define UNIT_TILES_PER_SECOND 5.0f
    // UPDATE allied units
    for(int i = 0; i < MAX_ARMY_SIZE; i++) {
        
        if(state->ally[i].x < state->ally[i].look_x + 0.1f && state->ally[i].x > state->ally[i].look_x - 0.1f
                && (state->ally[i].flags & UNIT_FLAG_MOVING) && state->ally[i].y < state->ally[i].look_y + 0.1f 
                && state->ally[i].y > state->ally[i].look_y - 0.1f) {
            state->ally[i].flags ^= UNIT_FLAG_MOVING;
        }

        if(state->ally[i].x < state->ally[i].look_x && (state->ally[i].flags & UNIT_FLAG_MOVING)) {
            
            state->ally[i].x += sys->dt * UNIT_TILES_PER_SECOND;
        }
        if(state->ally[i].y < state->ally[i].look_y && (state->ally[i].flags & UNIT_FLAG_MOVING)) {
            state->ally[i].y += sys->dt * UNIT_TILES_PER_SECOND;
        }
        if(state->ally[i].x > state->ally[i].look_x && (state->ally[i].flags & UNIT_FLAG_MOVING)) {
            state->ally[i].x -= sys->dt * UNIT_TILES_PER_SECOND; 
        }
        if(state->ally[i].y > state->ally[i].look_y && (state->ally[i].flags & UNIT_FLAG_MOVING)) {
            state->ally[i].y -= sys->dt * UNIT_TILES_PER_SECOND;
        }

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


    // DRAW allied units
//    glBindTexture(GL_TEXTURE_2D, state->sprite_sheet.id);
    glColor3f(color_white);
    glPushMatrix();
    {
        glLoadIdentity();
        glOrtho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
        glBegin(GL_QUADS);
            for(int i = 0; i < state->ally_count; i++) {
                int map_x = (int)state->ally[i].x;
                int map_y = (int)state->ally[i].y;
                
                if(map_x >= state->camera.x && map_x <= state->camera.x + GRID_SIZE
                   && map_y >= state->camera.y && map_y <= state->camera.y + GRID_SIZE) {

                    int draw_x = map_x - state->camera.x;
                    int draw_y = map_y - state->camera.y;

                    glVertex2f(draw_x * render_size, (draw_y+1) * render_size);
                    glVertex2f((draw_x+1) * render_size, (draw_y+1) * render_size);
                    glVertex2f((draw_x+1) * render_size, draw_y * render_size);
                    glVertex2f(draw_x * render_size, draw_y * render_size);
                    //__debugbreak();
                }
            }
        glEnd();
    }
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);


    // DRAW UI
    // ========================================================================
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
#if 0 
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
#endif 

#if 1 // draw the number of units selected
    char sel_text_buffer[64];
    sprintf(sel_text_buffer, "%i units selected", state->selection_count);
    glColor3f(color_white);
    centered_string(sys->width/2.0f, sys->height * 0.1f, 1.0f, sel_text_buffer);
#endif 

}



void quit(Sys_State *sys) {
    // NOTE(rayalan): idk if I want the user to be require to do this for sys.h
    sys_free(sys->memory);
    fclose(stdout);
}

/* TODO(rayalan): 

   map generation
*/
