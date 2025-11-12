#include "crate.h"
#include "simple_logger.h"
#include "physics.h"
#include "powerup.h"
#include <stdlib.h>

// Crate entity data
typedef struct {
    int is_broken;
} CrateData;

static void crate_free(Entity* self) {
    if (!self) return;
    physics_unregister_entity(self);
    if (self->data) free(self->data);
}

// Pass world pointer to crate_think for correct floor queries
extern World* g_world_ptr;
static void crate_think(Entity* self) {
    if (!self || !self->data) return;
    CrateData* data = (CrateData*)self->data;
    // Always update bounds for debug and easier collision
    if (self->mesh) {
        self->bounds = self->mesh->bounds; // Reset to original mesh bounds
        float expand = 1.2f;
        gfc_box_scale_local(&self->bounds, self->scale.x * expand, self->scale.y * expand, self->scale.z * expand);
        gfc_box_translate_local(&self->bounds, self->position.x, self->position.y, self->position.z);
    }
    if (data->is_broken == 2) return;
    extern int g_dino_attack_active;
    slog("[CRATE DEBUG] think: g_dino_attack_active=%d", g_dino_attack_active);
    if (!g_dino_attack_active) return;
    Entity* ents = entity_system_get_all_entities();
    Uint32 ent_max = entity_system_get_count();
    for (Uint32 i = 0; i < ent_max; ++i) {
        if (!ents[i]._inuse) continue;
        slog("[CRATE DEBUG] checking entity %d: name=%s", i, ents[i].name);
        if (strncmp(ents[i].name, "monster", 7) == 0) {
            slog("[CRATE DEBUG] found monster entity %d: pos=(%.2f, %.2f, %.2f)", i, ents[i].position.x, ents[i].position.y, ents[i].position.z);
            if (gfc_box_overlap(self->bounds, ents[i].bounds)) {
                slog("[CRATE DEBUG] overlap detected with monster entity %d (crate bounds: [%.2f %.2f %.2f %.2f %.2f %.2f], monster bounds: [%.2f %.2f %.2f %.2f %.2f %.2f])", i, self->bounds.x, self->bounds.y, self->bounds.z, self->bounds.w, self->bounds.h, self->bounds.d, ents[i].bounds.x, ents[i].bounds.y, ents[i].bounds.z, ents[i].bounds.w, ents[i].bounds.h, ents[i].bounds.d);
                // Only run break logic if is_broken is set (by player attack or other means)
                if (data->is_broken == 1) {
                    if (rand() % 2 == 0) {
                        Texture* powerupTexture = gf3d_texture_load("images/default.png");
                        PowerupType type = (PowerupType)(1 + rand() % (POWERUP_MAX - 1));
                        Mesh* mesh = gf3d_mesh_load("models/primitives/cube.obj");
                        Entity* powerup = entity_new();
                        if (powerup && mesh) {
                            gfc_line_cpy(powerup->name, "powerup");
                            powerup->mesh = mesh;
                            powerup->texture = powerupTexture;
                            powerup->scale = gfc_vector3d(0.8f, 0.8f, 0.8f);
                            // Place powerup at crate's (x, y), clamp Z to floor using entity_get_floor_position
                            GFC_Vector3D powerup_pos = self->position;
                            GFC_Vector3D floorContact = {0};
                            float powerup_z = powerup_pos.z;
                            Entity tempPowerupEntity = {0};
                            tempPowerupEntity.position = gfc_vector3d(powerup_pos.x, powerup_pos.y, 0);
                            tempPowerupEntity.scale = gfc_vector3d(0.8f, 0.8f, 0.8f);
                            if (entity_get_floor_position(&tempPowerupEntity, g_world_ptr, &floorContact)) {
                                powerup_z = floorContact.z + (mesh->bounds.d * 0.8f * 0.5f) + 0.06f;
                            }
                            powerup->position = gfc_vector3d(powerup_pos.x, powerup_pos.y, powerup_z);
                            powerup->rotation = gfc_vector3d(0, 0, 0);
                            PowerupData* data = calloc(1, sizeof(PowerupData));
                            if (data) {
                                data->type = type;
                                data->spawn_time = SDL_GetTicks() / 1000.0f;
                                data->active = 1;
                                powerup->data = data;
                            }
                            switch (type) {
                                case POWERUP_SLAP:    powerup->color = gfc_color(1.0f, 0.5f, 0.0f, 1.0f); break;
                                case POWERUP_CHAIN:   powerup->color = gfc_color(1.0f, 0.0f, 0.0f, 1.0f); break;
                                case POWERUP_THUNDER: powerup->color = gfc_color(0.2f, 0.4f, 1.0f, 1.0f); break;
                                case POWERUP_GOLDEN:  powerup->color = gfc_color(1.0f, 0.7f, 0.2f, 1.0f); break;
                                case POWERUP_SLOW:    powerup->color = gfc_color(0.5f, 0.5f, 0.5f, 1.0f); break;
                                case POWERUP_SPEED:   powerup->color = gfc_color(0.0f, 1.0f, 0.3f, 1.0f); break;
                                default:              powerup->color = gfc_color(1, 1, 1, 1); break;
                            }
                            powerup->bounds = mesh->bounds;
                            gfc_box_scale_local(&powerup->bounds, powerup->scale.x, powerup->scale.y, powerup->scale.z);
                            gfc_box_translate_local(&powerup->bounds, powerup->position.x, powerup->position.y, powerup->position.z);
                            powerup->drawShadow = 0;
                            powerup->doGenericUpdate = 1;
                            powerup->think = powerup_think;
                            powerup->static_bounds = 0;
                            slog("[CRATE] Powerup spawned at (%.2f, %.2f, %.2f)", powerup->position.x, powerup->position.y, powerup->position.z);
                        }
                        slog("[CRATE] Broken! Spawned powerup type %d", type);
                    } else {
                        float dx = ents[i].position.x - self->position.x;
                        float dy = ents[i].position.y - self->position.y;
                        float len = sqrtf(dx*dx + dy*dy);
                        if (len < 0.01f) len = 0.01f;
                        dx /= len; dy /= len;
                        float knockback = 8.0f; // heavy knockback
                        ents[i].velocity.x += dx * knockback;
                        ents[i].velocity.y += dy * knockback;
                        slog("[CRATE] Bounced player with heavy knockback");
                    }
                    data->is_broken = 2;
                    self->_inuse = 0;
                    return;
                }
            }
        }
    }
}

