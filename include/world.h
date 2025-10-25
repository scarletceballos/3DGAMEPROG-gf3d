#ifndef __WORLD_H__
#define __WORLD_H__

#include "gfc_types.h"
#include "gfc_vector.h"
#include "gfc_color.h"
#include "gfc_list.h"
#include "gf3d_mesh.h"
#include "gf3d_texture.h"

typedef struct World_S
{
	Mesh* terrain;
	Texture* texture;
	GFC_List* entities;
	GFC_Color lightcolor;
	GFC_Vector3D lightpos;
} World;

/**
 * @brief create a new world
 * @return NULL on error, a pointer to a new world otherwise
 */
World* world_new();

/**
 * @brief load a world from a definition file
 * @param filename the world definition file to load
 * @return NULL on error, a pointer to the loaded world otherwise
 */
World* world_load(const char* filename);

/**
 * @brief free a world and all its resources
 * @param world the world to free
 */
void world_free(World* world);

/**
 * @brief draw a world with all its entities
 * @param world the world to draw
 */
void world_draw(World* world);

#endif

