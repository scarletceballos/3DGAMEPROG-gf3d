#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include "gfc_types.h"
#include "gfc_vector.h"
#include "entity.h"
#include "gf3d_mesh.h"

void physics_init(void);
void physics_shutdown(void);
void physics_set_iterations(Uint32 iterations);
void physics_set_enabled(Uint8 enabled);
Uint8 physics_is_enabled(void);

void physics_register_entity(Entity* ent, Mesh* mesh, GFC_Vector3D scale, Uint8 isStatic);
void physics_unregister_entity(Entity* ent);

void physics_before_step(void);
void physics_step(void);
void physics_after_step(void);

#endif