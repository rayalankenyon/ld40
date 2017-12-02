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

#define SPRITE_SHEET_NAME "sprites.png"

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

typedef struct Game_State {
    unsigned int sprite_sheet;
    Vec2 mouse_released;
    Vec2 mouse_pressed;
} Game_State;

Sys_Config init(int argc, char **argv) {
    Sys_Config cfg = { 0 };
    cfg.width = 1024; // let's do 4:3 for the classic starcraft vibe
    cfg.height = 768;
    cfg.memory = sys_alloc(1024 * 1024 * 2, 0); // 2 megabytes memory
    cfg.title = "I have no idea what I am doing";

    Game_State *state = (Game_State *)cfg.memory.ptr;

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    
    glGenTextures(1, &state->sprite_sheet);
    sys_assert(state->sprite_sheet);
    glBindTexture(GL_TEXTURE_2D, state->sprite_sheet); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);
    unsigned char *data = stbi_load(SPRITE_SHEET_NAME, &width, &height, &channels, 0);
    sys_assert(channels == 4);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0); 

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
    }

    

    // DRAW mpa



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

    glColor3f(color_pink);
    left_string(50, 50, 2.0f, "this string is on the left");
    centered_string(sys->width/2.0f, sys->height/2.0f, 2.0f, "yet another string, but this time it's a bit longer.");
    right_string(sys->width - 50, sys->height * 0.1f, 2.0f, "resources\ngo\nhere");

    centered_string(sys->width/2.0f, sys->height * 0.7f, 1.0f, "you must construct additional pylons");
#if 0
    PushWarning("you must construct aditional pylons");
#endif

    // DRAW in game UI here

    //glColor3f(color_pink);
    glBindTexture(GL_TEXTURE_2D, state->sprite_sheet);
    glColor3f(1.0f, 1.0f, 1.0f);
    glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(0.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0625f);
            glVertex2f(1.0f, 1.0f);
            glTexCoord2f(0.0625f, 0.0625f);
            glVertex2f(1.0f, 0.8f);
            glTexCoord2i(0, 0.0625f);
            glVertex2f(0.0f, 0.8f);
        glEnd();
    glPopMatrix();
    glBindTexture(GL_TEXTURE_2D, 0);
        
    //glFlush();
}



void quit(Sys_State *sys) { }

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
        

    draw static textured rectangles 
    draw animated textured rectangles 
*/
