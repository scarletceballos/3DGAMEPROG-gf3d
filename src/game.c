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
#include "gfc_text.h"

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
#include "camera_entity.h"
// #include "world.h"  // not really needed until world map exists

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
    // World* world;
    Mesh* mesh;
    Texture* texture;
    Monster* dino1;
    Monster* dino2;
    Monster* dino3;
    Monster* dino4;
    Entity* camera_entity;
    GFC_Vector3D lightPos = { 8, 15, 12 };
    //initialization    
    parse_arguments(argc, argv);
    init_logger("gf3d.log", 0);
    slog("gf3d begin");
    //gfc init
    gfc_input_init("config/input.cfg");
    // Setup controls for entity direction and camera rotation
    gfc_input_command_add_key("walkleft", "LEFT");
    gfc_input_command_add_key("walkright", "RIGHT");
    gfc_input_command_add_key("up", "UP");
    gfc_input_command_add_key("down", "DOWN");
    gfc_config_def_init();
    gfc_action_init(1024);
    //gf3d init
    gf3d_vgraphics_init("config/setup.cfg");
    gf2d_font_init("config/font.cfg");
    gf2d_actor_init(1000);

    //entity system init
    entity_system_init(10);
    monster_system_init(20); // Increase from 5 to 20 for more digis

    //game init
    srand(SDL_GetTicks());
    slog_sync();
    gf2d_mouse_load("actors/mouse.actor");
    
    // TODO: Load world with terrain - after getting map
    /*
    world = world_load("defs/terrain/terrain.def");
    if (!world) {
        slog("Failed to load world from terrain.using test_world.def");
        world = world_load("defs/terrain/test_world.def");
        if (!world) {
            slog("Failed to load world file, creating empty world - NOT STABLE");
            world = world_new();
        }
    } else {
        slog("Successfully loaded world from terrain.def");
    }
    */
    
    // Load entity assets
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
        
        // make four dinos in square formation
        dino1 = monster_new(mesh, texture, gfc_vector3d(-15, -15, 0)); // Bottom-left
        dino2 = monster_new(mesh, texture, gfc_vector3d(15, -15, 0));  // Bottom-right
        dino3 = monster_new(mesh, texture, gfc_vector3d(-15, 15, 0));  // Top-left
        dino4 = monster_new(mesh, texture, gfc_vector3d(15, 15, 0));   // Top-right
        
        // IF COND TO TEST ENTITIES ONLY
        if (dino1 && dino2 && dino3 && dino4) {
            slog("all four models created successfully");
            
            // Create camera entity that follows dino1 - scrap for new class made camera
            camera_entity = camera_entity_spawn(
                gfc_vector3d(0, -20, 10), // Camera position
                dino1->entity              // Follow dino1
            );
            
            if (camera_entity) {
                slog("Camera entity created and following dino1");
            }
        }
    }
    
    slog("Entering main game loop");
    int frame_count = 0;
    while (!_done)
    {
        // Constantly check if things are being sent though input
        if (frame_count < 5) slog("Frame %d: Starting input update", frame_count);
        gfc_input_update();
        if (frame_count < 5) slog("Frame %d: Starting mouse update", frame_count);
        gf2d_mouse_update(); 
        if (frame_count < 5) slog("Frame %d: Starting font update", frame_count);
        gf2d_font_update();
        
        // Update all thinking, entity
        entity_system_think_all();
        entity_system_update_all();
        
        // Handle camera angle adjustment with left/right keys
        Entity* cam_ent = camera_entity_get();
        if (cam_ent) {
            if (gfc_input_command_down("walkleft")) {
                camera_entity_adjust_angle(cam_ent, 0.02f); // Rotate left
            }
            if (gfc_input_command_down("walkright")) {
                camera_entity_adjust_angle(cam_ent, -0.02f); // Rotate right
            }
        }
        
        // entity creation/destruction
        if (gfc_input_key_pressed("SPACE")) {
            // Create new digi at random position
            GFC_Vector3D randomPos = gfc_vector3d(
                (rand() % 40) - 20,  // Random X between -20 and 20
                (rand() % 40) - 20,  // Random Y between -20 and 20
                0
            );
            Monster* newMonster = monster_new(mesh, texture, randomPos);
            if (newMonster) {
                slog("Dynamically created new monster at (%.1f, %.1f, %.1f)", 
                     randomPos.x, randomPos.y, randomPos.z);
            }
        }
        
        if (gfc_input_key_pressed("DELETE")) {
            // test cleanup by removing oldest digi
            monster_cleanup_oldest();
        }

        gf3d_camera_update_view();
        if (frame_count < 5) slog("Frame %d: Starting render", frame_count);
        gf3d_vgraphics_render_start();

        // TODO: get world to work
        /*
        if (world) {
            world_draw(world);
        }
        */

        if (frame_count < 5) slog("Frame %d: Drawing entities", frame_count);
        // Draw all entities
        entity_system_draw_all(lightPos, GFC_COLOR_WHITE);

        if (frame_count < 5) slog("Frame %d: Drawing UI", frame_count);
        // UI elements
        gf2d_font_draw_line_tag("ALT+F4 to exit", FT_H1, GFC_COLOR_WHITE, gfc_vector2d(10, 10));
        gf2d_font_draw_line_tag("SPACE: Make Dino", FT_H2, GFC_COLOR_GREEN, gfc_vector2d(10, 40));
        gf2d_font_draw_line_tag("DELETE: Kill Dino (TEST)", FT_H2, GFC_COLOR_RED, gfc_vector2d(10, 70));
        gf2d_font_draw_line_tag("Arrows: Rotate Dino", FT_H3, GFC_COLOR_YELLOW, gfc_vector2d(10, 100));
        
        // console view that things are rendering
        if (frame_count < 5) slog("Frame %d: Drawing mouse", frame_count);
        gf2d_mouse_draw();
        if (frame_count < 5) slog("Frame %d: Ending render", frame_count);
        gf3d_vgraphics_render_end();

        if (gfc_input_command_down("exit")) _done = 1; // exit condition
        if (frame_count < 5) slog("Frame %d: Frame delay", frame_count);
        game_frame_delay();
        frame_count++;
    }
    vkDeviceWaitIdle(gf3d_vgraphics_get_default_logical_device());
    
    //cleanup
    /*
    if (world) {
        world_free(world);
    }
    */
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
    fps = 1000.0/fmax(SDL_GetTicks() - then,0.001);
//     slog("fps: %f",fps);
}
/*eol@eof*/
