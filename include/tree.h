#ifndef __TREE_H__
#define __TREE_H__

#include "gf3d_mesh.h"
#include "entity.h"


// Spawns all tree models side by side in the middle of the map
void tree_spawn_all(float map_min_x, float map_max_x, float map_min_y, float map_max_y, float map_floor_z);

// Update function for trees to keep bounding box matching the model
void tree_update(Entity* self);

#endif // __TREE_H__
