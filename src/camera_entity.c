#include "simple_logger.h"
#include "gfc_input.h"
#include "gf3d_camera.h"
#include "camera_entity.h"
#include "entity.h"
#include "gfc_types.h"
#include "gfc_vector.h"
#include "gfc_matrix.h"

// global camera entity reference
static Entity* g_camera_entity = NULL;

typedef struct {
    Entity* target;
    float followHeight;
    float followDistance;
    float angle;
} CameraEntityData;

void camera_entity_free(Entity *self) {
    CameraEntityData *data;
    if ((!self) || (!self->data)) return; 
    data = (CameraEntityData*)self->data;
    free(data);
    if (g_camera_entity == self) {
        g_camera_entity = NULL;
    }
}

void camera_entity_think(Entity *self) {
    CameraEntityData *data;
    
    if ((!self) || (!self->data)) return;
    data = (CameraEntityData*)self->data;
    if (!data->target) return;
    
    GFC_Vector3D newPos = gfc_vector3d(0, 0, data->followHeight); // Center at origin, high up (or should be)
    gfc_vector3d_copy(self->position, newPos);
    
    GFC_Vector3D lookTarget = gfc_vector3d(0, 0, 0); // Look at ground center
    gf3d_camera_set_position(self->position);
    gf3d_camera_look_at(lookTarget, &self->position);
}

void camera_entity_set_target(Entity* camera_entity, Entity* target) {
    CameraEntityData *data;
    if ((!camera_entity) || (!camera_entity->data)) return;
    data = (CameraEntityData*)camera_entity->data;
    data->target = target;
}

Entity* camera_entity_get() {
    return g_camera_entity;
}

Entity* camera_entity_spawn(GFC_Vector3D position, Entity* target) {
    CameraEntityData *data;
    Entity *self;
    
    self = entity_new();
    if (!self) return NULL;
    
    data = (CameraEntityData*)malloc(sizeof(CameraEntityData));
    if (!data) {
        entity_free(self);
        return NULL;
    }
    
    // Initialize camera entity
    self->data = data;
    gfc_vector3d_copy(self->position, position);
    self->think = camera_entity_think;
    self->free = camera_entity_free;
    gfc_line_cpy(self->name, "camera");
    
    // Initialize camera data
    data->target = target;
    data->followHeight = 50.0f;  // Height even higher simply for entity testing
    data->followDistance = 35.0f; // Not used/needed for overhead angle
    data->angle = 0.0f;
    
    // Set as global camera entity
    g_camera_entity = self;
    
    slog("Camera entity created following target");
    return self;
}

void camera_entity_set_follow_params(Entity* camera_entity, float height, float distance, float angle) {
    CameraEntityData *data;
    if ((!camera_entity) || (!camera_entity->data)) return;
    data = (CameraEntityData*)camera_entity->data;
    data->followHeight = height;
    data->followDistance = distance;
    data->angle = angle;
}

float camera_entity_get_angle(Entity* camera_entity) {
    CameraEntityData *data;
    if ((!camera_entity) || (!camera_entity->data)) return 0.0f;
    data = (CameraEntityData*)camera_entity->data;
    return data->angle;
}

void camera_entity_adjust_angle(Entity* camera_entity, float angle_delta) {
    CameraEntityData *data;
    if ((!camera_entity) || (!camera_entity->data)) return;
    data = (CameraEntityData*)camera_entity->data;
    data->angle += angle_delta;
    // Keep angle in range [0, 2*PI]
    while (data->angle < 0) data->angle += 2 * GFC_PI;
    while (data->angle >= 2 * GFC_PI) data->angle -= 2 * GFC_PI;
}