#ifndef __MONSTER_H__
#define __MONSTER_H__

#include "entity.h"
#include "gfc_vector.h"
#include "gfc_types.h"

typedef struct
{
    Entity* entity;
} Monster;

/**
 * @brief initialize the monster system
 * @param max_monsters maximum number of monsters to work with
 */
void monster_system_init(Uint32 max_monsters);

/**
 * @brief create a new monster
 * @param mesh the 3D mesh for the monster
 * @param texture the texture for the monster
 * @param position starting position
 * @return pointer to new monster or NULL on error
 */
Monster* monster_new(Mesh* mesh, Texture* texture, GFC_Vector3D position);

/**
 * @brief free a monster
 * @param monster the monster to free
 */
void monster_free(Monster* monster);

#endif