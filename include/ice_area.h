#ifndef __ICE_AREA_H__
#define __ICE_AREA_H__

#include "entity.h"

// Data for an ice/sticky area
typedef struct {
    float spawn_time;
    float duration;
    float rise_height;
    int   active;
} IceAreaData;

// Spawns an ice area at a random position/size
void ice_area_spawn_random();

// Call every frame to update all ice areas
void ice_area_think(Entity* self);

#endif
