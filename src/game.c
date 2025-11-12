#include "world.h"
#include "gfc_primitives.h"
#include <string.h>
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
#include "map_debug.h"
#include "gf3d_obj_load.h"
#include "gf3d_texture.h"
#include "gf3d_particle.h"
#include "entity.h"
#include "monster.h"
#include "camera_entity.h"
#include "collision.h"
#include "world.h"
#include "powerup.h"
#include "crate.h"
#include "ice_area.h"

// Global pointer to current world for crate/powerup logic
World* g_world_ptr = NULL;

// Helper: check if two entities' bounding boxes overlap (AABB)
static inline int entity_bounds_overlap(Entity* a, Entity* b) {
    if (!a || !b) return 0;
    return gfc_box_overlap(a->bounds, b->bounds) ? 1 : 0;
}

// Helper: scale a GFC_Box in-place
void gfc_box_scale_local(GFC_Box* box, float sx, float sy, float sz) {
    if (!box) return;
    // Only scale the size, not the min corner
    box->w *= sx;
    box->h *= sy;
    box->d *= sz;
}

// Debug: log player bounding box every frame
static void log_all_entity_bounds() {
    Entity* ents = entity_system_get_all_entities();
    Uint32 max = entity_system_get_count();
    for (Uint32 i = 0; i < max; ++i) {
        if (!ents[i]._inuse) continue;
        slog("[ENTITY BOUNDS] %s pos=(%.2f %.2f %.2f) box=[%.2f %.2f %.2f %.2f %.2f %.2f]", ents[i].name, ents[i].position.x, ents[i].position.y, ents[i].position.z, ents[i].bounds.x, ents[i].bounds.y, ents[i].bounds.z, ents[i].bounds.w, ents[i].bounds.h, ents[i].bounds.d);
    }
}

// Helper: translate a GFC_Box in-place
void gfc_box_translate_local(GFC_Box* box, float tx, float ty, float tz) {
    if (!box) return;
    box->x += tx;
    box->y += ty;
    box->z += tz;
}

extern int __DEBUG;

static int _done = 0;
static int mouse_locked = 1;
static Uint32 frame_delay = 33;
static float fps = 0;
// More than simple map bounds in world space (computed from map mesh bounds and entity transform)
static float g_map_min_x = -76.0f, g_map_max_x = 76.0f;
static float g_map_min_y = -76.0f, g_map_max_y = 76.0f;
// Debug: global Z offset for floor collision
float debug_z_offset = 0.0f;
// Inward padding so clamps land safely inside walls (tunable at runtime)
static float g_map_pad = 0.0f;
// Additional percentage-based shrink applied to each axis extent
static float g_map_shrink_pct = 0.0f;
// Player grounded state (for jumping)
static int g_dino_on_ground = 0;
// Player attack spin state
int g_dino_attack_active = 0;
static int g_dino_attack_frames_left = 0;
static float g_dino_attack_spin_per_frame = 0.0f; // radians per frame
// Power slap: next 3 hits have double knockback
static int g_power_slap_hits_left = 0;
// Chain slap: next 3 attacks bounce to up to 3 nearby targets
// Chain slap: next attack bounces to up to 3 nearby targets
static int g_power_chain_attacks_left = 0;
static const int CHAIN_SLAP_BOUNCES = 3;
static const float CHAIN_SLAP_RADIUS = 32.0f;
// Thunder clap: next attack hits all targets on screen
static int g_power_thunder_ready = 0;
// Slow-motion: everything is 50% slower for 10 seconds
static int g_power_slow_active = 0;
static Uint32 g_power_slow_end_time = 0;
#define SLOW_MOTION_DURATION_MS 10000
#define SLOW_MOTION_FACTOR 0.5f
// Speed Boost: player moves faster for 10 seconds
static int g_power_speed_active = 0;
static Uint32 g_power_speed_end_time = 0;
#define SPEED_BOOST_DURATION_MS 10000
#define SPEED_BOOST_FACTOR 3.0f
// HUD: last collected power-up display
#define POWERUP_HUD_MSG_MAX 64
static char g_powerup_hud_msg[POWERUP_HUD_MSG_MAX] = "";
static Uint32 g_powerup_hud_msg_time = 0;
static const Uint32 POWERUP_HUD_MSG_DURATION = 12000; // ms

// Helper: get power-up name string
const char* powerup_type_name(PowerupType type) {
    switch (type) {
        case POWERUP_SLAP:    return "Power Slap: Double Knockback (3x)";
        case POWERUP_CHAIN:   return "Chain Slap: Bounce Hits";
        case POWERUP_THUNDER: return "Thunder Clap: Area Hit";
        case POWERUP_GOLDEN:  return "Golden Touch: 2x Bonus";
        case POWERUP_SLOW:    return "Slow Motion: 50% Slower";
        case POWERUP_SPEED:   return "Speed Boost: Faster!";
        default:              return "Unknown Power-Up";
    }
}
// transient particle effect timers
static int g_attack_burst_frames = 0;
static int g_landing_puff_frames = 0;

// Mouse wheel accumulation via SDL event watch (avoids other systems consuming events)
static int g_wheel_steps_accum = 0;
static int sdl_wheel_watch(void* userdata, SDL_Event* event)
{
    if (!event) return 1;
    if (event->type == SDL_MOUSEWHEEL) {
        g_wheel_steps_accum += event->wheel.y;
    }
    return 1; // do not filter out the event
}

// Forward declaration: compute playable bounds; prefer terrain mesh over map AABB
static void compute_map_world_bounds_from_entity(Entity *mapEntity, World *world);

void parse_arguments(int argc, char* argv[]);
void game_frame_delay();

void exitGame()
{
    _done = 1;
}

// Debug: global pointer to map mesh for jump pad debug
#include "gf3d_mesh.h"
Mesh* g_map_mesh_debug = NULL;

int main(int argc, char* argv[])
{
    //local variables
    World* world;
    Mesh* mesh;
    Texture* texture;
    Mesh* mapMesh = NULL;
    Texture* mapTexture = NULL;
    Entity* mapEntity = NULL;
    Monster* dino1 = NULL;
    Entity* camera_entity = NULL;
    GFC_Vector3D lightPos = { 8, 15, 12 };
        int prev_pageup = 0, prev_pagedown = 0;
    //initialization    
    parse_arguments(argc, argv);
    init_logger("gf3d.log", 0);
    slog("gf3d begin");
    //gfc init
    gfc_input_init("config/input.cfg");
    // Setup controls for cam rotation with arrow keys 
    gfc_input_command_add_key("panleft", "LEFT");
    gfc_input_command_add_key("panright", "RIGHT");
    // Fallback keys cause sometimes these keys do not register

    // WASD movement will work with this
    gfc_input_command_add_key("walkleft", "a");
    gfc_input_command_add_key("walkright", "d");
    gfc_input_command_add_key("walkforward", "w");
    gfc_input_command_add_key("walkback", "s");
    // exit for later
    gfc_input_command_add_key("exit", "ESCAPE");
    // Toggle fullscreen
    gfc_input_command_add_key("togglefullscreen", "RETURN");
    // DEBUG: hotkeys to adjust map bounds padding at runtime to find right bound space
    gfc_input_command_add_key("boundspadinc", "]");
    gfc_input_command_add_key("boundspaddec", "[");
    // DEBUG: hotkeys to adjust percentage shrink at runtime; see what size works best with world
    gfc_input_command_add_key("boundsshrinkinc", "9");
    gfc_input_command_add_key("boundsshrinkdec", "8");
    // Log bindings for movement and camera 
    {
        GFC_TextWord label = {0};
        if (gfc_input_get_command_key_label("walkforward", label)) slog("Bind: walkforward=%s", label);
        if (gfc_input_get_command_key_label("walkback", label))    slog("Bind: walkback=%s", label);
        if (gfc_input_get_command_key_label("walkleft", label))    slog("Bind: walkleft=%s", label);
        if (gfc_input_get_command_key_label("walkright", label))   slog("Bind: walkright=%s", label);
        if (gfc_input_get_command_key_label("panleft", label))     slog("Bind: panleft=%s", label);
        if (gfc_input_get_command_key_label("panright", label))    slog("Bind: panright=%s", label);
    }
    gfc_config_def_init();
    gfc_action_init(1024);
    //gf3d init
    gf3d_vgraphics_init("config/setup.cfg");
    gf2d_font_init("config/font.cfg");
    gf2d_actor_init(1000);
    // particles init
    gf3d_particle_manager_init(2048);

    //entity system init
    entity_system_init(60);
    monster_system_init(20); // Increase from 5 to 20 for more digis

    //game init
    srand(SDL_GetTicks());
    slog_sync();
    slog("Mouse Actor is loading");
    gf2d_mouse_load("actors/mouse.actor");
    gf2d_mouse_hide(); // do not need cursor for this game
    slog("Mouse actor loaded and hidden");
    // Lock mouse to window at start
    SDL_SetRelativeMouseMode(SDL_TRUE);
    mouse_locked = 1;
    // Capture mouse wheel events even if other systems poll SDL events
    SDL_AddEventWatch(sdl_wheel_watch, NULL);
    slog("Mouse wheel watch attached");
    
    // Skip loading separate terrain; collision will use map-based floor box
    world = NULL;
    slog("Skipping terrain load; using map floor collision box");
    
    // Load entity assets
    slog("Loading dino mesh and texture...");
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
        // We'll set dino's Z after computing map floor
    }


