#include "house.h"
#include "simple_logger.h"
#include <stdlib.h>

void house_spawn_all(float minX, float maxX, float minY, float maxY, float map_floor_z, Texture* tempTexture) {
    Mesh* houseMesh = gf3d_mesh_load("models/houses/house2.obj");
    if (houseMesh) {
        GFC_Color houseColors[3] = {
            gfc_color(0.8f, 0.3f, 0.3f, 1),
            gfc_color(0.3f, 0.8f, 0.3f, 1),
            gfc_color(0.3f, 0.3f, 0.8f, 1)
        };
        for (int i = 0; i < 10; ++i) {
            float randX = minX + ((float)rand() / RAND_MAX) * (maxX - minX);
            float randY = minY + ((float)rand() / RAND_MAX) * (maxY - minY);
            Entity* houseEntity = entity_new();
            if (houseEntity) {
                char name[16];
                snprintf(name, sizeof(name), "house%d", i+1);
                gfc_line_cpy(houseEntity->name, name);
                houseEntity->mesh = houseMesh;
                houseEntity->texture = tempTexture;
                houseEntity->position = gfc_vector3d(randX, randY, map_floor_z + 6.45f);
                houseEntity->rotation = gfc_vector3d(0, 0, 0);
                houseEntity->scale = gfc_vector3d(9.5f, 9.5f, 10.5f);
                houseEntity->color = houseColors[i % 3];
                houseEntity->bounds = houseMesh->bounds;
                gfc_box_scale_local(&houseEntity->bounds, houseEntity->scale.x * 1.3f, houseEntity->scale.y * 1.3f, houseEntity->scale.z * 1.3f);
                gfc_box_translate_local(&houseEntity->bounds, houseEntity->position.x, houseEntity->position.y, houseEntity->position.z);
                slog("House%d entity created at (%.2f, %.2f, %.2f) color=(%.2f, %.2f, %.2f)", i+1, randX, randY, map_floor_z + 1.45f, houseEntity->color.r, houseEntity->color.g, houseEntity->color.b);
            } else {
                slog("Failed to allocate house%d entity", i+1);
            }
        }
    } else {
        slog("Failed to load house mesh");
    }
}
