#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include "simple_logger.h"
#include "gfc_types.h"
#include "gfc_primitives.h"
#include "gfc_matrix.h"
#include "gfc_list.h"
#include "gf3d_mesh.h"
#include "gf3d_obj_load.h"
#include "world.h"
#include "entity.h"

// Fix for unresolved externals
int g_dino_on_ground = 0;
int player_input_active = 0;


typedef struct
{
    Entity* entity_list;
    Uint32 entity_max;
} EntitySystem;

static EntitySystem entity_system = { NULL, 0 };

void entity_system_close() {
    int i;
    if (entity_system.entity_list) {
        for (i = 0; i < entity_system.entity_max; i++) {
            if (entity_system.entity_list[i]._inuse) {
                entity_free(&entity_system.entity_list[i]);
            }
        }
        free(entity_system.entity_list);
        entity_system.entity_list = NULL;
    }
    entity_system.entity_max = 0;
    slog("entity system closed.");
}

Entity* entity_new() {
    int i;
    if (entity_system.entity_list) {
        for (i = 0; i < entity_system.entity_max; i++) {
            if (!entity_system.entity_list[i]._inuse) {
                memset(&entity_system.entity_list[i], 0, sizeof(Entity));
                entity_system.entity_list[i]._inuse = 1;
                entity_system.entity_list[i].scale = gfc_vector3d(1, 1, 1);
                return &entity_system.entity_list[i];
            }
        }
    }
    return NULL;
}

void entity_free(Entity* ent) {
    if (!ent) return;
    if (ent->free)ent->free(ent);
    if (ent->mesh) {
        gf3d_mesh_free(ent->mesh);
    }
    if (ent->texture) {
        gf3d_texture_free(ent->texture);
    }
    memset(ent, 0, sizeof(Entity));
}

void entity_draw_shadow(Entity *ent){
    GFC_Vector3D drawPosition;
    GFC_Matrix4 modelMat;
    if((!ent)||(!ent->drawShadow)) return;
    gfc_vector3d_copy(drawPosition, ent->position);
    drawPosition.z += 0.1;
    gfc_matrix4_from_vectors(
        modelMat,
        ent->position,
        ent->rotation,
        gfc_vector3d(ent->scale.x,ent->scale.y,0.01));
    
    gf3d_mesh_draw(
        ent->mesh,
        modelMat,
        ent->color,
        ent->texture,
        gfc_vector3d(0,0,0),
        gfc_color8(0,0,0,128));
}


void entity_draw(Entity* ent, GFC_Vector3D lightPos, GFC_Color lightColor) {
    if (!ent) return;
    if (!ent->_inuse) return;

    // slog("[ENTITY DRAW] name=%s mesh=%p color=(%.2f %.2f %.2f %.2f) pos=(%.2f %.2f %.2f)",
    //     ent->name, ent->mesh,
    //     ent->color.r, ent->color.g, ent->color.b, ent->color.a,
    //     ent->position.x, ent->position.y, ent->position.z);

    GFC_Matrix4 modelMat;
    gfc_matrix4_from_vectors(
        modelMat,
        ent->position,
        ent->rotation,
        ent->scale);

    gf3d_mesh_draw(
        ent->mesh,
        modelMat,
        ent->color,
        ent->texture,
        lightPos,
        lightColor
    );

    if (ent->draw) {
        ent->draw(ent);
    }
}

// feature for flooring: try mesh triangles along a downward edge and update best floor hit
static void entity_test_mesh_edge_down(
    Mesh* mesh,
    GFC_Matrix4 model,
    GFC_Edge3D ray,
    float startZ,
    int *hit,
    float *bestZ,
    GFC_Vector3D *best)
{
    int i, count;
    MeshPrimitive* prim;
    GFC_Vector3D c = {0};
    if (!mesh || !hit || !bestZ || !best) return;
    count = gfc_list_get_count(mesh->primitives);
    for (i = 0; i < count; i++) {
        prim = gfc_list_get_nth(mesh->primitives, i);
        if (!prim || !prim->objData) continue;
        memset(&c, 0, sizeof(c));
        if (gf3d_obj_edge_test(prim->objData, model, ray, &c)) {
            if (c.z <= startZ && c.z > *bestZ) {
                *bestZ = c.z; *best = c; *hit = 1;
            }
        }
    }
}

#include "entity.h"