// Global to store the computed map floor Z
float g_map_floor_z = 0.0f;



    
    // load with temp texture for now
    // TODO: make map texture
    slog("Loading map mesh (models/map/map.obj) with TEMP texture");
    mapMesh = gf3d_mesh_load("models/map/map.obj");
    g_map_mesh_debug = mapMesh; // Set debug pointer for jump pad debug
    if (!mapMesh)
    {
        slog("Failed to load map mesh: models/map/map.obj");
    }
    else
    {
        mapTexture = gf3d_texture_load("images/default.png");
        if (!mapTexture) slog("Failed to load temp map texture, continuing with none");
        mapEntity = entity_new();
        if (mapEntity)
        {
            gfc_line_cpy(mapEntity->name, "map");
            mapEntity->mesh = mapMesh;
            mapEntity->texture = mapTexture;
            mapEntity->position = gfc_vector3d(0, 0, 0);
            mapEntity->rotation = gfc_vector3d(0, 0, 0);
            mapEntity->scale = gfc_vector3d(2.55f, 2.55f, 3.0f);
            mapEntity->color = GFC_COLOR_WHITE;
            // Set mesh bounds to match OBJ floor (Cube) extents
            mapEntity->mesh->bounds.x = -34.466248f;
            mapEntity->mesh->bounds.y = -38.582821f;
            mapEntity->mesh->bounds.z = -0.881610f;
            mapEntity->mesh->bounds.w = 68.932495f; // maxX - minX
            mapEntity->mesh->bounds.h = 78.818081f; // maxY - minY
            mapEntity->mesh->bounds.d = 0.875266f;  // maxZ - minZ
            slog("Map mesh bounds set to OBJ floor: x=[%.6f, %.6f] y=[%.6f, %.6f] z=[%.6f, %.6f]", -34.466248f, 34.466248f, -38.582821f, 40.235260f, -0.881610f, -0.006344f);

            // Compute dynamic world bounds using terrain (preferred) or map AABB
            compute_map_world_bounds_from_entity(mapEntity, world);

            // Find the most common Z value among all mesh vertices (mode) for the floor
            GFC_Box b = mapEntity->mesh->bounds;
            float floorZ = 0.0f;
            Mesh* mesh = mapEntity->mesh;
            int zBuckets = 256;
            float zMin = 1e9f, zMax = -1e9f;
            // First pass: find min/max Z
            for (Uint32 pi = 0; pi < gfc_list_get_count(mesh->primitives); ++pi) {
                MeshPrimitive* prim = gfc_list_get_nth(mesh->primitives, pi);
                if (!prim || !prim->objData) continue;
                for (Uint32 vi = 0; vi < prim->objData->face_vert_count; ++vi) {
                    float z = prim->objData->faceVertices[vi].vertex.z * mapEntity->scale.z + mapEntity->position.z;
                    if (z < zMin) zMin = z;
                    if (z > zMax) zMax = z;
                }
            }
            // Second pass: histogram to find mode
            float bucketSize = (zMax - zMin) / (float)zBuckets;
            int* hist = (int*)calloc(zBuckets, sizeof(int));
            for (Uint32 pi = 0; pi < gfc_list_get_count(mesh->primitives); ++pi) {
                MeshPrimitive* prim = gfc_list_get_nth(mesh->primitives, pi);
                if (!prim || !prim->objData) continue;
                for (Uint32 vi = 0; vi < prim->objData->face_vert_count; ++vi) {
                    float z = prim->objData->faceVertices[vi].vertex.z * mapEntity->scale.z + mapEntity->position.z;
                    int bucket = (int)((z - zMin) / bucketSize);
                    if (bucket < 0) bucket = 0;
                    if (bucket >= zBuckets) bucket = zBuckets - 1;
                    hist[bucket]++;
                }
            }
            int maxCount = 0, modeBucket = 0;
            for (int i = 0; i < zBuckets; ++i) {
                if (hist[i] > maxCount) { maxCount = hist[i]; modeBucket = i; }
            }
            free(hist);
            floorZ = zMin + modeBucket * bucketSize;
            g_map_floor_z = floorZ;
            slog("Computed map floor Z (mode): %.3f (from mesh vertices)", g_map_floor_z);


        }
        else
        {
            slog("Failed to allocate map entity");
        }
    }

    // Now that we know the map floor, spawn dino at a reasonable height above it
    if (mesh && texture) {
        // Feet-on-floor logic for dino
        float dino_mesh_min_z = mesh->bounds.z;
        float dino_scale = 0.25f;
        float dino_spawn_z = g_map_floor_z - (dino_mesh_min_z * dino_scale);
        dino1 = monster_new(mesh, texture, gfc_vector3d(0, 0, dino_spawn_z));
        if (dino1) {
            slog("Dino(player) created successfully");
            if (dino1->entity && dino1->entity->mesh) {
                dino1->entity->scale = gfc_vector3d(0.25f, 0.25f, 0.25f); // 25% scale
                dino1->entity->bounds = dino1->entity->mesh->bounds;
                // Scale to model, then increase bounds by 30% for better collision
                gfc_box_scale_local(&dino1->entity->bounds, dino1->entity->scale.x * 10.3f, dino1->entity->scale.y * 10.3f, dino1->entity->scale.z * 10.3f);
                gfc_box_translate_local(&dino1->entity->bounds, dino1->entity->position.x, dino1->entity->position.y, dino1->entity->position.z);
                slog("Dino entity scale set to 0.25, bounds increased by 30% for collision");
            }
            slog("Spawning camera entity to follow player");
            camera_entity = camera_entity_spawn(
                gfc_vector3d(0, -20, 10),
                dino1->entity
            );
            if (camera_entity) {
                slog("Camera player follower created and working");
            }
        }
    }


    // Spawn houses using new house module
    #include "house.h"
    float minX = g_map_min_x + 10.0f, maxX = g_map_max_x - 10.0f;
    float minY = g_map_min_y + 10.0f, maxY = g_map_max_y - 10.0f;
    Texture* tempTexture = gf3d_texture_load("images/default.png");
    house_spawn_all(minX, maxX, minY, maxY, g_map_floor_z, tempTexture);

    // Spawn people (guy models)
    #include "people.h"
    people_spawn_all(minX, maxX, minY, maxY, g_map_floor_z, tempTexture);

    // Spawn jump pads (medium, orange, center and random)
    #include "jump_pad.h"
    #include "powerup.h"
    slog("[JUMP PAD] minX=%.2f maxX=%.2f minY=%.2f maxY=%.2f floorZ=%.2f", minX, maxX, minY, maxY, g_map_floor_z);
    Texture* jumpPadTexture = gf3d_texture_load("images/jump_pad.png");
    Texture* powerupTexture = gf3d_texture_load("images/default.png");

    // Spawn up to 6 random crates in the world at a time
    #define MAX_CRATES 6
    GFC_Vector3D crate_scale = gfc_vector3d(1.2f, 1.2f, 1.2f);
    GFC_Color crate_brown = gfc_color(0.55f, 0.27f, 0.07f, 1.0f);
    int crate_count = 0;
    Entity* ents = entity_system_get_all_entities();
    Uint32 ent_max = entity_system_get_count();
    for (Uint32 i = 0; i < ent_max; ++i) {
        if (!ents[i]._inuse) continue;
        if (strncmp(ents[i].name, "crate", 5) == 0) crate_count++;
    }
    int crates_to_spawn = MAX_CRATES - crate_count;
    for (int i = 0; i < crates_to_spawn; ++i) {
        float rx = minX + ((float)rand() / RAND_MAX) * (maxX - minX);
        float ry = minY + ((float)rand() / RAND_MAX) * (maxY - minY);
        GFC_Vector3D crate_pos = gfc_vector3d(rx, ry, 0);
        // Use entity_get_floor_position to find the correct floor Z at the crate's (x, y)
        GFC_Vector3D floorContact = {0};
        Entity tempCrateEntity = {0};
        tempCrateEntity.position = crate_pos;
        tempCrateEntity.scale = crate_scale;
        float crate_z = 0.0f;
        if (entity_get_floor_position(&tempCrateEntity, world, &floorContact)) {
            crate_z = floorContact.z + 1.0f;
        } else {
            crate_z = g_map_floor_z + 1.0f;
            if (crate_z < 1.0f) crate_z = 1.0f;
        }
        crate_pos.z = crate_z;
        crate_spawn(crate_pos, crate_scale, crate_brown);
    }

    // Track game start time for HUD timer
    Uint32 game_start_ticks = SDL_GetTicks();
    slog("Entering main game loop");
    int frame_count = 0;
    // Set global world pointer for crate/powerup logic
    g_world_ptr = world;
    while (!_done)
    {
        // Spawn jump pads at interval (logic inside function)
        jump_pad_spawn_all(minX, maxX, minY, maxY, g_map_floor_z, jumpPadTexture);
        // Only log player and power-up bounds if they are close
        Entity* ents_dbg = entity_system_get_all_entities();
        Uint32 ents_dbg_count = entity_system_get_count();
        for (Uint32 dbg_i = 0; dbg_i < ents_dbg_count; ++dbg_i) {
            if (!ents_dbg[dbg_i]._inuse) continue;
            if (strncmp(ents_dbg[dbg_i].name, "monster", 7) == 0) {
                for (Uint32 dbg_j = 0; dbg_j < ents_dbg_count; ++dbg_j) {
                    if (!ents_dbg[dbg_j]._inuse) continue;
                    if (strncmp(ents_dbg[dbg_j].name, "powerup", 7) == 0) {
                        float dx = ents_dbg[dbg_i].position.x - ents_dbg[dbg_j].position.x;
                        float dy = ents_dbg[dbg_i].position.y - ents_dbg[dbg_j].position.y;
                        float dz = ents_dbg[dbg_i].position.z - ents_dbg[dbg_j].position.z;
                        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                        if (dist < 5.0f) {
                            slog("[DEBUG CLOSE] Player bounds: [%.2f %.2f %.2f %.2f %.2f %.2f] pos=(%.2f %.2f %.2f)",
                                ents_dbg[dbg_i].bounds.x, ents_dbg[dbg_i].bounds.y, ents_dbg[dbg_i].bounds.z, ents_dbg[dbg_i].bounds.w, ents_dbg[dbg_i].bounds.h, ents_dbg[dbg_i].bounds.d,
                                ents_dbg[dbg_i].position.x, ents_dbg[dbg_i].position.y, ents_dbg[dbg_i].position.z);
                            slog("[DEBUG CLOSE] Powerup bounds: [%.2f %.2f %.2f %.2f %.2f %.2f] pos=(%.2f %.2f %.2f)",
                                ents_dbg[dbg_j].bounds.x, ents_dbg[dbg_j].bounds.y, ents_dbg[dbg_j].bounds.z, ents_dbg[dbg_j].bounds.w, ents_dbg[dbg_j].bounds.h, ents_dbg[dbg_j].bounds.d,
                                ents_dbg[dbg_j].position.x, ents_dbg[dbg_j].position.y, ents_dbg[dbg_j].position.z);
                        }
                    }
                }
            }
        }
        // Constantly check if things are being sent though input
        if (frame_count < 5) slog("Frame %d: Starting input update", frame_count);
        gfc_input_update();
        if (frame_count < 5) slog("Frame %d: Starting mouse update", frame_count);
        gf2d_mouse_update(); 
        if (frame_count < 5) slog("Frame %d: Starting font update", frame_count);
        gf2d_font_update();

        // Read and reset accumulated mouse wheel steps (captured by event watch)
        int wheel_steps = g_wheel_steps_accum;
        g_wheel_steps_accum = 0;

        // log to make sure inputs are being pressed
        Uint8 key_w = gfc_input_key_down("w");
        Uint8 key_a = gfc_input_key_down("a");
        Uint8 key_s = gfc_input_key_down("s");
        Uint8 key_d = gfc_input_key_down("d");
        // Debug: number keys 1-6 spawn each power-up at player position
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        if (dino1 && dino1->entity) {
            // Debug key cooldowns (per key)
            static Uint32 powerup_key_last_time[6] = {0};
            Uint32 now = SDL_GetTicks();

            float px = dino1->entity->position.x;
            float py = dino1->entity->position.y;
            float pz = dino1->entity->position.z;
            float facing = dino1->entity->rotation.z;
            float spawn_dist = 6.0f;
            float fx = -sinf(facing);
            float fy =  cosf(facing);
            float spawn_x = px + fx * spawn_dist;
            float spawn_y = py + fy * spawn_dist;
            Texture* powerupTexture = gf3d_texture_load("images/default.png");

            // Count active power-ups
            int powerup_count = 0;
            Entity* ents = entity_system_get_all_entities();
            Uint32 ent_max = entity_system_get_count();
            for (Uint32 i = 0; i < ent_max; ++i) {
                if (!ents[i]._inuse) continue;
                if (strncmp(ents[i].name, "powerup", 7) == 0) powerup_count++;
            }

            // Only allow up to 3 power-ups at a time
            if (powerup_count < 3) {
                // 1-second cooldown for each debug key
                if (keys[SDL_SCANCODE_1]) {
                    slog("[DEBUG] 1 key pressed. dino1=%p dino1->entity=%p powerup_count=%d now=%u last=%u", dino1, dino1 ? dino1->entity : NULL, powerup_count, now, powerup_key_last_time[0]);
                    if (now - powerup_key_last_time[0] > 1000) {
                        slog("[DEBUG] Spawning POWERUP_SLAP at player position");
                        powerup_spawn_near_player(dino1->entity, g_map_floor_z, powerupTexture, POWERUP_SLAP);
                        powerup_key_last_time[0] = now;
                    }
                }
                if (keys[SDL_SCANCODE_2]) {
                    if (now - powerup_key_last_time[1] > 1000) {
                        slog("[DEBUG] Spawning POWERUP_CHAIN at player position");
                        powerup_spawn_near_player(dino1->entity, g_map_floor_z, powerupTexture, POWERUP_CHAIN);
                        powerup_key_last_time[1] = now;
                    }
                }
                if (keys[SDL_SCANCODE_3]) {
                    if (now - powerup_key_last_time[2] > 1000) {
                        slog("[DEBUG] Spawning POWERUP_THUNDER at player position");
                        powerup_spawn_near_player(dino1->entity, g_map_floor_z, powerupTexture, POWERUP_THUNDER);
                        powerup_key_last_time[2] = now;
                    }
                }
                if (keys[SDL_SCANCODE_4]) {
                    if (now - powerup_key_last_time[3] > 1000) {
                        slog("[DEBUG] Spawning POWERUP_SLOW at player position");
                        powerup_spawn_near_player(dino1->entity, g_map_floor_z, powerupTexture, POWERUP_SLOW);
                        powerup_key_last_time[3] = now;
                    }
                }
            }
        }
        Uint8 cmd_wf = gfc_input_command_down("walkforward");
        Uint8 cmd_wb = gfc_input_command_down("walkback");
        Uint8 cmd_wl = gfc_input_command_down("walkleft");
        Uint8 cmd_wr = gfc_input_command_down("walkright");
        Uint8 cmd_pl = gfc_input_command_down("panleft");
        Uint8 cmd_pr = gfc_input_command_down("panright");
        int mouse_lmb = gf2d_mouse_button_state(0); // Use state instead of pressed since mouse is hidden
        int player_input_active = (key_w || key_a || key_s || key_d || cmd_wf || cmd_wb || cmd_wl || cmd_wr);
        if (key_w || key_a || key_s || key_d || cmd_wf || cmd_wb || cmd_wl || cmd_wr || cmd_pl || cmd_pr || mouse_lmb)
        {
            slog("Input active: keys[w=%i a=%i s=%i d=%i] cmds[wf=%i wb=%i wl=%i wr=%i pl=%i pr=%i] mouse[lmb=%i]",
                 key_w,key_a,key_s,key_d, cmd_wf,cmd_wb,cmd_wl,cmd_wr, cmd_pl,cmd_pr, mouse_lmb);
        }
            // Debug: Levitate player up/down with Page Up/Page Down keys (persistent offset, only on key press)
            const Uint8* keystate = SDL_GetKeyboardState(NULL);
            float z_step = 0.05f;
            int cur_pageup = keystate[SDL_SCANCODE_PAGEUP];
            int cur_pagedown = keystate[SDL_SCANCODE_PAGEDOWN];
            if (cur_pageup && !prev_pageup) debug_z_offset += z_step;
            if (cur_pagedown && !prev_pagedown) debug_z_offset -= z_step;
            prev_pageup = cur_pageup;
            prev_pagedown = cur_pagedown;
            // No longer apply offset to player Z directly; now applied in entity_get_floor_position
            if ((cur_pageup || cur_pagedown) && dino1 && dino1->entity) {
                slog("DEBUG: Floor Z offset: %.3f", debug_z_offset);
            }
        
        // Update all thinking, entity
        entity_system_think_all();
        entity_system_update_all();
        
        // Player movement: WASD moves the dino on the ground plane (XY), Z controlled by gravity/jump with floor collision
        if (dino1 && dino1->entity)
        {
            float moveSpeed = 0.35f; // units/frame
            if (g_power_speed_active && SDL_GetTicks() < g_power_speed_end_time) {
                moveSpeed *= SPEED_BOOST_FACTOR;
            } else if (g_power_speed_active) {
                g_power_speed_active = 0;
            }

            // Power-up pickup logic (Speed Boost)
            Entity* ents = entity_system_get_all_entities();
            Uint32 ent_max = entity_system_get_count();
            for (Uint32 i = 0; i < ent_max; ++i) {
                if (!ents[i]._inuse) continue;
                if (strncmp(ents[i].name, "powerup", 7) == 0) {
                    PowerupData* pdata = (PowerupData*)ents[i].data;
                    if (!pdata) continue;
                    if (pdata->type == POWERUP_SPEED && entity_bounds_overlap(dino1->entity, &ents[i])) {
                        g_power_speed_active = 1;
                        g_power_speed_end_time = SDL_GetTicks() + SPEED_BOOST_DURATION_MS;
                        snprintf(g_powerup_hud_msg, POWERUP_HUD_MSG_MAX, "Speed Boost! (10s)");
                        g_powerup_hud_msg_time = SDL_GetTicks();
                        ents[i]._inuse = 0; // Remove power-up
                        slog("[POWERUP] Picked up SPEED BOOST");
                    }
                }
            }

            // Debug: number keys 1-6 spawn each power-up at player position
            // Make sure now and powerup_key_last_time are in scope
            static Uint32 powerup_key_last_time[6] = {0};
            Uint32 now = SDL_GetTicks();
            if (keys[SDL_SCANCODE_5]) {
                if (now - powerup_key_last_time[4] > 1000) {
                    slog("[DEBUG] Spawning POWERUP_SPEED at player position");
                    powerup_spawn_near_player(dino1->entity, g_map_floor_z, powerupTexture, POWERUP_SPEED);
                    powerup_key_last_time[4] = now;
                }
            }
            const float FOOT_OFFSET = 0.2f; // small lift to avoid z-fighting
            // TODO: get collision flooring to work with world map
            const float GRAVITY = -0.23f;    // units/frame^2 (increase for faster fall)
            const float JUMP_IMPULSE = 4.13f; // reduced from 8.5 for lower jump height

            GFC_Vector3D p = dino1->entity->position;
            GFC_Vector3D prev = p;
            int was_on_ground = g_dino_on_ground;
            // Camera-relative movement on the XY plane
            int in_left  = (gfc_input_command_down("walkleft")    || gfc_input_key_down("a")) ? 1 : 0;
            int in_right = (gfc_input_command_down("walkright")   || gfc_input_key_down("d")) ? 1 : 0;
            int in_fwd   = (gfc_input_command_down("walkforward") || gfc_input_key_down("w")) ? 1 : 0;
            int in_back  = (gfc_input_command_down("walkback")    || gfc_input_key_down("s")) ? 1 : 0;

            int axis_right = in_right - in_left; // +1 right, -1 left
            int axis_fwd   = in_fwd   - in_back; // +1 forward, -1 back

            if (axis_right || axis_fwd) {
                Entity* cam_e = camera_entity_get();
                float cam_a = cam_e ? camera_entity_get_angle(cam_e) : 0.0f;
                // Build camera-space basis on ground plane
                float fx = -sinf(cam_a); // forward X
                float fy =  cosf(cam_a); // forward Y
                float rx =  cosf(cam_a); // right X
                float ry =  sinf(cam_a); // right Y
                float dx = (rx * axis_right + fx * axis_fwd) * moveSpeed;
                float dy = (ry * axis_right + fy * axis_fwd) * moveSpeed;
                p.x += dx;
                p.y += dy;
            }
            
            // Attack trigger: left mouse click starts a quick spin attack
            // Use state check + manual edge detection since mouse is hidden (gf2d_mouse_button_pressed returns 0 when hidden)
            static int lmb_last_state = 0;
            int lmb_state = gf2d_mouse_button_state(0);
            int lmb_pressed = (lmb_state && !lmb_last_state);
            lmb_last_state = lmb_state;
            
            if (lmb_pressed) slog("LMB pressed detected! attack_active=%d", g_dino_attack_active);
            if (!g_dino_attack_active && lmb_pressed)
            {
                g_dino_attack_active = 1;
                g_dino_attack_frames_left = 12; // quick spin over ~12 frames
                g_dino_attack_spin_per_frame = (float)(2.0 * GFC_PI) / (float)g_dino_attack_frames_left; // one full rotation
                slog("Attack: spin start (%d frames, %.3f rad/frame)", g_dino_attack_frames_left, g_dino_attack_spin_per_frame);
                g_attack_burst_frames = 1; // trigger a one-frame burst in render
            }

            // Floor detection via downward ray against world + map geometry
            GFC_Vector3D floorContact = {0};
            float floorZ = 0.0f;
            if (entity_get_floor_position(dino1->entity, world, &floorContact)) {
                floorZ = floorContact.z + FOOT_OFFSET;
            } else {
                // Fallback to plane at z=0
                floorZ = 0.0f + FOOT_OFFSET;
            }

            // jumping
            if ((gfc_input_command_pressed("jump") || gfc_input_key_pressed("SPACE")) && g_dino_on_ground)
            {
                dino1->entity->velocity.z = JUMP_IMPULSE;
                g_dino_on_ground = 0;
                slog("Jump! vz=%.2f", dino1->entity->velocity.z);
            }

            // Apply gravity & vertical movement
            dino1->entity->velocity.z += GRAVITY;
            p.z += dino1->entity->velocity.z;

            // Floor collision clamp
            if (p.z < floorZ)
            {
                p.z = floorZ;
                if (dino1->entity->velocity.z < 0) dino1->entity->velocity.z = 0;
                if (!g_dino_on_ground) slog("Landed at z=%.2f (floorZ=%.2f)", p.z, floorZ);
                g_dino_on_ground = 1;
                if (!was_on_ground) { g_landing_puff_frames = 6; }
            }
            else
            {
                g_dino_on_ground = 0;
            }

            dino1->entity->position = p;
            // Spin attack animation: overrides facing rotation while active
            if (g_dino_attack_active && g_dino_attack_frames_left > 0)
            {
                dino1->entity->rotation.z += g_dino_attack_spin_per_frame;
                g_dino_attack_frames_left--;
                if (g_dino_attack_frames_left <= 0)
                {
                    g_dino_attack_active = 0;
                    slog("Attack: spin end");
                }
            }
            // Face the direction of movement when not attacking; rotate around Z to match planar movement
            float dx = p.x - prev.x;
            float dy = p.y - prev.y;
            if (!g_dino_attack_active && ((dx != 0.0f) || (dy != 0.0f)))
            {
                // invert Y to fix front/back movement without affecting left/right
                float angle = atan2f(dx, -dy);
                dino1->entity->rotation.z = angle;
                slog("Dino facing: rot.z=%.3f rad", angle);
            }
            if ((p.x != prev.x) || (p.y != prev.y) || (p.z != prev.z))
            {
                slog("Dino moved: pos=(%.2f, %.2f, %.2f)", p.x, p.y, p.z);
            }
        }

        // Handle camera rotation: hold RMB and move mouse to rotate
        Entity* cam_ent = camera_entity_get();
        if (cam_ent) {
            static int rmb_last_state = 0;
            static int relative_mode_enabled = 0;
            // Use SDL button mask directly (SDL_BUTTON_RIGHT), gf2d maps don't align for RMB
            int rmb_state = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;

            // Toggle SDL relative mouse mode on RMB press/release for unconstrained rotation
            if (rmb_state && !rmb_last_state) {
                if (!relative_mode_enabled) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    relative_mode_enabled = 1;
                    slog("Camera: RMB engage, relative mouse ON");
                }
            } else if (!rmb_state && rmb_last_state) {
                if (relative_mode_enabled) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    relative_mode_enabled = 0;
                    slog("Camera: RMB release, relative mouse OFF");
                }
            }

            if (rmb_state) {
                // RMB held: rotate camera based on horizontal mouse relative movement
                int relx = 0, rely = 0;
                SDL_GetRelativeMouseState(&relx, &rely);
                if (relx != 0) {
                    float rotation_speed = 0.005f; // sensitivity tuning
                    float delta = -(float)relx * rotation_speed;
                    camera_entity_adjust_angle(cam_ent, delta);
                    slog("Camera: rotate dx=%d (angle adjust=%.4f) rmb=%d", relx, delta, rmb_state);
                }
                if (rely != 0) {
                    // Adjust camera height with vertical mouse while RMB held
                    float height_speed = 0.02f; // sensitivity for height adjust
                    float hdelta = -(float)rely * height_speed; // mouse up => increase height
                    camera_entity_adjust_height(cam_ent, hdelta);
                    slog("Camera: height dy=%d (delta=%.3f)", rely, hdelta);
                }
            }

            rmb_last_state = rmb_state;

            // Fallback: arrow keys still work for camera rotation
            if (gfc_input_command_down("panleft")) {
                camera_entity_adjust_angle(cam_ent, 0.02f);
                slog("Camera: panleft (arrow key)");
            }
            if (gfc_input_command_down("panright")) {
                camera_entity_adjust_angle(cam_ent, -0.02f);
                slog("Camera: panright (arrow key)");
            }
            // Mouse wheel zoom
            if (wheel_steps != 0) {
                float wheel_zoom_speed = 1.2f; // distance units per wheel step
                float d = -(float)wheel_steps * wheel_zoom_speed; // up => zoom in (reduce distance)
                camera_entity_adjust_distance(cam_ent, d);
                slog("Camera: wheel zoom steps=%d (delta=%.3f)", wheel_steps, d);
            }
            // No arrow up/down height fallback per current controls preference

            // Safety: ensure relative mode is disabled if exiting while held
            if (_done && relative_mode_enabled) {
                SDL_SetRelativeMouseMode(SDL_FALSE);
                relative_mode_enabled = 0;
            }
        }

        // Check for entity collisions
        Entity* entityList = entity_system_get_all_entities();
        if (entityList) {
            for (int i = 0; i < entity_system_get_count(); i++) {
                if (!entityList[i]._inuse) continue;
                
                // Check collision with other entities, including map (floor) for all
                for (int j = i + 1; j < entity_system_get_count(); j++) {
                    if (!entityList[j]._inuse) continue;
                    if (collision_entity_intersect(&entityList[i], &entityList[j])) {
                        slog("[COLLISION] i=%d (%s) <-> j=%d (%s)", i, entityList[i].name, j, entityList[j].name);
                        // Identify jump pad entities
                        int i_is_jump_pad = (strncmp(entityList[i].name, "jump_pad", 8) == 0);
                        int j_is_jump_pad = (strncmp(entityList[j].name, "jump_pad", 8) == 0);
                        int i_is_player = (strncmp(entityList[i].name, "monster", 7) == 0);
                        int j_is_player = (strncmp(entityList[j].name, "monster", 7) == 0);
                        int i_is_map = (strncmp(entityList[i].name, "map", 3) == 0);
                        int j_is_map = (strncmp(entityList[j].name, "map", 3) == 0);
                        int i_is_target = (strncmp(entityList[i].name, "house", 5) == 0 || strncmp(entityList[i].name, "guy", 3) == 0);
                        int j_is_target = (strncmp(entityList[j].name, "house", 5) == 0 || strncmp(entityList[j].name, "guy", 3) == 0);
                        int i_is_powerup = (strncmp(entityList[i].name, "powerup", 7) == 0);
                        int j_is_powerup = (strncmp(entityList[j].name, "powerup", 7) == 0);

                        // Jump pad launch effect: player collides with jump pad
                        // Allow all entities except map, powerup, and jump pad itself to bounce off jump pads
                        if (i_is_jump_pad && !(j_is_map || j_is_powerup || j_is_jump_pad)) {
                            entityList[j].velocity.z = 8.0f; // Launch entity upward
                            continue;
                        } else if (j_is_jump_pad && !(i_is_map || i_is_powerup || i_is_jump_pad)) {
                            entityList[i].velocity.z = 8.0f; // Launch entity upward
                            continue;
                        }

                        // Power-up pickup: player collides with power-up
                        if (i_is_player && j_is_powerup && entityList[j]._inuse) {
                            // [POWERUP] Player picked up power-up (i, j)
                            PowerupData* pdata = (PowerupData*)entityList[j].data;
                            slog("[POWERUP PICKUP DEBUG] Player entity %d (%s) overlapped powerup entity %d (type %d) bounds: player [%.2f %.2f %.2f %.2f %.2f %.2f], powerup [%.2f %.2f %.2f %.2f %.2f %.2f]", i, entityList[i].name, j, pdata ? pdata->type : -1, entityList[i].bounds.x, entityList[i].bounds.y, entityList[i].bounds.z, entityList[i].bounds.w, entityList[i].bounds.h, entityList[i].bounds.d, entityList[j].bounds.x, entityList[j].bounds.y, entityList[j].bounds.z, entityList[j].bounds.w, entityList[j].bounds.h, entityList[j].bounds.d);
                            if (pdata) {
                                if (pdata->type == POWERUP_SLAP) {
                                    g_power_slap_hits_left = 3;
                                    slog("[POWERUP] Power Slap activated: next 3 hits double knockback");
                                } else if (pdata->type == POWERUP_CHAIN) {
                                    g_power_chain_attacks_left = 1;
                                    slog("[POWERUP] Chain Slap activated: next attack bounces to 3 targets");
                                } else if (pdata->type == POWERUP_THUNDER) {
                                    g_power_thunder_ready = 1;
                                    slog("[POWERUP] Thunder Clap activated: next attack hits all targets!");
                                } else if (pdata->type == POWERUP_SLOW) {
                                    g_power_slow_active = 1;
                                    g_power_slow_end_time = SDL_GetTicks() + SLOW_MOTION_DURATION_MS;
                                    slog("[POWERUP] Slow Motion activated: everything is 50%% slower for 10 seconds");
                                }
                                snprintf(g_powerup_hud_msg, POWERUP_HUD_MSG_MAX, "%s", powerup_type_name(pdata->type));
                                g_powerup_hud_msg_time = SDL_GetTicks();
                            } else {
                                slog("[POWERUP PICKUP DEBUG] Powerup data is NULL (should not happen)");
                            }
                            entityList[j]._inuse = 0;
                            slog("[POWERUP] Player picked up power-up type %d (entity j)", pdata ? pdata->type : -1);
                            continue;
                        } else if (j_is_player && i_is_powerup && entityList[i]._inuse) {
                            // [POWERUP] Player picked up power-up (j, i)
                            PowerupData* pdata = (PowerupData*)entityList[i].data;
                            if (pdata) {
                                if (pdata->type == POWERUP_SLAP) {
                                    g_power_slap_hits_left = 3;
                                    slog("[POWERUP] Power Slap activated: next 3 hits double knockback");
                                } else if (pdata->type == POWERUP_CHAIN) {
                                    g_power_chain_attacks_left = 1;
                                    slog("[POWERUP] Chain Slap activated: next attack bounces to 3 targets");
                                } else if (pdata->type == POWERUP_THUNDER) {
                                    g_power_thunder_ready = 1;
                                    slog("[POWERUP] Thunder Clap activated: next attack hits all targets!");
                                } else if (pdata->type == POWERUP_SLOW) {
                                    g_power_slow_active = 1;
                                    g_power_slow_end_time = SDL_GetTicks() + SLOW_MOTION_DURATION_MS;
                                    slog("[POWERUP] Slow Motion activated: everything is 50%% slower for 10 seconds");
                                }
                                snprintf(g_powerup_hud_msg, POWERUP_HUD_MSG_MAX, "%s", powerup_type_name(pdata->type));
                                g_powerup_hud_msg_time = SDL_GetTicks();
                            } else {
                                // Power-up data is NULL (should not happen)
                            }
                            entityList[i]._inuse = 0;
                            slog("[POWERUP] Player picked up power-up type %d (entity i)", pdata ? pdata->type : -1);
                            continue;
                        }

                        // Prevent push/attack logic for jump pads
                        if (i_is_jump_pad || j_is_jump_pad) {
                            continue;
                        }
                        // Prevent push/attack logic for power-ups (player should go through)
                        if (i_is_powerup || j_is_powerup) {
                            continue;
                        }

                        // Only push if at least one is not the map (so trees, player, houses, etc. get pushed by the floor)
                        if (i_is_map && j_is_map) continue;

                        GFC_Vector3D diff = gfc_vector3d_subbed(entityList[i].position, entityList[j].position);
                        diff.z = 0.0f; // do not push up/down
                        if (diff.x != 0.0f || diff.y != 0.0f) {
                            gfc_vector3d_normalize(&diff);
                        }

                        // Player attack: if player is attacking and collides with house/person/crate, apply effects
                        float attack_impulse = 2.5f;
                        float actual_attack_impulse = attack_impulse;
                        // Crate break logic: break crate if player attacks and collides
                        int i_is_crate = (strncmp(entityList[i].name, "crate", 5) == 0);
                        int j_is_crate = (strncmp(entityList[j].name, "crate", 5) == 0);
                        if (g_dino_attack_active) {
                            // Break crate if player collides with it; let crate.c handle powerup spawn
                            if (i_is_player && j_is_crate && entityList[j]._inuse) {
                                if (entityList[j].data) ((int*)entityList[j].data)[0] = 1; // is_broken = 1
                                continue;
                            } else if (j_is_player && i_is_crate && entityList[i]._inuse) {
                                if (entityList[i].data) ((int*)entityList[i].data)[0] = 1;
                                continue;
                            }
                                                        int did_thunder = 0;
                                                        if (g_power_thunder_ready > 0) {
                                                            // Thunder Clap: scatter all targets (houses/people) in random directions
                                                            int player_idx = -1;
                                                            if (i_is_player && j_is_target) player_idx = i;
                                                            else if (j_is_player && i_is_target) player_idx = j;
                                                            if (player_idx >= 0) {
                                                                int hits = 0;
                                                                for (int k = 0; k < entity_system_get_count(); ++k) {
                                                                    if (!entityList[k]._inuse) continue;
                                                                    if (!(strncmp(entityList[k].name, "house", 5) == 0 || strncmp(entityList[k].name, "guy", 3) == 0)) continue;
                                                                    // Scatter: random direction
                                                                    float angle = ((float)rand() / (float)RAND_MAX) * 2.0f * (float)GFC_PI;
                                                                    float nx = cosf(angle);
                                                                    float ny = sinf(angle);
                                                                    float thunder_impulse = actual_attack_impulse * 5.0f;
                                                                    entityList[k].velocity.x += nx * thunder_impulse;
                                                                    entityList[k].velocity.y += ny * thunder_impulse;
                                                                    hits++;
                                                                }
                                                                slog("[POWERUP] Thunder Clap: scattered %d targets!", hits);
                                                                g_power_thunder_ready = 0;
                                                                did_thunder = 1;
                                                            }
                                                        }
                            if (g_power_slap_hits_left > 0) {
                                actual_attack_impulse *= 2.0f;
                                g_power_slap_hits_left--;
                                if (g_power_slap_hits_left == 0) {
                                    slog("[POWERUP] Power Slap effect expired");
                                }
                            }
                            int did_chain = 0;
                            if (g_power_chain_attacks_left > 0) {
                                                            if (did_thunder) continue;
                                // Find up to 3 nearby targets (houses/people) not already hit
                                int main_target = -1;
                                int player_idx = -1;
                                if (i_is_player && j_is_target) { main_target = j; player_idx = i; }
                                else if (j_is_player && i_is_target) { main_target = i; player_idx = j; }
                                if (main_target >= 0 && player_idx >= 0) {
                                    // Apply normal knockback to main target
                                    entityList[main_target].velocity.x += (main_target == j ? -diff.x : diff.x) * actual_attack_impulse;
                                    entityList[main_target].velocity.y += (main_target == j ? -diff.y : diff.y) * actual_attack_impulse;
                                    // Chain to up to 3 nearby targets
                                    int bounces = 0;
                                    for (int k = 0; k < entity_system_get_count(); ++k) {
                                        if (k == main_target || k == player_idx) continue;
                                        if (!entityList[k]._inuse) continue;
                                        if (!(strncmp(entityList[k].name, "house", 5) == 0 || strncmp(entityList[k].name, "guy", 3) == 0)) continue;
                                        float dx = entityList[k].position.x - entityList[main_target].position.x;
                                        float dy = entityList[k].position.y - entityList[main_target].position.y;
                                        float dist2 = dx*dx + dy*dy;
                                        if (dist2 > CHAIN_SLAP_RADIUS*CHAIN_SLAP_RADIUS) continue;
                                        // Apply knockback away from main target
                                        float dist = sqrtf(dist2);
                                        float nx = (dist > 0.01f) ? dx/dist : 1.0f;
                                        float ny = (dist > 0.01f) ? dy/dist : 0.0f;
                                        float chain_impulse = actual_attack_impulse * 0.7f;
                                        entityList[k].velocity.x += nx * chain_impulse;
                                        entityList[k].velocity.y += ny * chain_impulse;
                                        bounces++;
                                        if (bounces >= CHAIN_SLAP_BOUNCES) break;
                                    }
                                    slog("[POWERUP] Chain Slap: bounced to %d targets", bounces);
                                    g_power_chain_attacks_left--;
                                    if (g_power_chain_attacks_left == 0) {
                                        slog("[POWERUP] Chain Slap effect expired");
                                    }
                                    did_chain = 1;
                                }
                            }
                            if (!did_chain) {
                                if (i_is_player && j_is_target) {
                                    entityList[j].velocity.x -= diff.x * actual_attack_impulse;
                                    entityList[j].velocity.y -= diff.y * actual_attack_impulse;
                                } else if (j_is_player && i_is_target) {
                                    entityList[i].velocity.x += diff.x * actual_attack_impulse;
                                    entityList[i].velocity.y += diff.y * actual_attack_impulse;
                                }
                            }
                        }

                        // Inelastic collision: average velocities (if neither is map)
                        if (!i_is_map && !j_is_map) {
                            float avg_vx = 0.5f * (entityList[i].velocity.x + entityList[j].velocity.x);
                            float avg_vy = 0.5f * (entityList[i].velocity.y + entityList[j].velocity.y);
                            float avg_vz = 0.5f * (entityList[i].velocity.z + entityList[j].velocity.z);
                            entityList[i].velocity.x = avg_vx;
                            entityList[i].velocity.y = avg_vy;
                            entityList[i].velocity.z = avg_vz;
                            entityList[j].velocity.x = avg_vx;
                            entityList[j].velocity.y = avg_vy;
                            entityList[j].velocity.z = avg_vz;
                        }

                        // If one is the map, apply a smaller lateral nudge; no vertical change
                        float pushScale = (i_is_map || j_is_map) ? 0.5f : 1.0f;
                        GFC_Vector3D push; gfc_vector3d_scale(push, diff, pushScale);
                        entityList[i].position = gfc_vector3d_added(entityList[i].position, push);
                        entityList[j].position = gfc_vector3d_subbed(entityList[j].position, push);
                    }
                }
                
                // Clamp to dynamic world bounds with a small player radius so we don't clip into walls
                float ox = entityList[i].position.x;
                float oy = entityList[i].position.y;
                // Clamp so the player's edge never crosses the wall (center always inside)
                const float PLAYER_RADIUS = 4.0f;
                // Per-side wall thickness offsets (tune as needed)
                const float WALL_LEFT_OFFSET   = 5.5f;
                const float WALL_RIGHT_OFFSET  = 5.5f;
                const float WALL_BOTTOM_OFFSET = 5.5f;
                const float WALL_TOP_OFFSET    = 5.5f;
                float effMinX = g_map_min_x + PLAYER_RADIUS + WALL_LEFT_OFFSET;
                float effMaxX = g_map_max_x - PLAYER_RADIUS - WALL_RIGHT_OFFSET;
                float effMinY = g_map_min_y + PLAYER_RADIUS + WALL_BOTTOM_OFFSET;
                float effMaxY = g_map_max_y - PLAYER_RADIUS - WALL_TOP_OFFSET;
                // Clamp after all movement, so player never visually reaches the wall
                int hit_wall = 0;
                if (entityList[i].position.x < effMinX) {
                    entityList[i].position.x = effMinX;
                    entityList[i].velocity.x = 0;
                    hit_wall = 1;
                    slog("WALL COLLISION: ent='%s' hit XMIN (%.2f)", entityList[i].name, effMinX);
                }
                if (entityList[i].position.x > effMaxX) {
                    entityList[i].position.x = effMaxX;
                    entityList[i].velocity.x = 0;
                    hit_wall = 1;
                    slog("WALL COLLISION: ent='%s' hit XMAX (%.2f)", entityList[i].name, effMaxX);
                }
                if (entityList[i].position.y < effMinY) {
                    entityList[i].position.y = effMinY;
                    entityList[i].velocity.y = 0;
                    hit_wall = 1;
                    slog("WALL COLLISION: ent='%s' hit YMIN (%.2f)", entityList[i].name, effMinY);
                }
                if (entityList[i].position.y > effMaxY) {
                    entityList[i].position.y = effMaxY;
                    entityList[i].velocity.y = 0;
                    hit_wall = 1;
                    slog("WALL COLLISION: ent='%s' hit YMAX (%.2f)", entityList[i].name, effMaxY);
                }
            }
        }

        // Runtime tuning: adjust padding and recompute bounds; logs are emitted on change
        if (gfc_input_command_pressed("boundspadinc") || gfc_input_key_pressed("]")) {
            g_map_pad += 0.5f;
            if (mapEntity) compute_map_world_bounds_from_entity(mapEntity, world);
        }
        if (gfc_input_command_pressed("boundspaddec") || gfc_input_key_pressed("[")) {
            g_map_pad -= 0.5f; if (g_map_pad < 0.0f) g_map_pad = 0.0f;
            if (mapEntity) compute_map_world_bounds_from_entity(mapEntity, world);
        }
        if (gfc_input_command_pressed("boundsshrinkinc") || gfc_input_key_pressed("9")) {
            g_map_shrink_pct += 0.01f; if (g_map_shrink_pct > 0.25f) g_map_shrink_pct = 0.25f;
            if (mapEntity) compute_map_world_bounds_from_entity(mapEntity, world);
        }
        if (gfc_input_command_pressed("boundsshrinkdec") || gfc_input_key_pressed("8")) {
            g_map_shrink_pct -= 0.01f; if (g_map_shrink_pct < 0.0f) g_map_shrink_pct = 0.0f;
            if (mapEntity) compute_map_world_bounds_from_entity(mapEntity, world);
        }

        gf3d_camera_update_view();
        if (frame_count < 5) slog("Frame %d: Starting render", frame_count);
        gf3d_vgraphics_render_start();
        // reset particle pipes at start of frame's render
        gf3d_particle_reset_pipes();

        // Draw terrain to visualize floor coverage
        // Terrain draw disabled (no terrain)

        if (frame_count < 5) slog("Frame %d: Drawing entities", frame_count);
        // Draw all entities
        entity_system_draw_all(lightPos, GFC_COLOR_WHITE);

        // Always-on debug: draw all entity bounding boxes as transparent cubes
        Entity* ents = entity_system_get_all_entities();
        Uint32 max = entity_system_get_count();
        for (Uint32 i = 0; i < max; ++i) {
            if (!ents[i]._inuse) continue;
            // Hide map entity bounding box
            if (strncmp(ents[i].name, "map", 3) == 0) continue;
            // Pick color: red for player/monster, blue for houses, gray for others
            GFC_Color color = gfc_color(0.5f, 0.5f, 0.5f, 0.3f); // default: gray, 30% opacity
            if (strncmp(ents[i].name, "monster", 7) == 0) color = gfc_color(1.0f, 0.0f, 0.0f, 0.3f); // player
            else if (strncmp(ents[i].name, "house", 5) == 0) color = gfc_color(0.0f, 0.5f, 1.0f, 0.3f); // houses
            // Draw the bounding box as a solid cube
            // gf3d_draw_cube_solid(ents[i].bounds, gfc_vector3d(0,0,0), gfc_vector3d(0,0,0), gfc_vector3d(1,1,1), color); // HIDDEN: collision box visual
        }

        // Particles: draw any scheduled effects (attack burst, trail, landing puff)
        if (dino1 && dino1->entity)
        {
            GFC_Vector3D dp = dino1->entity->position;
            // One-shot burst on attack start
            if (g_attack_burst_frames > 0)
            {
                for (int i = 0; i < 18; ++i) {
                    float t = (float)i / 18.0f;
                    float ang = t * 2.0f * (float)GFC_PI;
                    GFC_Vector3D pp = dp;
                    pp.x += cosf(ang) * 0.7f;
                    pp.y += sinf(ang) * 0.7f;
                    pp.z += 0.4f;
                    GFC_Color c = gfc_color(1.0f, 0.8f * (1.0f - t), 0.2f + 0.6f * t, 1.0f);
                    gf3d_particle_draw(gf3d_particle(pp, c, 10.0f));
                }
                g_attack_burst_frames = 0;
            }

            // Continuous short trail while spinning
            if (g_dino_attack_active)
            {
                for (int i = 0; i < 10; ++i) {
                    float t = (float)i / 10.0f;
                    float ang = dino1->entity->rotation.z + t * 2.0f * (float)GFC_PI;
                    GFC_Vector3D pp = dp;
                    pp.x += cosf(ang) * 0.9f;
                    pp.y += sinf(ang) * 0.9f;
                    pp.z += 0.5f;
                    gf3d_particle_draw(gf3d_particle(pp, GFC_COLOR_YELLOW, 8.0f));
                }
            }

            // Landing puff decay over a few frames
            if (g_landing_puff_frames > 0)
            {
                int rings = 2;
                float base = (float)g_landing_puff_frames;
                for (int r = 0; r < rings; ++r) {
                    float radius = 0.3f + 0.15f * (float)r + (6.0f - base) * 0.03f;
                    for (int i = 0; i < 10; ++i) {
                        float t = (float)i / 10.0f;
                        float ang = t * 2.0f * (float)GFC_PI;
                        GFC_Vector3D pp = dp;
                        pp.x += cosf(ang) * radius;
                        pp.y += sinf(ang) * radius;
                        pp.z += 0.1f;
                        GFC_Color dust = gfc_color(0.65f, 0.58f, 0.5f, 1.0f);
                        gf3d_particle_draw(gf3d_particle(pp, dust, 6.0f));
                    }
                }
                g_landing_puff_frames--;
            }
        }

        if (frame_count < 5) slog("Frame %d: Drawing UI", frame_count);
        // UI elements (top-left controls)
                // HUD: show last collected power-up in bottom left for a few seconds
                if (g_powerup_hud_msg[0] && SDL_GetTicks() - g_powerup_hud_msg_time < POWERUP_HUD_MSG_DURATION) {
                    // Draw a shadow for better visibility
                    gf2d_font_draw_line_tag(g_powerup_hud_msg, FT_H1, gfc_color(0,0,0,0.7f), gfc_vector2d(24, 604));
                    // Draw the main text in bold yellow, higher on the screen, larger font
                    gf2d_font_draw_line_tag(g_powerup_hud_msg, FT_H1, gfc_color(1,0.95f,0.2f,1), gfc_vector2d(20, 600));
                }
                // HUD: show slow-motion cooldown in bottom left if active
                if (g_power_slow_active) {
                    Uint32 now = SDL_GetTicks();
                    float seconds_left = (g_power_slow_end_time > now) ? (g_power_slow_end_time - now) / 1000.0f : 0.0f;
                    char slowBuf[64];
                    snprintf(slowBuf, sizeof(slowBuf), "Slow Motion: %.1fs left", seconds_left);
                    gf2d_font_draw_line_tag(slowBuf, FT_H2, gfc_color(0.7f,0.7f,0.7f,1), gfc_vector2d(20, 630));
                }
                // HUD: show speed boost cooldown in bottom left if active
                if (g_power_speed_active) {
                    Uint32 now = SDL_GetTicks();
                    float seconds_left = (g_power_speed_end_time > now) ? (g_power_speed_end_time - now) / 1000.0f : 0.0f;
                    if (seconds_left > 0.0f) {
                        char speedBuf[64];
                        snprintf(speedBuf, sizeof(speedBuf), "Speed Boost: %.1fs left", seconds_left);
                        gf2d_font_draw_line_tag(speedBuf, FT_H2, gfc_color(0.2f,0.9f,0.2f,1), gfc_vector2d(20, 660));
                    }
                }
        gf2d_font_draw_line_tag("Controls", FT_H3, GFC_COLOR_WHITE, gfc_vector2d(10, 10));
        gf2d_font_draw_line_tag("WASD: Move", FT_H3, GFC_COLOR_GREEN, gfc_vector2d(10, 70));
        {
            int rmb_ui = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
            if (rmb_ui) {
                gf2d_font_draw_line_tag("SPACE: Jump | LMB: Attack | RMB+Drag: Rotate | Wheel: Zoom", FT_H3, GFC_COLOR_CYAN, gfc_vector2d(10, 100));
            } else {
                gf2d_font_draw_line_tag("SPACE: Jump | LMB: Attack | RMB: Look", FT_H3, GFC_COLOR_CYAN, gfc_vector2d(10, 100));
            }
        }

        // UI elements (top-right world/gameplay HUD)
        {
            GFC_Vector2D res = gf3d_vgraphics_get_view_extent_as_vector2d();
            float hud_x = res.x - 350.0f; // fixed-width HUD area anchored to right
            float hud_y = 10.0f;

            // Timer (mm:ss)
            Uint32 elapsed_ms = SDL_GetTicks() - game_start_ticks;
            Uint32 total_sec = elapsed_ms / 1000;
            Uint32 mm = total_sec / 60;
            Uint32 ss = total_sec % 60;
            char timeBuf[64];
            snprintf(timeBuf, sizeof(timeBuf), "Time: %02u:%02u", mm, ss);
            gf2d_font_draw_line_tag(timeBuf, FT_H3, GFC_COLOR_WHITE, gfc_vector2d(hud_x, hud_y));

            // Enemies list placeholder
            gf2d_font_draw_line_tag("Enemies", FT_H3, GFC_COLOR_YELLOW, gfc_vector2d(hud_x, hud_y + 30));
            gf2d_font_draw_line_tag("- (none yet)", FT_H3, GFC_COLOR_WHITE, gfc_vector2d(hud_x + 15, hud_y + 55));

            // Buildings list placeholder
            gf2d_font_draw_line_tag("Buildings", FT_H3, GFC_COLOR_YELLOW, gfc_vector2d(hud_x, hud_y + 85));
            gf2d_font_draw_line_tag("- (none yet)", FT_H3, GFC_COLOR_WHITE, gfc_vector2d(hud_x + 15, hud_y + 110));
        }
        
        // System status
        char entityCount[64];
        
        // console view that things are rendering
        if (frame_count < 5) slog("Frame %d: Drawing mouse", frame_count);
        gf2d_mouse_draw(); // mouse is hidden, so this won't draw anything
        // submit particle draw commands for this frame
        gf3d_particle_submit_pipe_commands();
        if (frame_count < 5) slog("Frame %d: Ending render", frame_count);
        gf3d_vgraphics_render_end();


        // --- ICE AREA RANDOM SPAWN LOGIC ---
