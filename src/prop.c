#include "prop.h"
#include "simple_logger.h"
#include "physics.h"

static void prop_free(Entity* self)
{
    if (!self) return;
    physics_unregister_entity(self);
}

Entity* prop_spawn(Mesh* mesh, Texture* texture, GFC_Vector3D position, GFC_Vector3D scale, GFC_Color color)
{
    if (!mesh)
    {
        slog("prop_spawn: mesh is required");
        return NULL;
    }
    Entity* ent = entity_new();
    if (!ent) return NULL;

    ent->mesh = mesh;
    ent->texture = texture; // may be NULL; default texture used in mesh draw
    ent->position = position;
    ent->rotation = gfc_vector3d(0,0,0);
    if ((scale.x == 0) && (scale.y == 0) && (scale.z == 0))
    {
        ent->scale = gfc_vector3d(1,1,1);
    }
    else
    {
        ent->scale = scale;
    }
    ent->color = color;
    ent->drawShadow = 0;
    ent->think = NULL;
    ent->update = NULL;
    ent->free = prop_free;
    ent->doGenericUpdate = 0;

    // Register static prop in physics space
    physics_register_entity(ent, mesh, ent->scale, 1);
    return ent;
}
