#ifndef __ENTITY_H__
#define __ENTITY_H__

#include "gfc_text.h"
#include "gfc_vector.h"
#include "gfc_matrix.h"
#include "gfc_color.h"
#include "gfc_primitives.h"
#include "gfc_types.h"

#include "gf3d_texture.h"
#include "gf3d_mesh.h"

typedef struct Entity_S
{
    Uint8           _inuse;
    GFC_TextLine    name;
    Mesh*           mesh;
    Texture*        texture;
    GFC_Color       color;
    GFC_Matrix4     matrix;
    GFC_Vector3D    position;
    GFC_Vector3D    rotation;
    GFC_Vector3D    scale;
    GFC_Box         bounds;
    void           (*draw)(struct Entity_S* self);
    void           (*think)(struct Entity_S* self);
    void           (*update)(struct Entity_S* self);

} Entity;


/**
 * @brief get a pointer to a new blank entity
 * @return NULL on out of memory or other error, a pointer to a blank entity otherwise
 */
Entity* entity_new();

/**
 * @brief free a previously new'd entity
 * @param ent the entity to be freed
 * @note the memory address should no longer be used
 */
void entity_free(Entity* ent);

/**
 * @brief initializes the entity subsystem
 * @param max_ents how many to support concurrently
 */
void entity_system_init(Uint32 max_ents);


/*
@brief draw all entities with provided light
@param lightPos where light is in world space
@param lightColor color of the light
*/
void entity_system_draw_all(GFC_Vector3D lightPos, GFC_Color lightColor);

void entity_system_think_all();

void entity_system_update_all();

#endif
