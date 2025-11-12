#include "people.h"
#include "simple_logger.h"
#include <stdlib.h>

void people_spawn_all(float minX, float maxX, float minY, float maxY, float map_floor_z, Texture* tempTexture) {
    Mesh* guyMesh = gf3d_mesh_load("models/people/guy.obj");
    if (guyMesh) {
        // Calculate Z offset so feet are on the floor
        float meshMinZ = guyMesh->bounds.z;
        float meshMaxZ = guyMesh->bounds.z + guyMesh->bounds.h;
        float meshHeight = meshMaxZ - meshMinZ;
        // Center guy uses scale 8, others use scale 2
        float centerScale = 8.0f;
        float otherScale = 2.0f;
        extern float g_map_floor_z; // Use the top face of the map as the floor
        float centerZOffset = g_map_floor_z - (meshMinZ * centerScale);
        float otherZOffset = g_map_floor_z - (meshMinZ * otherScale);

        // No center guy; only spawn random guys
        // Spawn all guys randomly, with color/size variety and improved collision
        for (int i = 0; i < 10; ++i) {
            float randX = minX + ((float)rand() / RAND_MAX) * (maxX - minX);
            float randY = minY + ((float)rand() / RAND_MAX) * (maxY - minY);
            Entity* guyEntity = entity_new();
            if (guyEntity) {
                char name[16];
                snprintf(name, sizeof(name), "guy%d", i+1);
                gfc_line_cpy(guyEntity->name, name);
                guyEntity->mesh = guyMesh;
                guyEntity->texture = tempTexture;
                // Set all guys to the same default scale for consistency
                float scale = otherScale * 2.0f;
                guyEntity->scale = gfc_vector3d(scale, scale, scale);
                guyEntity->position = gfc_vector3d(randX, randY, g_map_floor_z - (meshMinZ * scale));
                guyEntity->rotation = gfc_vector3d(0, 0, 0);
                // Color variety: red, green, blue
                GFC_Color colors[3] = {gfc_color(1,0.3f,0.3f,1), gfc_color(0.3f,1,0.3f,1), gfc_color(0.3f,0.3f,1,1)};
                guyEntity->color = colors[i % 3];
                guyEntity->bounds = guyMesh->bounds;
                // Make the bounding box larger and taller for better collision
                float boxScale = 1.35f + 0.15f * i; // 1.35, 1.5, 1.65
                gfc_box_scale_local(&guyEntity->bounds, guyEntity->scale.x * boxScale, guyEntity->scale.y * boxScale, guyEntity->scale.z * (boxScale + 0.3f));
                // Raise the box so it sits above the floor and covers the head
                float boxVerticalOffset = guyEntity->scale.z * 0.5f;
                gfc_box_translate_local(&guyEntity->bounds, guyEntity->position.x, guyEntity->position.y, guyEntity->position.z + boxVerticalOffset);
                slog("Guy%d entity created at (%.2f, %.2f, %.2f) scale=%.2f color=(%.2f,%.2f,%.2f) bounds=[%.2f %.2f %.2f %.2f %.2f %.2f]", i+1, randX, randY, guyEntity->position.z, scale, guyEntity->color.r, guyEntity->color.g, guyEntity->color.b, guyEntity->bounds.x, guyEntity->bounds.y, guyEntity->bounds.z, guyEntity->bounds.w, guyEntity->bounds.h, guyEntity->bounds.d);
            } else {
                slog("Failed to allocate guy%d entity", i+1);
            }
        }
    } else {
        slog("Failed to load guy mesh");
    }
}
