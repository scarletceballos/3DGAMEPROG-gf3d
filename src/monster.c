#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "simple_logger.h"
#include "gfc_types.h"
#include "monster.h"
#include "entity.h"
#include "gfc_input.h"

typedef struct
{
    Monster* monster_list;
    Uint32 monster_max;
    Uint32 monster_count;
} MonsterSystem;

typedef struct
{
    Entity *camera;
    float move_step;
    int direction; // 1 for forward, -1 for backward
    int behavior_type; // 0=normal, 1=fast, 2=slow
    float creation_time; // Track when entity was created
} MonsterEntityData;

static MonsterSystem monster_system = { NULL, 0, 0 };

void monster_system_close();

void monster_think(Entity *self) {
    // let game.c control facing based on movement. game work better this way for now
    // Keep rotation values normalized only; no autonomous rotation here.
    // TODO: maybe move movement here one day
    if (!self) return;
    while (self->rotation.x > 2 * GFC_PI) self->rotation.x -= 2 * GFC_PI;
    while (self->rotation.x < 0) self->rotation.x += 2 * GFC_PI;
    while (self->rotation.y > 2 * GFC_PI) self->rotation.y -= 2 * GFC_PI;
    while (self->rotation.y < 0) self->rotation.y += 2 * GFC_PI;
    while (self->rotation.z > 2 * GFC_PI) self->rotation.z -= 2 * GFC_PI;
    while (self->rotation.z < 0) self->rotation.z += 2 * GFC_PI;
}

void monster_update(Entity *self) {
    if (!self) return;
    // Generic update can go here if needed
}

void monster_free_data(Entity *self) {
    if ((!self) || (!self->data)) return;
    free(self->data);
    self->data = NULL;
}

void monster_free(Monster* monster)
{
    if (!monster) return;
    
    slog("Freeing monster entity");
    
    if (monster->entity) {
        entity_free(monster->entity);
        monster->entity = NULL;
    }
    
    // Clear the entire monster slot for reuse
    memset(monster, 0, sizeof(Monster));
    
    // Decrement count so we know a slot is available
    if (monster_system.monster_count > 0) {
        monster_system.monster_count--;
    }
    
    slog("Monster freed. Current count: %d", monster_system.monster_count);
}

void monster_set_camera_ent(Entity *self, Entity *camera) {
    MonsterEntityData *data;
    if ((!self) || (!camera)) return;
    data = (MonsterEntityData*)self->data;
    if (data) {
        data->camera = camera;
    }
}

void monster_system_init(Uint32 max_monsters)
{
    if (!max_monsters) {
        slog("cannot init monster system with zero monsters");
        return;
    }
    
    monster_system.monster_list = gfc_allocate_array(sizeof(Monster), max_monsters);
    if (!monster_system.monster_list) {
        slog("failed to allocate %i monsters for the system", max_monsters);
        return;
    }
    
    monster_system.monster_max = max_monsters;
    monster_system.monster_count = 0;
    
    atexit(monster_system_close);
    slog("monster system initialized with %i monsters.", max_monsters);
}

void monster_system_close()
{
    int i;
    if (monster_system.monster_list) {
        for (i = 0; i < monster_system.monster_max; i++) {
            if (monster_system.monster_list[i].entity) {
                monster_free(&monster_system.monster_list[i]);
            }
        }
        free(monster_system.monster_list);
        monster_system.monster_list = NULL;
    }
    monster_system.monster_max = 0;
    monster_system.monster_count = 0;
    slog("monster system closed.");
}

Monster* monster_new(Mesh* mesh, Texture* texture, GFC_Vector3D position)
{
    int i;
    Monster* monster = NULL;
    Entity* entity = NULL;
    MonsterEntityData* data = NULL;
    
    if (!monster_system.monster_list) {
        slog("monster system not initialized");
        return NULL;
    }
    
    // Find free monster slot
    for (i = 0; i < monster_system.monster_max; i++) {
        if (!monster_system.monster_list[i].entity) { // Check if entity is NULL (free slot)
            monster = &monster_system.monster_list[i];
            slog("Found free monster slot at index %d", i);
            break;
        }
    }
    
    if (!monster) {
        slog("no free monster slots available (current count: %d, max: %d)", 
             monster_system.monster_count, monster_system.monster_max);
        return NULL;
    }
    
    // Create entity
    entity = entity_new();
    if (!entity) {
        slog("failed to create entity for monster");
        return NULL;
    }
    
    // Create monster data
    data = gfc_allocate_array(sizeof(MonsterEntityData), 1);
    if (!data) {
        entity_free(entity);
        slog("failed to allocate monster data");
        return NULL;
    }
    
    // Initialize monster
    memset(monster, 0, sizeof(Monster));
    monster->entity = entity;
    
    // Setup entity
    entity->mesh = mesh;
    entity->texture = texture;
    entity->position = position;
    entity->rotation = gfc_vector3d(0, 0, 0);  // Face camera (no rotation)
    // Make dino 50% smaller than before (was 2.0)
    entity->scale = gfc_vector3d(1.0f, 1.0f, 1.0f);
    entity->color = GFC_COLOR_WHITE;
    entity->data = data;
    entity->think = monster_think;
    entity->update = monster_update;
    entity->free = monster_free_data;
    
    // Initialize monster data
    data->camera = NULL;
    data->move_step = 0.1f;
    data->direction = 1; // Not used for spinning, but keep for compatibility
    data->behavior_type = monster_system.monster_count % 3; // Cycle through 0, 1, 2
    data->creation_time = SDL_GetTicks() * 0.001f; // Store creation time
    data->behavior_type = monster_system.monster_count % 3; // Cycle through 0, 1, 2
    data->creation_time = SDL_GetTicks() * 0.001f; // Store creation time
    
    gfc_line_cpy(entity->name, "monster");
    
    monster_system.monster_count++;
    
    slog("created monster at position (%f, %f, %f) with behavior type %d", 
         position.x, position.y, position.z, data->behavior_type);
    return monster;
}

void monster_cleanup_oldest() {
    int i;
    float oldest_time = SDL_GetTicks() * 0.001f;
    int oldest_index = -1;
    
    slog("Looking for oldest monster to clean up...");
    
    // Find the oldest monster
    for (i = 0; i < monster_system.monster_max; i++) {
        if (monster_system.monster_list[i].entity) {
            MonsterEntityData* data = (MonsterEntityData*)monster_system.monster_list[i].entity->data;
            if (data && data->creation_time < oldest_time) {
                oldest_time = data->creation_time;
                oldest_index = i;
            }
        }
    }
    
    // Remove the oldest monster to demonstrate cleanup
    if (oldest_index >= 0) {
        slog("Cleaning up oldest monster at index %d (created at time %.2f)", oldest_index, oldest_time);
        monster_free(&monster_system.monster_list[oldest_index]);
        slog("Cleanup complete. Slots available for new monsters.");
    } else {
        slog("No monsters to clean up");
    }
}