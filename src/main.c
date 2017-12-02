#define SYS_INIT_PROC init
#define SYS_LOOP_PROC loop
#define SYS_QUIT_PROC quit
#define SYS_OPENGL
#define SYS_IMPLEMENTATION
#include "sys.h"


Sys_Config init(int argc, char **argv) {
    Sys_Config cfg = { 0 };
    cfg.width = 800; // let's do 4:3 for the classic starcraft vibe
    cfg.height = 600;
    cfg.title = "I have no idea what I am doing";
    return cfg;
}

void loop(Sys_State sys) {


}

void quit(Sys_State sys) {

}



// TODO(rayalan): unit definitons
//
