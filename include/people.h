#ifndef __PEOPLE_H__
#define __PEOPLE_H__

#include "gfc_types.h"
#include "entity.h"


void people_spawn_all(float minX, float maxX, float minY, float maxY, float map_floor_z, Texture* tempTexture);
void people_spawn_single(GFC_Vector3D pos, Texture* texture);

#endif // __PEOPLE_H__
