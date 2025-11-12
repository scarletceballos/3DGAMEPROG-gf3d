// Spawns a power-up of the given type in front of the player entity
#include <stdlib.h>
#include "gf3d_texture.h"
#include "gf3d_mesh.h"
#include "entity.h"
#include "powerup.h"
#include "simple_logger.h"

void powerup_spawn_near_player(Entity* player, float map_floor_z, Texture* texture, PowerupType type) {
    if (!player) return;
    Mesh* mesh = gf3d_mesh_load("models/primitives/cube.obj");
    if (!mesh) return;
    float scale = 0.8f;
    // Match the working logic from spawn_center
    float padZ = map_floor_z + (mesh->bounds.d * scale * 0.5f) + 9.4f;
    if (padZ < 0.0f) padZ = 0.0f;
    //slog("[POWERUP DEBUG] map_floor_z=%.2f mesh->bounds.d=%.2f scale=%.2f padZ=%.2f", map_floor_z, mesh->bounds.d, scale, padZ);
    float spawn_dist = 6.0f;
    float facing = player->rotation.z;
    float fx = -sinf(facing);
    float fy =  cosf(facing);
    float spawn_x = player->position.x + fx * spawn_dist;
    float spawn_y = player->position.y + fy * spawn_dist;
    Entity* ent = entity_new();
    if (!ent) return;
    gfc_line_cpy(ent->name, "powerup");
    ent->mesh = mesh;
    ent->texture = texture;
    ent->scale = gfc_vector3d(scale, scale, scale);
    ent->position = gfc_vector3d(spawn_x, spawn_y, padZ);
    ent->rotation = gfc_vector3d(0, 0, 0);
    PowerupData* data = calloc(1, sizeof(PowerupData));
    if (data) {
        data->type = type;
        data->spawn_time = SDL_GetTicks() / 1000.0f;
        data->active = 1;
        ent->data = data;
    }
    switch (type) {
        case POWERUP_SLAP:    ent->color = gfc_color(1.0f, 0.5f, 0.0f, 1.0f); break; // orange
        case POWERUP_CHAIN:   ent->color = gfc_color(1.0f, 0.0f, 0.0f, 1.0f); break; // red
        case POWERUP_THUNDER: ent->color = gfc_color(0.2f, 0.4f, 1.0f, 1.0f); break; // blue
        case POWERUP_GOLDEN:  ent->color = gfc_color(1.0f, 0.7f, 0.2f, 1.0f); break; // yellow-orange
        case POWERUP_SLOW:    ent->color = gfc_color(0.5f, 0.5f, 0.5f, 1.0f); break; // grey
        case POWERUP_SPEED:   ent->color = gfc_color(0.0f, 1.0f, 0.3f, 1.0f); break; // green
        default:              ent->color = gfc_color(1, 1, 1, 1); break;
    }
    ent->bounds = mesh->bounds;
    gfc_box_scale_local(&ent->bounds, ent->scale.x, ent->scale.y, ent->scale.z);
    gfc_box_translate_local(&ent->bounds, ent->position.x, ent->position.y, ent->position.z);
    ent->drawShadow = 0;
    ent->doGenericUpdate = 1;
    ent->think = powerup_think;
    ent->static_bounds = 0;
    //slog("[POWERUP] Spawned type %d in front of player at (%.2f, %.2f, %.2f)", type, ent->position.x, ent->position.y, ent->position.z);
}

// dummy power-up
// void powerup_spawn_center(float centerX, float centerY, float map_floor_z, Texture* texture, PowerupType type) {
//     Mesh* mesh = gf3d_mesh_load("models/primitives/cube.obj");
//     if (!mesh) return;
//     float scale = 0.8f;
//     float padZ = map_floor_z + (mesh->bounds.d * scale * 0.5f) + 0.06f;
//     Entity* ent = entity_new();
//     if (!ent) return;
//     gfc_line_cpy(ent->name, "powerup");
//     ent->mesh = mesh;
//     ent->texture = texture;
//     ent->scale = gfc_vector3d(scale, scale, scale);
//     ent->position = gfc_vector3d(centerX, centerY, padZ);
//     ent->rotation = gfc_vector3d(0, 0, 0);
//     PowerupData* data = calloc(1, sizeof(PowerupData));
//     if (data) {
//         data->type = type;
//         data->spawn_time = SDL_GetTicks() / 1000.0f;
//         data->active = 1;
//         ent->data = data;
//     }
//     switch (type) {
//         case POWERUP_SLAP:    ent->color = gfc_color(1.0f, 0.5f, 0.0f, 1.0f); break; // orange
//         case POWERUP_CHAIN:   ent->color = gfc_color(1.0f, 0.0f, 0.0f, 1.0f); break; // red
//         case POWERUP_THUNDER: ent->color = gfc_color(0.2f, 0.4f, 1.0f, 1.0f); break; // blue
//         case POWERUP_GOLDEN:  ent->color = gfc_color(1.0f, 0.7f, 0.2f, 1.0f); break; // yellow-orange
//         case POWERUP_SLOW:    ent->color = gfc_color(0.5f, 0.5f, 0.5f, 1.0f); break; // grey
//         case POWERUP_SPEED:   ent->color = gfc_color(0.0f, 1.0f, 0.3f, 1.0f); break; // green
//         default:              ent->color = gfc_color(1, 1, 1, 1); break;
//     }
//     ent->bounds = mesh->bounds;
//     gfc_box_scale_local(&ent->bounds, ent->scale.x, ent->scale.y, ent->scale.z);
//     gfc_box_translate_local(&ent->bounds, ent->position.x, ent->position.y, ent->position.z);
//     ent->drawShadow = 0;
//     ent->doGenericUpdate = 1;
//     ent->think = powerup_think;
//     ent->static_bounds = 0; // Always update bounds
//     slog("[POWERUP] Center test type %d at (%.2f, %.2f, %.2f)", data ? data->type : 0, ent->position.x, ent->position.y, ent->position.z);
// }


