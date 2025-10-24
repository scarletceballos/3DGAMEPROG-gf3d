#include <string.h>
#include <stdlib.h>
#include "simple_logger.h"
#include "gfc_types.h"
#include "gfc_primitives.h"
#include "gfc_matrix.h"
#include "gf3d_mesh.h"
#include "entity.h"

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

Uint8 entity_get_floor_position(Entity *entity, World *world, GFC_Vector3D *contact){
    GFC_Vector3D up,down;
    if(!entity)return 0;
    // TODO: do floor position calculation
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