Entity* crate_spawn(GFC_Vector3D position, GFC_Vector3D scale, GFC_Color color) {
    Mesh* mesh = gf3d_mesh_load("models/primitives/cube.obj");
    Texture* texture = gf3d_texture_load("images/default.png");
    if (!mesh) return NULL;
    Entity* ent = entity_new();
    if (!ent) return NULL;
    gfc_line_cpy(ent->name, "crate");
    ent->mesh = mesh;
    ent->texture = texture;
    ent->scale = scale;
    ent->rotation = gfc_vector3d(0,0,0);
    // Use a classic brown for wood crate
    ent->color = gfc_color(0.59f, 0.29f, 0.0f, 1.0f);
    ent->drawShadow = 0;
    ent->think = crate_think;
    ent->update = NULL;
    ent->free = crate_free;
    ent->doGenericUpdate = 1;
    ent->static_bounds = 0;
    // Compute Z like powerup: padZ = map_floor_z + (mesh->bounds.d * scale * 0.5f) + 0.06f
    extern float g_map_floor_z;
    float padZ = g_map_floor_z + (mesh->bounds.d * scale.z * 0.5f) + 0.06f;
    ent->position = gfc_vector3d(position.x, position.y, padZ);
    // Set bounds to match the full mesh, scaled and centered at the crate's position
    ent->bounds = mesh->bounds;
    // Expand bounds by 20% for easier hit detection
    float expand = 1.2f;
    gfc_box_scale_local(&ent->bounds, scale.x * expand, scale.y * expand, scale.z * expand);
    gfc_box_translate_local(&ent->bounds, ent->position.x, ent->position.y, ent->position.z);
    CrateData* data = calloc(1, sizeof(CrateData));
    ent->data = data;
    // Register in physics
    physics_register_entity(ent, mesh, scale, 1);
    return ent;
}
