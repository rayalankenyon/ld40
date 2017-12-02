#define SYS_INIT_PROC init
#define SYS_LOOP_PROC loop
#define SYS_QUIT_PROC quit
#define SYS_OPENGL
#define SYS_OPENGL_MAJOR 1
#define SYS_OPENGL_MINOR 2
#define SYS_OPENGL_COMBATIBILITY
#define SYS_IMPLEMENTATION
#include "sys.h"

#include "stb_easy_font.h"

void debug_string(float x, float y, char *text)
{
  static char buffer[99999]; // ~500 chars
  int num_quads;

  num_quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));

  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 16, buffer);
  glDrawArrays(GL_QUADS, 0, num_quads*4);
  glDisableClientState(GL_VERTEX_ARRAY);
}

Sys_Config init(int argc, char **argv) {
    Sys_Config cfg = { 0 };
    cfg.width = 800; // let's do 4:3 for the classic starcraft vibe
    cfg.height = 600;
    cfg.title = "I have no idea what I am doing";

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

void loop(Sys_State *sys) {
    init_gl(sys->width, sys->height); 
    glColor3f(color_pink);
    debug_string(50, 50, "hi how are ya?");
}

void quit(Sys_State *sys) {

}



// TODO(rayalan): unit definitons
//
