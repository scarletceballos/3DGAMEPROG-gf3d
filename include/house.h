#ifndef __HOUSE_H__
#define __HOUSE_H__

#include "gfc_types.h"
#include "entity.h"


void house_spawn_all(float minX, float maxX, float minY, float maxY, float map_floor_z, Texture* tempTexture);
void house_spawn_single(GFC_Vector3D pos, Texture* texture);

#endif // __HOUSE_H__