// void powerup_spawn_random(float minX, float maxX, float minY, float maxY, float map_floor_z, Texture* texture) {
//     Mesh* mesh = gf3d_mesh_load("models/primitives/cube.obj");
//     if (!mesh) return;
//     float scale = 0.8f;
//     float padZ = map_floor_z + (mesh->bounds.d * scale * 0.5f) + 0.004f;
//     float randX = minX + ((float)rand() / RAND_MAX) * (maxX - minX);
//     float randY = minY + ((float)rand() / RAND_MAX) * (maxY - minY);
//     Entity* ent = entity_new();
//     if (!ent) return;
//     gfc_line_cpy(ent->name, "powerup");
//     ent->mesh = mesh;
//     ent->texture = texture;
//     ent->scale = gfc_vector3d(scale, scale, scale);
//     ent->position = gfc_vector3d(randX, randY, padZ);
//     ent->rotation = gfc_vector3d(0, 0, 0);
//     // Set color by powerup type
//     PowerupData* data = calloc(1, sizeof(PowerupData));
//     PowerupType type = POWERUP_SLAP;
//     if (data) {
//         type = (PowerupType)(1 + rand() % (POWERUP_MAX - 1));
//         data->type = type;
//         data->spawn_time = SDL_GetTicks() / 1000.0f;
//         data->active = 1;
//         ent->data = data;
//     }
//     switch (type) {
//         case POWERUP_SLAP:    ent->color = gfc_color(1.0f, 0.5f, 0.0f, 1.0f); break; // orange
//         case POWERUP_CHAIN:   ent->color = gfc_color(1.0f, 0.0f, 0.0f, 1.0f); break; // red
//         case POWERUP_THUNDER: ent->color = gfc_color(0.2f, 0.4f, 1.0f, 1.0f); break; // blue
//         case POWERUP_GOLDEN:  ent->color = gfc_color(1.0f, 0.7f, 0.2f, 1.0f); break; // yellow-orange
//         case POWERUP_SLOW:    ent->color = gfc_color(0.5f, 0.5f, 0.5f, 1.0f); break; // grey
//         case POWERUP_SPEED:   ent->color = gfc_color(0.0f, 1.0f, 0.3f, 1.0f); break; // green
//         default:              ent->color = gfc_color(1, 1, 1, 1); break;
//     }
//     ent->bounds = mesh->bounds;
//     gfc_box_scale_local(&ent->bounds, ent->scale.x, ent->scale.y, ent->scale.z);
//     gfc_box_translate_local(&ent->bounds, ent->position.x, ent->position.y, ent->position.z);
//     ent->drawShadow = 0;
//     ent->doGenericUpdate = 1;
//     ent->think = powerup_think;
//     ent->static_bounds = 0; // Always update bounds
//     slog("[POWERUP] Spawned type %d at (%.2f, %.2f, %.2f)", data ? data->type : 0, ent->position.x, ent->position.y, ent->position.z);
// }

void powerup_think(Entity* self) {
    if (!self || !self->_inuse || !self->data) return;
    if (self->_inuse == 0) {
        // Entity system will clean up, skip all logic
        return;
    }
    // Always update bounds based on current position/scale, expand for easier pickup
    float expand = 1.3f;
    self->bounds.x = self->position.x - self->scale.x * 0.5f * expand;
    self->bounds.y = self->position.y - self->scale.y * 0.5f * expand;
    self->bounds.z = self->position.z - self->scale.z * 0.5f * expand;
    self->bounds.w = self->scale.x * expand;
    self->bounds.h = self->scale.y * expand;
    self->bounds.d = self->scale.z * expand;
    slog("[POWERUP THINK] name=%s pos=(%.2f %.2f %.2f) bounds=[%.2f %.2f %.2f %.2f %.2f %.2f]", self->name, self->position.x, self->position.y, self->position.z, self->bounds.x, self->bounds.y, self->bounds.z, self->bounds.w, self->bounds.h, self->bounds.d);
    PowerupData* data = (PowerupData*)self->data;
    float now = SDL_GetTicks() / 1000.0f;
    // Floating animation: up/down and rotation
    float float_height = 1.0f * sinf(now * 2.0f + (uintptr_t)self) + 0.0f;
    self->position.z += (float_height - (self->position.z - (self->position.z - float_height)));
    self->rotation.z += 0.04f; // rotate in place
    // Auto-destroy after 20s if not picked up
    if (now - data->spawn_time > 20.0f) {
        self->_inuse = 0;
        slog("[POWERUP] Expired: type %d", data->type);
        return;
    }
}
