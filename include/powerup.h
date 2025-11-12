#ifndef __POWERUP_H__
#define __POWERUP_H__

#include "gf3d_texture.h"
#include "gf3d_mesh.h"
#include "entity.h"


// Power-up types
typedef enum {
    POWERUP_NONE = 0,
    POWERUP_SLAP,
    POWERUP_CHAIN,
    POWERUP_THUNDER,
    POWERUP_GOLDEN,
    POWERUP_SLOW,
    POWERUP_SPEED,
    POWERUP_MAX
} PowerupType;

// Power-up entity data
typedef struct {
    PowerupType type;
    float spawn_time;
    int active;
} PowerupData;

// Spawns a power-up of the given type in front of the player
void powerup_spawn_near_player(Entity* player, float map_floor_z, Texture* texture, PowerupType type);


// void powerup_spawn_center(float centerX, float centerY, float map_floor_z, Texture* texture, PowerupType type);
void powerup_spawn_random(float minX, float maxX, float minY, float maxY, float map_floor_z, Texture* texture);
void powerup_think(Entity* self);

#endif