Uint8 entity_get_floor_position(Entity *entity, World *world, GFC_Vector3D *contact)
{
    if (!entity) return 0;

    Entity *ents = entity_system_get_all_entities();
    Uint32 max = entity_system_get_count();

    for (Uint32 i = 0; i < max; i++) {
        if (!ents[i]._inuse) continue;
        if (strncmp(ents[i].name, "map", 3) != 0) continue;
        if (!ents[i].mesh) break;

        GFC_Box b = ents[i].mesh->bounds;
        GFC_Vector3D s = ents[i].scale;
        GFC_Vector3D p = ents[i].position;

        float minx = p.x + b.x * s.x;
        float maxx = p.x + (b.x + b.w) * s.x;
        float miny = p.y + b.y * s.y;
        float maxy = p.y + (b.y + b.h) * s.y;

        if (entity->position.x >= minx && entity->position.x <= maxx &&
            entity->position.y >= miny && entity->position.y <= maxy) {
            float floorZ = g_map_floor_z + 1.45f;
            if (contact) *contact = gfc_vector3d(entity->position.x, entity->position.y, floorZ);
            slog("FLOOR (box) at (%.2f, %.2f): hit=1 z=%.2f (offset=1.450)", entity->position.x, entity->position.y, floorZ);
            return 1;
        }
        break;
    }

    slog("FLOOR (box) at (%.2f, %.2f): hit=0", entity->position.x, entity->position.y);
    return 0;
}

void entity_think(Entity* ent) {
    if (!ent) return;
    if (!ent->_inuse) return;
    if (ent->think) ent->think(ent);
}

void entity_update(Entity* ent) {
    if (!ent) return;
    if (!ent->_inuse) return;


    // Physics: velocity += acceleration, position += velocity
    ent->velocity.x += ent->acceleration.x;
    ent->velocity.y += ent->acceleration.y;
    ent->velocity.z += ent->acceleration.z;

    ent->position.x += ent->velocity.x;
    ent->position.y += ent->velocity.y;
    ent->position.z += ent->velocity.z;


    // Apply friction/damping to slow down over time
    float friction = 0.96f;
    int is_player = (strncmp(ent->name, "monster", 7) == 0);
    if (is_player) {
        // Stronger friction for player
        extern int g_dino_on_ground;
        extern int player_input_active;
        if (g_dino_on_ground) {
            if (!player_input_active) {
                // No input and on ground: stop sliding
                ent->velocity.x = 0;
                ent->velocity.y = 0;
            } else {
                // Input: apply strong friction for quick stop
                friction = 0.7f;
                ent->velocity.x *= friction;
                ent->velocity.y *= friction;
            }
        } else {
            // In air: normal friction
            ent->velocity.x *= friction;
            ent->velocity.y *= friction;
        }
        ent->velocity.z *= friction;
    } else {
        // Non-player entities: normal friction
        ent->velocity.x *= friction;
        ent->velocity.y *= friction;
        ent->velocity.z *= friction;
    }

    if (ent->update) {
        ent->update(ent);
    }
}

void entity_system_init(Uint32 max_ents) {
    if (!max_ents) {
        slog("cannot init entity system with zero ents");
        return;
    }
    entity_system.entity_list = gfc_allocate_array(sizeof(Entity), max_ents);
    if (!entity_system.entity_list) {
        slog("failed to allocate %i entities for the system", max_ents);
        return;
    }
    entity_system.entity_max = max_ents;
    atexit(entity_system_close);
    slog("entity system initialized with %i entities.", max_ents);
}

void entity_system_draw_all(GFC_Vector3D lightPos, GFC_Color lightColor) {
    int i;
    for (i = 0; i < entity_system.entity_max; i++) {
        if (entity_system.entity_list[i]._inuse) {
            entity_draw(&entity_system.entity_list[i], lightPos, lightColor);
        }
    }
}

void entity_system_think_all() {
    int i;
    for (i = 0; i < entity_system.entity_max; i++) {
        if (entity_system.entity_list[i]._inuse) {
            entity_think(&entity_system.entity_list[i]);
        }
    }
}

void entity_system_update_all() {
    int i;
    for (i = 0; i < entity_system.entity_max; i++) {
        if (entity_system.entity_list[i]._inuse) {
            entity_update(&entity_system.entity_list[i]);
        }
    }
}

Entity* entity_system_get_all_entities() {
    return entity_system.entity_list;
}

Uint32 entity_system_get_count() {
    return entity_system.entity_max;
}

Uint32 entity_system_get_max() {
    return entity_system.entity_max;
}