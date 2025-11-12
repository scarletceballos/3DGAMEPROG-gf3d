#include "gf3d_draw.h"
#include "gfc_primitives.h"
#include "gfc_color.h"
#include "gfc_vector.h"
#include "gfc_matrix.h"
#include "simple_logger.h"

// Draw solid cube here
#include "gf3d_mesh.h"

static Mesh* _gf3d_draw_unit_cube = NULL;

// Helper to create a unit cube mesh in memory (if not already loaded)
static void _gf3d_draw_ensure_unit_cube() {
    if (_gf3d_draw_unit_cube) return;
    // Use the existing cube.obj asset for bounding box visualization
    _gf3d_draw_unit_cube = gf3d_mesh_load("models/cube.obj");
    if (!_gf3d_draw_unit_cube) {
        slog("[gf3d_draw] WARNING: Could not load cube.obj, bounding box will not be visible!");
    }
}

void gf3d_draw_cube_solid(GFC_Box cube, GFC_Vector3D position, GFC_Vector3D rotation, GFC_Vector3D scale, GFC_Color color) {
    _gf3d_draw_ensure_unit_cube();
    if (!_gf3d_draw_unit_cube) return;


    // For cube.obj from (0,0,0) to (1,1,1):
    // Scale to (w, h, d), then translate to (x + w/2, y + h/2, z + d/2) to center
    GFC_Vector3D box_scale = { cube.w, cube.h, cube.d };
    GFC_Vector3D box_center = {
        cube.x + cube.w * 0.5f,
        cube.y + cube.h * 0.5f,
        cube.z + cube.d * 0.5f
    };

    GFC_Matrix4 modelMat;
    gfc_matrix4_identity(modelMat);
    gfc_matrix4_scale(modelMat, modelMat, box_scale);
    gfc_matrix4_translate(modelMat, modelMat, box_center);

    // Always use solid red, 40% transparent
    GFC_Color box_color = gfc_color(1.0f, 0.0f, 0.0f, 0.4f);

    gf3d_mesh_draw(
        _gf3d_draw_unit_cube,
        modelMat,
        box_color,
        NULL, // no texture
        gfc_vector3d(0,0,0), // no lighting
        gfc_color(0,0,0,0)
    );
}
