#ifndef __CRATE_H__
#define __CRATE_H__

#include "entity.h"
#include "gf3d_mesh.h"
#include "gf3d_texture.h"
#include "gfc_color.h"

/**
 * @brief Spawn a crate at the given position.
 * @param position World position for the crate
 * @param scale World scale (gfc_vector3d(1,1,1) for unit scale)
 * @param color Color modulation (GFC_COLOR_WHITE for no tint)
 * @return pointer to Entity on success, NULL on failure
 */
Entity* crate_spawn(GFC_Vector3D position, GFC_Vector3D scale, GFC_Color color);

#endif
