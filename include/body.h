#ifndef __BODY_H__
#define __BODY_H__

#include "gfc_text.h"
#include "gfc_list.h"
#include "gfc_primitives.h"
#include "gfc_vector.h"
#include "gfc_types.h"

typedef struct Body_S Body;

typedef void body_collide_func(Body* self, Body* other, void* data);

struct Body_S
{
    GFC_TextLine  name;
    GFC_Vector3D  position;      // center of mass
    GFC_Vector3D  velocity;      // current velocity
    GFC_List*     volumes;       // list of GFC_Primitive*
    body_collide_func* onCollide;
    void*         data;          // user data for callback

    GFC_Vector3D  stepPosition;  // position used for this step
    GFC_Vector3D  stepVelocity;  // velocity scaled by step factor

    Uint8         stopped;       // stop iterating on this body
    Uint8         isStatic;      // static bodies do not move in resolution
};

Body* body_new(void);
void  body_free(Body* b);

void  body_reset_for_updates(Body* b, float factor);
void  body_add_volume(Body* b, GFC_Primitive v);
void  body_set_collision(Body* b, body_collide_func* collide, void* data);
void  body_set_static(Body* b, Uint8 isStatic);

int   body_test_body(Body* a, Body* b);

#endif