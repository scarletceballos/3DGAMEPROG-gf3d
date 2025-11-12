#include "ice_area.h"
#include "simple_logger.h"
#include <stdlib.h>
#include <time.h>

#define ICE_AREA_MIN_SIZE 8.0f
#define ICE_AREA_MAX_SIZE 32.0f
#define ICE_AREA_MIN_DURATION 6.0f
#define ICE_AREA_MAX_DURATION 14.0f
#define ICE_AREA_RISE_SPEED 0.08f
#define ICE_AREA_ALPHA 0.45f

void ice_area_think(Entity* self) {
    if (!self || !self->data) return;
    IceAreaData* data = (IceAreaData*)self->data;
    float now = SDL_GetTicks() / 1000.0f;
    float t = now - data->spawn_time;
    if (t > data->duration) {
        self->_inuse = 0;
        return;
    }
    // Slowly rise up to max height (1.5 units)
    float max_rise = 1.5f;
    float rise = t * ICE_AREA_RISE_SPEED;
    if (rise > max_rise) rise = max_rise;
    data->rise_height = rise;
    self->position.z = data->rise_height;
    // Color: semi-transparent blue
    self->color = gfc_color(0.2f, 0.6f, 1.0f, ICE_AREA_ALPHA);
}

void ice_area_spawn_random() {
    Entity* ent = entity_new();
    if (!ent) return;
    gfc_line_cpy(ent->name, "ice_area");
    float size = ICE_AREA_MIN_SIZE + ((float)rand() / RAND_MAX) * (ICE_AREA_MAX_SIZE - ICE_AREA_MIN_SIZE);
    float x = g_map_min_x + ((float)rand() / RAND_MAX) * (g_map_max_x - g_map_min_x - size);
    float y = g_map_min_y + ((float)rand() / RAND_MAX) * (g_map_max_y - g_map_min_y - size);
    ent->position = gfc_vector3d(x + size/2, y + size/2, 0);
    ent->scale = gfc_vector3d(size, size, 0.2f);
    ent->rotation = gfc_vector3d(0,0,0);
    ent->color = gfc_color(0.2f, 0.6f, 1.0f, ICE_AREA_ALPHA);
    ent->drawShadow = 0;
    ent->think = ice_area_think;
    ent->doGenericUpdate = 1;
    ent->static_bounds = 1;
    ent->mesh = NULL; // Will draw as a rectangle/quad in game.c
    IceAreaData* data = calloc(1, sizeof(IceAreaData));
    data->spawn_time = SDL_GetTicks() / 1000.0f;
    data->duration = ICE_AREA_MIN_DURATION + ((float)rand() / RAND_MAX) * (ICE_AREA_MAX_DURATION - ICE_AREA_MIN_DURATION);
    data->rise_height = 0;
    data->active = 1;
    ent->data = data;
}