static float ice_area_next_spawn_time = 0;
if (SDL_GetTicks() / 1000.0f > ice_area_next_spawn_time) {
    ice_area_spawn_random();
    float interval = 4.0f + ((float)rand() / RAND_MAX) * 7.0f; // 4-11s
    ice_area_next_spawn_time = SDL_GetTicks() / 1000.0f + interval;
}

        if (gfc_input_command_down("exit")) _done = 1; // exit condition

        // Unlock mouse if ESC is pressed
        if (gfc_input_command_pressed("exit") && mouse_locked) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            mouse_locked = 0;
            slog("Mouse unlocked (ESC pressed)");
        }

        // Relock mouse if user clicks in window and it's unlocked
        if (!mouse_locked && gf2d_mouse_button_pressed(0)) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            mouse_locked = 1;
            slog("Mouse relocked (LMB click)");
        }
        
        // Toggle fullscreen with Alt+Enter
        if ((gfc_input_command_pressed("togglefullscreen") || gfc_input_key_pressed("RETURN")) && (SDL_GetModState() & KMOD_ALT)) {
            SDL_Window* win = gf3d_vgraphics_get_window();
            if (win) {
                Uint32 flags = SDL_GetWindowFlags(win);
                if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                    SDL_SetWindowFullscreen(win, 0);
                    slog("Toggled to windowed mode");
                } else {
                    SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    slog("Toggled to fullscreen mode");
                }
            }
        }
        if (frame_count < 5) slog("Frame %d: Frame delay", frame_count);
        game_frame_delay();
        frame_count++;
    }
    vkDeviceWaitIdle(gf3d_vgraphics_get_default_logical_device());
    // Cleanup event watch
    SDL_DelEventWatch(sdl_wheel_watch, NULL);
    
    //cleanup
    if (world) {
        world_free(world);
    }
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
    Uint32 delay = frame_delay;
    if (g_power_slow_active) {
        Uint32 tnow = SDL_GetTicks();
        if (tnow >= g_power_slow_end_time) {
            g_power_slow_active = 0;
            slog("[POWERUP] Slow Motion expired");
        } else {
            delay = (Uint32)(frame_delay / SLOW_MOTION_FACTOR);
        }
    }
    if (diff < delay)
    {
        SDL_Delay(delay - diff);
    }
    fps = 1000.0/fmax(SDL_GetTicks() - then,0.001);
