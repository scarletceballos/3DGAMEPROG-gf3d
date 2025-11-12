#include "jump_pad.h"
#include "map_debug.h"

#include "simple_logger.h"
#include <stdlib.h>

// Extern now in map_debug.h

void jump_pad_spawn_all(float minX, float maxX, float minY, float maxY, float map_floor_z, Texture* texture) {
        slog("[JUMP PAD] (spawn_all) minX=%.2f maxX=%.2f minY=%.2f maxY=%.2f", minX, maxX, minY, maxY);
    // Debug: print map floor Z and map mesh bounds Z
    if (g_map_mesh_debug) {
        slog("[DEBUG] map_floor_z=%.3f, map mesh bounds.z=%.3f, bounds.d=%.3f, bounds.z+bounds.d=%.3f", map_floor_z, g_map_mesh_debug->bounds.z, g_map_mesh_debug->bounds.d, g_map_mesh_debug->bounds.z + g_map_mesh_debug->bounds.d);
    } else {
        slog("[DEBUG] map_floor_z=%.3f, map mesh debug pointer is NULL", map_floor_z);
    }
    Mesh* padMesh = gf3d_mesh_load("models/jump_pad.obj");
    // slog("[JUMP PAD] padMesh=%p", padMesh);
    if (padMesh) {
        // Use mesh min Z for floor alignment and a uniform scale
        float padScale = 4.0f;
        float meshMinZ = padMesh->bounds.z;
        // Only spawn random jump pads
        float padHeight = padMesh->bounds.d;
        float manualOffset = 8.20f; // <-- Edit this value to adjust pad height above floor
        // Static variable to track last spawn time
        static float last_spawn_time = 0.0f;
        float now = SDL_GetTicks() / 1000.0f;
        Entity* ents = entity_system_get_all_entities();
        Uint32 max = entity_system_get_count();
        Entity* pads[3] = {0};
        float pad_times[3] = {0};
        int pad_count = 0;
        for (Uint32 i = 0; i < max; ++i) {
            if (!ents[i]._inuse) continue;
            if (strncmp(ents[i].name, "jump_pad", 8) == 0) {
                if (pad_count < 3) {
                    pads[pad_count] = &ents[i];
                    pad_times[pad_count] = ents[i].data ? *(float*)ents[i].data : 0.0f;
                    pad_count++;
                }
            }
        }
        // Only spawn a new pad if less than 3 exist and 3.2s have passed since last spawn
        if (pad_count < 3 && (now - last_spawn_time >= 3.2f || last_spawn_time == 0.0f)) {
            float randX = minX + ((float)rand() / RAND_MAX) * (maxX - minX);
            float randY = minY + ((float)rand() / RAND_MAX) * (maxY - minY);
            float padZ = map_floor_z + (padHeight * padScale * 0.5f) + manualOffset;
            Entity* padEntity = entity_new();
            if (padEntity) {
                char name[16];
                snprintf(name, sizeof(name), "jump_pad%d", rand() % 10000);
                gfc_line_cpy(padEntity->name, name);
                padEntity->mesh = padMesh;
                padEntity->texture = texture;
                padEntity->scale = gfc_vector3d(padScale, padScale, padScale);
                padEntity->position = gfc_vector3d(randX, randY, padZ);
                padEntity->rotation = gfc_vector3d(0, 0, 0);
                padEntity->color = gfc_color(1.0f, 0.0f, 1.0f, 1.0f);
                padEntity->bounds = padMesh->bounds;
                gfc_box_scale_local(&padEntity->bounds, padEntity->scale.x * 1.2f, padEntity->scale.y * 1.2f, padEntity->scale.z * 1.2f);
                gfc_box_translate_local(&padEntity->bounds, padEntity->position.x, padEntity->position.y, padEntity->position.z);
                padEntity->drawShadow = 0;
                padEntity->doGenericUpdate = 1;
                padEntity->think = jump_pad_think;
                padEntity->data = malloc(sizeof(float));
                if (padEntity->data) {
                    // Store spawn time in seconds
                    *(float*)padEntity->data = now;
                }
                slog("[JUMP PAD] SPAWNED: %s at (%.2f, %.2f, %.2f)", padEntity->name, padEntity->position.x, padEntity->position.y, padEntity->position.z);
                last_spawn_time = now;
            } else {
                slog("[JUMP PAD] Failed to allocate jump pad entity");
            }
        }
        // If already 3 pads, destroy the oldest and spawn one new (if 3.2s passed)
        if (pad_count == 3 && (now - last_spawn_time >= 3.2f)) {
            int oldest = 0;
            for (int i = 1; i < 3; ++i) {
                if (pad_times[i] < pad_times[oldest]) oldest = i;
            }
            pads[oldest]->_inuse = 0;
            slog("[JUMP PAD] OLDEST DESTROYED: %s", pads[oldest]->name);
            float randX = minX + ((float)rand() / RAND_MAX) * (maxX - minX);
            float randY = minY + ((float)rand() / RAND_MAX) * (maxY - minY);
            float padZ = map_floor_z + (padHeight * padScale * 0.5f) + manualOffset;
            Entity* padEntity = entity_new();
            if (padEntity) {
                char name[16];
                snprintf(name, sizeof(name), "jump_pad%d", rand() % 10000);
                gfc_line_cpy(padEntity->name, name);
                padEntity->mesh = padMesh;
                padEntity->texture = texture;
                padEntity->scale = gfc_vector3d(padScale, padScale, padScale);
                padEntity->position = gfc_vector3d(randX, randY, padZ);
                padEntity->rotation = gfc_vector3d(0, 0, 0);
                padEntity->color = gfc_color(1.0f, 0.0f, 1.0f, 1.0f);
                padEntity->bounds = padMesh->bounds;
                gfc_box_scale_local(&padEntity->bounds, padEntity->scale.x * 1.2f, padEntity->scale.y * 1.2f, padEntity->scale.z * 1.2f);
                gfc_box_translate_local(&padEntity->bounds, padEntity->position.x, padEntity->position.y, padEntity->position.z);
                padEntity->drawShadow = 0;
                padEntity->doGenericUpdate = 1;
                padEntity->think = jump_pad_think;
                padEntity->data = malloc(sizeof(float));
                if (padEntity->data) {
                    // Store spawn time in seconds
                    *(float*)padEntity->data = now;
                }
                slog("[JUMP PAD] SPAWNED: %s at (%.2f, %.2f, %.2f)", padEntity->name, padEntity->position.x, padEntity->position.y, padEntity->position.z);
                last_spawn_time = now;
            } else {
                slog("[JUMP PAD] Failed to allocate jump pad entity");
            }
        }
    } else {
        slog("Failed to load jump pad mesh");
    }
}

// Called every frame for each jump pad entity
void jump_pad_think(Entity* self) {
    if (!self || !self->_inuse) return;
    float now = SDL_GetTicks() / 1000.0f;
    if (!self->data) {
        self->data = malloc(sizeof(float));
        if (self->data) *(float*)self->data = now;
    }
    float spawn_time = self->data ? *(float*)self->data : 0.0f;
    if (now - spawn_time >= 25.0f) {
        self->_inuse = 0;
        slog("[JUMP PAD] AUTO-DESTROYED after 25s: %s at (%.2f, %.2f, %.2f)", self->name, self->position.x, self->position.y, self->position.z);
    }
}
