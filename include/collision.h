#ifndef __COLLISION_H__
#define __COLLISION_H__

#include "gfc_vector.h"
#include "gfc_primitives.h"
#include "entity.h"

/**
 * @brief check if two bounding boxes intersect
 * @param box1 first bounding box
 * @param box2 second bounding box
 * @return 1 if they intersect, 0 otherwise
 */
Uint8 collision_box_intersect(GFC_Box box1, GFC_Box box2);

/**
 * @brief check if a point is inside a bounding box
 * @param point the point to check
 * @param box the bounding box
 * @return 1 if point is inside, 0 otherwise
 */
Uint8 collision_point_in_box(GFC_Vector3D point, GFC_Box box);

/**
 * @brief check collision between two entities
 * @param entity1 first entity
 * @param entity2 second entity
 * @return 1 if they collide, 0 otherwise
 */
Uint8 collision_entity_intersect(Entity* entity1, Entity* entity2);

/**
 * @brief update entity bounding box based on position and scale
 * @param entity the entity to update bounds for
 */
void collision_update_entity_bounds(Entity* entity);

/**
 * @brief check if entity is within world boundaries
 * @param entity the entity to check
 * @param worldMin minimum world coordinates
 * @param worldMax maximum world coordinates
 * @return 1 if within bounds, 0 if outside
 */
Uint8 collision_entity_in_world_bounds(Entity* entity, GFC_Vector3D worldMin, GFC_Vector3D worldMax);

#endif