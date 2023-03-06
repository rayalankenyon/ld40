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
#define GRID_SIZE 32
#define LOG_FILE "log.txt"
#define SPRITE_SIZE 8

#define UNIT_TILES_PER_SECOND 10.0f
#define UNIT_ANIMATION_FRAMES 8
#define UNIT_ANIMATION_FRAME_TIME 0.1f

#define START_UNITS 5
#define START_HP 10
#define START_RESOURCE 10
#define UNIT_COST 10

// NOTE(rayalan): 1.0 maybe for a get the highest score you can type of game
#define RESOURCE_DRAIN_TIME 0.0f

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
    int shift = stb_easy_font_width(text) / 2;
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
    COOLDOWN_HARVEST,
    COOLDOWN_MAX
} Cooldown;

typedef enum Tile_Type {
    TILE_TYPE_WATER,
    TILE_TYPE_GRASS,
    TILE_TYPE_WALKABLE,
    TILE_TYPE_TREE,
    TILE_TYPE_TREE_RED,
    TILE_TYPE_TREE_ORANGE,
    TILE_TYPE_ROCK,
    TILE_TYPE_SHRUB,
    TILE_TYPE_SHRUB_PURPLE,
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
    int animation_frame;
    float animation_time;
    float cooldown[COOLDOWN_MAX];
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
    int ally_count;
    Unit ally[MAX_ARMY_SIZE];
    int enemy_count;
    Unit enemy[MAX_ARMY_SIZE];
    int selection_count;
    Unit *selection[MAX_ARMY_SIZE];
    float resource_ticks;
} Game_State;