//     slog("fps: %f",fps);
}
/*eol@eof*/

// Helper: compute world-space bounds from map entity and its mesh bounds, using current padding
// Extra asymmetrical margins to keep player away from top/right walls specifically
static float g_edge_margin_top = 0.0f;   // keep exact
static float g_edge_margin_right = 0.0f; // keep exact

static void compute_map_world_bounds_from_entity(Entity *mapEntity, World *world)
{
    if (!mapEntity || !mapEntity->mesh) return;
    // Prefer terrain's bounds (rectangle of floor) if available; else fallback to map mesh AABB
    GFC_Box b;
    if (world && world->terrain) {
        b = world->terrain->bounds;
    } else {
        b = mapEntity->mesh->bounds; // b.x/y/z = min corner, b.w/h/d = size
    }
    const GFC_Vector3D s = mapEntity->scale;
    const GFC_Vector3D p = mapEntity->position;
    // No rotation assumed for map entity
    const float extentX = (b.w) * s.x;
    const float extentY = (b.h) * s.y;
    const float shrinkX = extentX * g_map_shrink_pct;
    const float shrinkY = extentY * g_map_shrink_pct;
    g_map_min_x = p.x + (b.x) * s.x + g_map_pad + shrinkX;
    g_map_max_x = p.x + (b.x + b.w) * s.x - g_map_pad - shrinkX - g_edge_margin_right;
    g_map_min_y = p.y + (b.y) * s.y + g_map_pad + shrinkY;
    g_map_max_y = p.y + (b.y + b.h) * s.y - g_map_pad - shrinkY - g_edge_margin_top;
    // Ensure min <= max even at extreme shrink values
    if (g_map_min_x > g_map_max_x) { float mid = (g_map_min_x + g_map_max_x) * 0.5f; g_map_min_x = g_map_max_x = mid; }
    if (g_map_min_y > g_map_max_y) { float mid = (g_map_min_y + g_map_max_y) * 0.5f; g_map_min_y = g_map_max_y = mid; }
        slog("Map world bounds: X:[%.2f, %.2f] Y:[%.2f, %.2f] (exact, no shrink)",
            g_map_min_x, g_map_max_x, g_map_min_y, g_map_max_y);
}
