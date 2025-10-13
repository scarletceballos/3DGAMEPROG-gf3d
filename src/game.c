#include <SDL.h>
#include <math.h>

#include "simple_json.h"
#include "simple_logger.h"

#include "gfc_input.h"
#include "gfc_config_def.h"
#include "gfc_vector.h"
#include "gfc_matrix.h"
#include "gfc_audio.h"
#include "gfc_string.h"
#include "gfc_actions.h"

#include "gf2d_sprite.h"
#include "gf2d_font.h"
#include "gf2d_actor.h"
#include "gf2d_mouse.h"

#include "gf3d_vgraphics.h"
#include "gf3d_pipeline.h"
#include "gf3d_swapchain.h"
#include "gf3d_camera.h"
#include "gf3d_mesh.h"
#include "gf3d_texture.h"
#include "entity.h"
#include "monster.h"

extern int __DEBUG;

static int _done = 0;
static Uint32 frame_delay = 33;
static float fps = 0;

void parse_arguments(int argc, char* argv[]);
void game_frame_delay();

void exitGame()
{
    _done = 1;
}

int main(int argc, char* argv[])
{
    //local variables
    Mesh* mesh;
    Texture* texture;
    //  hard test with two dinos
    Monster* dino1;
    Monster* dino2;
    GFC_Vector3D lightPos = { 8, 15, 12 };
    GFC_Vector3D cam = { 0, -1, 55 };
    //initializtion    
    parse_arguments(argc, argv);
    init_logger("gf3d.log", 0);
    slog("gf3d begin");
    //gfc init
    gfc_input_init("config/input.cfg");
    gfc_config_def_init();
    gfc_action_init(1024);
    //gf3d init
    gf3d_vgraphics_init("config/setup.cfg");
    gf2d_font_init("config/font.cfg");
    gf2d_actor_init(1000);

    //entity system init
    entity_system_init(10);
    monster_system_init(5);

    //game init
    srand(SDL_GetTicks());
    slog_sync();
    gf2d_mouse_load("actors/mouse.actor");
    
    mesh = gf3d_mesh_load("models/dino/dino.obj");
    texture = gf3d_texture_load("models/dino/dino.png");
    if (!texture)
    {
        slog("Failed to load model texture, using default texture");
    }
    if (!mesh) {
        slog("Failed to load dino mesh!");
    } else {
        slog("mesh loaded successfully");
        
        // have two dinos side by side
        dino1 = monster_new(mesh, texture, gfc_vector3d(-12, 0, 0));
        dino2 = monster_new(mesh, texture, gfc_vector3d(12, 0, 0));
        
        if (dino1 && dino2) {
            slog("both models made successfully");
        }
    }
    gf3d_camera_set_position(cam);
    gf3d_camera_look_at(gfc_vector3d(0, 0, 4), NULL);
    while (!_done)
    {
        gfc_input_update();
        gf2d_mouse_update(); 
        gf2d_font_update();

        gf3d_camera_update_view();
        gf3d_vgraphics_render_start();

        entity_system_draw_all(lightPos, GFC_COLOR_WHITE);

        gf2d_font_draw_line_tag("ALT+F4 to exit", FT_H1, GFC_COLOR_WHITE, gfc_vector2d(10, 10));
        gf2d_mouse_draw();
        gf3d_vgraphics_render_end();

        if (gfc_input_command_down("exit")) _done = 1; // exit condition
        game_frame_delay();
    }
    vkDeviceWaitIdle(gf3d_vgraphics_get_default_logical_device());
    //cleanup
    slog("gf3d program end");
    exit(0);
    // slog_sync(); // test things
    // return 0;    // test things
}

void parse_arguments(int argc, char* argv[])
{
    int a;

    for (a = 1; a < argc;a++)
    {
        if (strcmp(argv[a],"--debug") == 0)
        {
            __DEBUG = 1;
        }
    }
}

void game_frame_delay()
{
    Uint32 diff;
    static Uint32 now;
    static Uint32 then;
    then = now;
    slog_sync();// make sure logs get written when we have time to write it
    now = SDL_GetTicks();
    diff = (now - then);
    if (diff < frame_delay)
    {
        SDL_Delay(frame_delay - diff);
    }
    fps = 1000.0/MAX(SDL_GetTicks() - then,0.001);
//     slog("fps: %f",fps);
}
/*eol@eof*/
