#ifndef __CAMERA_ENTITY_H__
#define __CAMERA_ENTITY_H__

#include "entity.h"
#include "gfc_vector.h"
#include "gfc_types.h"

/**
 * @brief create a new camera entity that follows a target
 * @param position initial camera position
 * @param target the entity to follow
 * @return pointer to new camera entity or NULL on error
 */
Entity* camera_entity_spawn(GFC_Vector3D position, Entity* target);

/**
 * @brief set the target entity for the camera to follow
 * @param camera_entity the camera entity
 * @param target the entity to follow
 */
void camera_entity_set_target(Entity* camera_entity, Entity* target);

/**
 * @brief get the current camera entity
 * @return pointer to camera entity or NULL if none exists
 */
Entity* camera_entity_get();

/**
 * @brief set camera follow parameters
 * @param camera_entity the camera entity to modify
 * @param height height offset from target
 * @param distance distance offset from target
 * @param angle rotation angle around target
 */
void camera_entity_set_follow_params(Entity* camera_entity, float height, float distance, float angle);

/**
 * @brief get camera follow angle for rotation controls
 * @param camera_entity the camera entity
 * @return current follow angle in radians
 */
float camera_entity_get_angle(Entity* camera_entity);

/**
 * @brief adjust camera follow angle (for left/right rotation)
 * @param camera_entity the camera entity
 * @param angle_delta amount to add to current angle
 */
void camera_entity_adjust_angle(Entity* camera_entity, float angle_delta);

/**
 * @brief adjust camera follow height (for up/down movement only)
 * @param camera_entity the camera entity
 * @param height_delta amount to add to current height
 */
void camera_entity_adjust_height(Entity* camera_entity, float height_delta);

/**
 * @brief adjust camera follow distance (for zoom in/out)
 * @param camera_entity the camera entity
 * @param distance_delta amount to add to current distance
 */
void camera_entity_adjust_distance(Entity* camera_entity, float distance_delta);

#endif