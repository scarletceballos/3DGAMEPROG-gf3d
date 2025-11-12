#include <math.h>
#include "collision.h"
#include "simple_logger.h"

Uint8 collision_box_intersect(GFC_Box box1, GFC_Box box2)
{
    // Check if boxes don't overlap on any axis
    if (box1.x + box1.w < box2.x || box2.x + box2.w < box1.x) return 0; // X axis
    if (box1.y + box1.h < box2.y || box2.y + box2.h < box1.y) return 0; // Y axis
    if (box1.z + box1.d < box2.z || box2.z + box2.d < box1.z) return 0; // Z axis

    // Debug: log when any two boxes overlap
    slog("[COLLISION] box1=[%.2f %.2f %.2f %.2f %.2f %.2f] box2=[%.2f %.2f %.2f %.2f %.2f %.2f]", box1.x, box1.y, box1.z, box1.w, box1.h, box1.d, box2.x, box2.y, box2.z, box2.w, box2.h, box2.d);
    return 1; // They overlap on all axes
}

Uint8 collision_point_in_box(GFC_Vector3D point, GFC_Box box)
{
    if (point.x >= box.x && point.x <= box.x + box.w &&
        point.y >= box.y && point.y <= box.y + box.h &&
        point.z >= box.z && point.z <= box.z + box.d)
    {
        return 1;
    }
    return 0;
}

Uint8 collision_entity_intersect(Entity* entity1, Entity* entity2)
{
    if (!entity1 || !entity2) return 0;
    if (!entity1->_inuse || !entity2->_inuse) return 0;
    // Update bounds for both entities
    collision_update_entity_bounds(entity1);
    collision_update_entity_bounds(entity2);
    //slog("[COLLISION CHECK] %s vs %s | bounds1=[%.2f %.2f %.2f %.2f %.2f %.2f] pos1=(%.2f %.2f %.2f) | bounds2=[%.2f %.2f %.2f %.2f %.2f %.2f] pos2=(%.2f %.2f %.2f)",
        // entity1->name, entity2->name,
        // entity1->bounds.x, entity1->bounds.y, entity1->bounds.z, entity1->bounds.w, entity1->bounds.h, entity1->bounds.d,
        // entity1->position.x, entity1->position.y, entity1->position.z,
        // entity2->bounds.x, entity2->bounds.y, entity2->bounds.z, entity2->bounds.w, entity2->bounds.h, entity2->bounds.d,
        // entity2->position.x, entity2->position.y, entity2->position.z);
    return collision_box_intersect(entity1->bounds, entity2->bounds);
}

void collision_update_entity_bounds(Entity* entity)
{
    if (!entity) return;
    if (entity->static_bounds) return; // Do not overwrite bounds if static_bounds is set
    // Create bounding box based on entity position and scale
    // Assuming a basic cube with scale determining size
    float halfWidth = entity->scale.x * 0.5f;
    float halfHeight = entity->scale.y * 0.5f;
    float halfDepth = entity->scale.z * 0.5f;
    entity->bounds.x = entity->position.x - halfWidth;
    entity->bounds.y = entity->position.y - halfHeight;
    entity->bounds.z = entity->position.z - halfDepth;
    entity->bounds.w = entity->scale.x;
    entity->bounds.h = entity->scale.y;
    entity->bounds.d = entity->scale.z;
}

Uint8 collision_entity_in_world_bounds(Entity* entity, GFC_Vector3D worldMin, GFC_Vector3D worldMax)
{
    if (!entity) return 0;
    
    collision_update_entity_bounds(entity);
    
    // Check if entity is completely within world boundaries
    if (entity->bounds.x >= worldMin.x && 
        entity->bounds.y >= worldMin.y && 
        entity->bounds.z >= worldMin.z &&
        entity->bounds.x + entity->bounds.w <= worldMax.x &&
        entity->bounds.y + entity->bounds.h <= worldMax.y &&
        entity->bounds.z + entity->bounds.d <= worldMax.z)
    {
        return 1;
    }
    
    return 0;
}

// Basic collision response - separate entities that are overlapping
void collision_resolve_entity_overlap(Entity* entity1, Entity* entity2)
{
    if (!collision_entity_intersect(entity1, entity2)) return;
    
    // Calculate separation vector
    GFC_Vector3D center1 = gfc_vector3d(
        entity1->position.x,
        entity1->position.y,
        entity1->position.z
    );
    
    GFC_Vector3D center2 = gfc_vector3d(
        entity2->position.x,
        entity2->position.y,
        entity2->position.z
    );
    
    GFC_Vector3D separation;
    gfc_vector3d_sub(separation, center1, center2);
    
    float distance = gfc_vector3d_magnitude(separation);
    if (distance > 0)
    {
        gfc_vector3d_normalize(&separation);
        
        // Move entities apart by half the overlap distance each
        float minSeparation = (entity1->scale.x + entity2->scale.x) * 0.6f;
        float pushDistance = (minSeparation - distance) * 0.5f;
        
        GFC_Vector3D push;
        gfc_vector3d_scale(push, separation, pushDistance);
        
        gfc_vector3d_add(entity1->position, entity1->position, push);
        gfc_vector3d_sub(entity2->position, entity2->position, push);
        
        slog("Collision resolved between entities");
    }
}