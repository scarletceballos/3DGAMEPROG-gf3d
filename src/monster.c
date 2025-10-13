#include <string.h>
#include <stdlib.h>
#include "simple_logger.h"
#include "gfc_types.h"
#include "monster.h"
#include "entity.h"

typedef struct
{
    Monster* monster_list;
    Uint32 monster_max;
    Uint32 monster_count;
} MonsterSystem;

static MonsterSystem monster_system = { NULL, 0, 0 };

void monster_system_close();

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
    
    if (!monster_system.monster_list) {
        slog("monster system not initialized");
        return NULL;
    }
    
    // Find free monster slot
    for (i = 0; i < monster_system.monster_max; i++) {
        if (!monster_system.monster_list[i].entity) {
            monster = &monster_system.monster_list[i];
            break;
        }
    }
    
    if (!monster) {
        slog("no free monster slots available");
        return NULL;
    }
    
    // new entity here:
    entity = entity_new();
    if (!entity) {
        slog("failed to create entity for dino");
        return NULL;
    }
    
    // Initialize monster
    memset(monster, 0, sizeof(Monster));
    monster->entity = entity;
    
    // Setup entity
    entity->mesh = mesh;
    entity->texture = texture;
    entity->position = position;
    entity->rotation = gfc_vector3d(0, -GFC_HALF_PI, 0);  // work to have model look and stand face forward and upright
    entity->scale = gfc_vector3d(2.0f, 2.0f, 2.0f);
    entity->color = GFC_COLOR_WHITE;
    
    gfc_line_cpy(entity->name, "dino");
    
    monster_system.monster_count++;
    
    slog("created dino at position (%f, %f, %f)", position.x, position.y, position.z);
    return monster;
}

void monster_free(Monster* monster)
{
    if (!monster) return;
    
    if (monster->entity) {
        entity_free(monster->entity);
        monster->entity = NULL;
    }
    
    memset(monster, 0, sizeof(Monster));
    monster_system.monster_count--;
}