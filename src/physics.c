#include <string.h>
#include "physics.h"
#include "space.h"
#include "body.h"
#include "entity.h"
#include "gf3d_mesh.h"
#include "gfc_list.h"

typedef struct PhysicsBinding_S
{
    Entity* ent;
    Body*   body;
    Uint8   isStatic;
} PhysicsBinding;

static Space*   gSpace = NULL;
static GFC_List* gBindings = NULL;
static Uint8 gPhysicsEnabled = 1;

void physics_set_enabled(Uint8 enabled)
{
    if (enabled)
    {
        if (gPhysicsEnabled) return;
        gPhysicsEnabled = 1;
        physics_init();
        return;
    }

    if (!gPhysicsEnabled) return;
    gPhysicsEnabled = 0;
    physics_shutdown();
}

Uint8 physics_is_enabled(void)
{
    return gPhysicsEnabled;
}

void physics_init(void)
{
    if (!gPhysicsEnabled) return;
    if (gSpace) return;
    gSpace = space_new();
    gBindings = gfc_list_new();
}

void physics_shutdown(void)
{
    if (!gPhysicsEnabled && !gSpace && !gBindings) return;
    if (gBindings)
    {
        for (Uint32 i = 0; i < gfc_list_count(gBindings); ++i)
        {
            PhysicsBinding* pb = (PhysicsBinding*)gfc_list_nth(gBindings, i);
            if (!pb) continue;
            if (pb->body) body_free(pb->body);
            free(pb);
        }
        gfc_list_delete(gBindings);
        gBindings = NULL;
    }
    if (gSpace)
    {
        space_free(gSpace);
        gSpace = NULL;
    }
    gPhysicsEnabled = 0;
}

void physics_set_iterations(Uint32 iterations)
{
    if (!gPhysicsEnabled) return;
    if (!gSpace) physics_init();
    space_set_iterations(gSpace, iterations);
}

static GFC_Primitive physics_make_scaled_box(Mesh* mesh, GFC_Vector3D scale)
{
    GFC_Primitive prim = {0};
    prim.type = GPT_BOX;
    prim.s.b = mesh ? mesh->bounds : gfc_box(-0.5f,-0.5f,-0.5f,1,1,1);
    prim.s.b.x *= scale.x;
    prim.s.b.y *= scale.y;
    prim.s.b.z *= scale.z;
    prim.s.b.w *= scale.x;
    prim.s.b.h *= scale.y;
    prim.s.b.d *= scale.z;
    return prim;
}

void physics_register_entity(Entity* ent, Mesh* mesh, GFC_Vector3D scale, Uint8 isStatic)
{
    if (!gPhysicsEnabled) return;
    if (!ent) return;
    if (!gSpace) physics_init();
    PhysicsBinding* pb = (PhysicsBinding*)gfc_allocate_array(sizeof(PhysicsBinding), 1);
    if (!pb) return;
    pb->ent = ent;
    pb->body = body_new();
    if (!pb->body)
    {
        free(pb);
        return;
    }
    body_set_static(pb->body, isStatic ? 1 : 0);
    GFC_Primitive prim = physics_make_scaled_box(mesh, scale);
    body_add_volume(pb->body, prim);
    pb->body->position = ent->position;
    pb->body->stepPosition = ent->position;
    space_add_body(gSpace, pb->body);
    pb->isStatic = isStatic ? 1 : 0;
    gfc_list_append(gBindings, pb);
}

void physics_unregister_entity(Entity* ent)
{
    if (!gPhysicsEnabled) return;
    if (!ent || !gBindings) return;
    // linear search and remove
    for (Uint32 i = 0; i < gfc_list_count(gBindings); ++i)
    {
        PhysicsBinding* pb = (PhysicsBinding*)gfc_list_nth(gBindings, i);
        if (pb && pb->ent == ent)
        {
            if (pb->body) body_free(pb->body);
            free(pb);
            // tidying: move last into i and delete last
            Uint32 lastIndex = gfc_list_count(gBindings) - 1;
            if (i != lastIndex)
            {
                PhysicsBinding* last = (PhysicsBinding*)gfc_list_nth(gBindings, lastIndex);
                gfc_list_set_nth(gBindings, i, last);
            }
            gfc_list_delete_last(gBindings);
            return;
        }
    }
}

void physics_before_step(void)
{
    if (!gPhysicsEnabled) return;
    if (!gBindings) return;
    for (Uint32 i = 0; i < gfc_list_count(gBindings); ++i)
    {
        PhysicsBinding* pb = (PhysicsBinding*)gfc_list_nth(gBindings, i);
        if (!pb || !pb->body || !pb->ent) continue;
        pb->body->position = pb->ent->position;
        pb->body->velocity = pb->ent->velocity; // if used
    }
}

void physics_step(void)
{
    if (!gPhysicsEnabled) return;
    if (!gSpace) return;
    space_run(gSpace);
}

void physics_after_step(void)
{
    if (!gPhysicsEnabled) return;
    if (!gBindings) return;
    for (Uint32 i = 0; i < gfc_list_count(gBindings); ++i)
    {
        PhysicsBinding* pb = (PhysicsBinding*)gfc_list_nth(gBindings, i);
        if (!pb || !pb->body || !pb->ent) continue;
        pb->ent->position = pb->body->stepPosition;
        if (pb->ent->position.z < 0.0f)
        {
            pb->ent->position.z = 0.0f;
        }
    }
}
