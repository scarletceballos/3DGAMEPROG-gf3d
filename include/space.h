#ifndef __SPACE_H__
#define __SPACE_H__

#include "gfc_list.h"
#include "gfc_types.h"

typedef struct Body_S Body; // forward declare

typedef struct Space_S
{
    GFC_List* staticMeshes;  // optional: list of meshes or geometry
    GFC_List* bodies;        // dynamic bodies
    GFC_List* staticBodies;  // static bodies (non-moving)
    Uint32    iterations;    // physics substeps per frame
    float     step;          // 1.0f / iterations
} Space;

Space* space_new(void);
void   space_free(Space* space);

void   space_set_iterations(Space* space, Uint32 iterations);
void   space_run(Space* space);
void   space_step(Space* space);

void   space_add_body(Space* space, Body* body);

#endif