Sys_Config init(int argc, char **argv) {
    sys_unused(argc);
    sys_unused(argv);

    freopen(LOG_FILE, "w", stdout);

    Sys_Config cfg = { 0 };
    cfg.width = 1024; // let's do 4:3 for the classic starcraft vibe
    cfg.height = 768;
    cfg.memory = sys_alloc(1024 * 1024 * 8, 0); //
    cfg.title = "ld40";
    cfg.monitor = SYS_MONITOR_PRIMARY;

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
    sys_assert(state->sprite_sheet.width == state->sprite_sheet.height);

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
            if (k <= 93) { t = TILE_TYPE_GRASS; }
            else if (k <= 95) { t = TILE_TYPE_TREE; }
            else if (k <= 96) { t = TILE_TYPE_TREE_RED; }
            else if (k <= 97) { t = TILE_TYPE_TREE_ORANGE; }
            else if (k <= 98) { t = TILE_TYPE_SHRUB; }
            else if (k <= 99) { t = TILE_TYPE_SHRUB_PURPLE; }
            else { t = TILE_TYPE_ROCK; }
            state->tile[i][j].type = t;
            state->tile[i][j].resource = 10 * t;
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
                if(k <= 2 && state->ally_count < START_UNITS) {
                    state->ally[state->ally_count].x = (float)i;
                    state->ally[state->ally_count].y = (float)j;
                    state->ally[state->ally_count].type = k > 1 ? UNIT_TYPE_MALE : UNIT_TYPE_FEMALE;
                    state->ally[state->ally_count].hp = START_HP;
                    state->ally[state->ally_count].resource = START_RESOURCE;
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
#define color_ally 0.0f, 1.0f, 1.0f

void loop(Sys_State *sys) {
    Game_State *state = (Game_State *)sys->memory.ptr;
    init_gl(sys->width, sys->height); 

    state->resource_ticks += sys->dt;

    if(sys_key_pressed(SYS_MOUSE_LEFT)) {
        state->mouse_pressed = vec2(sys->mouse.x, sys->mouse.y);
    }
    if(sys_key_released(SYS_MOUSE_LEFT)) {
        state->mouse_released = vec2(sys->mouse.x, sys->mouse.y);
        // TODO(rayalan): this is where you select units
        //
        float pressed_x = state->camera.x + state->mouse_pressed.x * GRID_SIZE / sys->width;
        float pressed_y = state->camera.y + state->mouse_pressed.y * GRID_SIZE / sys->height;
        float released_x = state->camera.x + sys->mouse.x * GRID_SIZE / sys->width;
        float released_y = state->camera.y +  sys->mouse.y * GRID_SIZE / sys->height;
        float box_left = (pressed_x <= released_x) ? pressed_x : released_x;
        float box_right = (pressed_x >= released_x) ? pressed_x : released_x;
        float box_top = (pressed_y <= released_y) ? pressed_y : released_y;
        float box_bottom = (pressed_y >= released_y) ? pressed_y : released_y;

        state->selection_count = 0;

        for(int i = 0; i < MAX_ARMY_SIZE; i++) {
            if(state->ally[i].type > UNIT_TYPE_NONE) {
                if(state->ally[i].x <= box_right && state->ally[i].x >= box_left && state->ally[i].y <= box_bottom && state->ally[i].y >= box_top) {
                    state->selection[state->selection_count] = &state->ally[i];
                    state->selection_count++;
                }
            }
        }
 
    }
    if(sys_key_pressed(SYS_MOUSE_RIGHT)) {
        if(state->selection_count) {
            for(int i = 0; i < state->selection_count; i++) {
                state->selection[i]->look_x = state->camera.x + ((int)sys->mouse.x * GRID_SIZE / sys->width);
                state->selection[i]->look_y =  state->camera.y +((int)sys->mouse.y * GRID_SIZE / sys->height);
                state->selection[i]->flags |= UNIT_FLAG_MOVING; 
                state->selection[i]->animation_time = (float)sys_time_now();
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
    if(sys_key_pressed('S')) { // stop 
        for(int i = 0; i < state->selection_count; i++) {
            state->selection[i]->look_x = state->selection[i]->x;
            state->selection[i]->look_y = state->selection[i]->y;
        }
    }
    if(sys_key_pressed('D')) { // disperse
        for(int i = 0; i < state->selection_count; i++) {
            state->selection[i]->look_x = rand() % GRID_SIZE + state->camera.x;
            state->selection[i]->look_y = rand() % GRID_SIZE + state->camera.y;
            state->selection[i]->flags |= UNIT_FLAG_MOVING;
        }
    }

    if(sys_key_pressed('E')) { // HARVEST / EAT
        for(int i = 0; i < state->selection_count; i++) {
            int tx = (int)state->selection[i]->x+1;
            int ty = (int)state->selection[i]->y+1;
            if(state->tile[tx][ty].type > TILE_TYPE_GRASS && state->tile[tx][ty].resource > 0) {
                state->tile[tx][ty].resource--;
                state->selection[i]->resource++;
                if(state->tile[tx][ty].resource == 0) {
                    state->tile[tx][ty].type = TILE_TYPE_GRASS;
                }
            }
        }
    }
    if(sys_key_pressed('Q')) {
        for(int i = 0; i < state->selection_count; i++) {
            if(state->selection[i]->resource >= UNIT_COST && state->ally_count < MAX_ARMY_SIZE) {
                state->selection[i]->resource -= UNIT_COST;
                state->ally[state->ally_count].type = state->selection[i]->type;
                state->ally[state->ally_count].x = state->selection[i]->x;
                state->ally[state->ally_count].y = state->selection[i]->y;
                state->ally[state->ally_count].look_x = state->selection[i]->look_x;
                state->ally[state->ally_count].look_y = state->selection[i]->look_y;
                state->ally[state->ally_count].hp = START_HP;
                state->ally[state->ally_count].resource = 0;
                state->ally_count++;
            }
        }
    }

    // UPDATE allied units
    for(int i = 0; i < MAX_ARMY_SIZE; i++) {
        
        if(state->resource_ticks >= RESOURCE_DRAIN_TIME && state->ally[i].resource > 0) {
            state->ally[i].resource -= 1;
        }
        if(state->ally[i].x < state->ally[i].look_x + 0.1f && state->ally[i].x > state->ally[i].look_x - 0.1f
                && (state->ally[i].flags & UNIT_FLAG_MOVING) && state->ally[i].y < state->ally[i].look_y + 0.1f 
                && state->ally[i].y > state->ally[i].look_y - 0.1f) {
            state->ally[i].flags ^= UNIT_FLAG_MOVING;
            state->ally[i].x = state->ally[i].look_x;
            state->ally[i].y = state->ally[i].look_y;
            state->ally[i].animation_frame = 0;
            state->ally[i].animation_time = 0;
        }

        if(state->ally[i].flags & UNIT_FLAG_MOVING) {

            state->ally[i].animation_time += sys->dt;
            if(state->ally[i].animation_time >= UNIT_ANIMATION_FRAME_TIME) {
                state->ally[i].animation_frame = (state->ally[i].animation_frame + 1) % UNIT_ANIMATION_FRAMES;
                state->ally[i].animation_time = 0;
            }

            float dx = 0.0f;
            float dy = 0.0f;

            if(state->ally[i].x < state->ally[i].look_x - 0.1f) {
                dx = sys->dt * UNIT_TILES_PER_SECOND;
            } else if(state->ally[i].x > state->ally[i].look_x + 0.1f) {
                dx = -1.0f * sys->dt * UNIT_TILES_PER_SECOND; 
            }

            if(state->ally[i].y < state->ally[i].look_y - 0.1f) {
                dy = sys->dt * UNIT_TILES_PER_SECOND;
            } else if(state->ally[i].y > state->ally[i].look_y + 0.1f) {
               dy = -1.0f * sys->dt * UNIT_TILES_PER_SECOND;
            }

            state->ally[i].x += dx;
            state->ally[i].y += dy;
        }
    }

    // UPDATE tick timers
    if(state->resource_ticks >= RESOURCE_DRAIN_TIME) { state->resource_ticks = 0.0f; }

    // DRAW map
    float render_size = 1.0f/ GRID_SIZE;
    float tile_size = 1.0f / state->sprite_sheet.width * SPRITE_SIZE;

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
    glBindTexture(GL_TEXTURE_2D, state->sprite_sheet.id);

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
                    
                    float draw_x = state->ally[i].x - state->camera.x;
                    float  draw_y = state->ally[i].y - state->camera.y;
                    int tx = state->ally[i].animation_frame;
                    int ty = state->sprite_sheet.width / SPRITE_SIZE - state->ally[i].type + 1;
                    glTexCoord2f(tx * tile_size, (ty-1) * tile_size);
                    glVertex2f(draw_x * render_size, (draw_y+1) * render_size);
                    glTexCoord2f((tx+1) * tile_size, (ty-1) * tile_size);
                    glVertex2f((draw_x+1) * render_size, (draw_y+1) * render_size);
                    glTexCoord2f((tx+1) * tile_size, ty * tile_size);
                    glVertex2f((draw_x+1) * render_size, draw_y * render_size);
                    glTexCoord2f(tx * tile_size, ty * tile_size);
                    glVertex2f(draw_x * render_size, draw_y * render_size);
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


    // draw total hp / resource counts here
    uint64_t total_hp = 0;
    uint64_t total_resource = 0;

    for(int i = 0; i < state->ally_count; i++) {
        total_hp += state->ally[i].hp;
        total_resource += state->ally[i].resource;
    }

    char hp_buf[64], res_buf[64];
    sprintf(hp_buf, "%lld", total_hp);
    sprintf(res_buf, "%lld", total_resource);

    glColor3f(color_white);
    right_string((float)sys->width - 32.0f, sys->height * 0.02f, 2.0f, hp_buf);
    right_string((float)sys->width - 32.0f, sys->height * 0.05f, 2.0f, res_buf);
}



void quit(Sys_State *sys) {
    // NOTE(rayalan): idk if I want the user to be require to do this for sys.h
    sys_free(sys->memory);
    fclose(stdout);
}

/* TODO(rayalan): 

   map generation
*/
