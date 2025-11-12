#ifndef __PROP_H__
#define __PROP_H__

#include "gfc_vector.h"
#include "gfc_color.h"
#include "gf3d_mesh.h"
#include "gf3d_texture.h"
#include "entity.h"


/**
 * @brief Spawn a static prop entity that uses the standard model shader pipeline.
 * The prop has no think/update logic; it's just rendered each frame.
 * @param mesh loaded mesh to render (required)
 * @param texture optional texture; if NULL, engine default texture is used
 * @param position initial world position
 * @param scale world scale (use gfc_vector3d(1,1,1) for unit scale)
 * @param color color modulation (use GFC_COLOR_WHITE for no tint)
 * @return pointer to Entity on success, NULL on failure
 */
Entity* prop_spawn(Mesh* mesh, Texture* texture, GFC_Vector3D position, GFC_Vector3D scale, GFC_Color color);

#endif
