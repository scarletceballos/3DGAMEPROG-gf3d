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

// Forward declarations to avoid circular dependencies
typedef struct World_S World;

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
    GFC_Vector3D    velocity;
    Uint8           drawShadow;

    
    GFC_Box         bounds;
    void           (*draw)(struct Entity_S* self);
    void           (*think)(struct Entity_S* self);
    void           (*update)(struct Entity_S* self);
    void           (*free)(struct Entity_S* self);
    Uint8           doGenericUpdate;
    void            *data;

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

/**
 * @brief draw an entity with lighting
 * @param ent the entity to draw
 * @param lightPos position of light source
 * @param lightColor color of the light
 */
void entity_draw(Entity* ent, GFC_Vector3D lightPos, GFC_Color lightColor);

/**
 * @brief draw entity shadow
 * @param ent the entity to draw shadow for
 */
void entity_draw_shadow(Entity *ent);

/**
 * @brief get floor position for entity collision
 * @param entity the entity to check
 * @param world the world to check against
 * @param contact output contact point
 * @return 1 if floor found, 0 otherwise
 */
Uint8 entity_get_floor_position(Entity *entity, World *world, GFC_Vector3D *contact);

#endif
