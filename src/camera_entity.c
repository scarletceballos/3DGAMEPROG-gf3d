#include <math.h>
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
    // Orbit around the target using angle and followDistance; Z is up.
    // angle=0 points camera behind target along -Y, matching initial spawn (0,-20,10).
    float xoff = sinf(data->angle) * data->followDistance;
    float yoff = -cosf(data->angle) * data->followDistance;
    GFC_Vector3D desired = gfc_vector3d(
        data->target->position.x + xoff,
        data->target->position.y + yoff,
        data->target->position.z + data->followHeight
    );
    gfc_vector3d_copy(self->position, desired);

    // Look at the target's current position
    GFC_Vector3D lookTarget = data->target->position;
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
    data->followHeight = 10.0f;   // Slightly above target
    data->followDistance = 20.0f; // Back a bit from the target
    data->angle = 0.0f;           // Facing along -Y initially
    
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

void camera_entity_adjust_height(Entity* camera_entity, float height_delta) {
    CameraEntityData *data;
    if ((!camera_entity) || (!camera_entity->data)) return;
    data = (CameraEntityData*)camera_entity->data;
    data->followHeight += height_delta;
    if (data->followHeight < 2.0f) data->followHeight = 2.0f;
    if (data->followHeight > 50.0f) data->followHeight = 50.0f;
}

void camera_entity_adjust_distance(Entity* camera_entity, float distance_delta) {
    CameraEntityData *data;
    if ((!camera_entity) || (!camera_entity->data)) return;
    data = (CameraEntityData*)camera_entity->data;
    data->followDistance += distance_delta;
    if (data->followDistance < 3.0f) data->followDistance = 3.0f;
    if (data->followDistance > 100.0f) data->followDistance = 100.0f;
}