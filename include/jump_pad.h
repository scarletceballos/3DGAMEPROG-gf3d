#ifndef __JUMP_PAD_H__
#define __JUMP_PAD_H__

#include "gf3d_mesh.h"

#include "gf3d_texture.h"
#include "entity.h"


void jump_pad_spawn_all(float minX, float maxX, float minY, float maxY, float map_floor_z, Texture* texture);
void jump_pad_think(Entity* self);

#endif
