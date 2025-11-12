#include <stdlib.h>
#include <string.h>

#include "simple_json.h"
#include "simple_json_value.h"
#include "simple_logger.h"
#include "gfc_types.h"
#include "gfc_matrix.h"
#include "gfc_color.h"
#include "gf3d_mesh.h"
#include "gf3d_texture.h"
#include "world.h"

World* world_new() {
    World* world;
    world = gfc_allocate_array(sizeof(World), 1);
    if (!world) return NULL;
    
    memset(world, 0, sizeof(World));
    world->lightcolor = GFC_COLOR_WHITE;
    world->lightpos = gfc_vector3d(0, 10, 10);
    
    return world;
}

Uint8 world_edge_test(World* world, GFC_Vector3D start, GFC_Vector3D end) {
    // TODO: Work on collision detection between edge and world geometry
    // Notes: check intersection with terrain mesh triangles
    if (!world) return 0;
    
    // Placeholder implementation - always return no collision for now
    return 0;
}

World* world_load(const char* filename) {
    World* world;
    SJson* json;
    SJson* config;
    const char* str;
    
    if (!filename) {
        slog("world_load: filename is NULL");
        return NULL;
    }
    
    slog("world_load: attempting to load JSON from %s", filename);
    json = sj_load(filename);
    if (!json) {
        slog("failed to load world file %s", filename);
        return NULL;
    }
    slog("world_load: JSON loaded successfully");
    
    world = world_new();
    if (!world) {
        slog("failed to allocate a world for %s", filename);
        sj_free(json);
        return NULL;
    }
    slog("world_load: world structure allocated");
    
    config = sj_object_get_value(json, "world");
    if (!config) {
        slog("failed to parse world configuration for %s - missing 'world' key", filename);
        sj_free(json);
        world_free(world);
        return NULL;
    }
    slog("world_load: found 'world' object in JSON");
    
    // Load terrain mesh
    slog("world_load: attempting to read terrain mesh path");
    SJson* terrainJson = sj_object_get_value(config, "terrain");
    str = NULL;
    if (terrainJson) {
        slog("world_load: terrain JSON node found, type=%d", terrainJson->sjtype);
        if (terrainJson->sjtype == SJVT_String && terrainJson->v.string) {
            str = terrainJson->v.string->text;
            slog("world_load: extracted terrain path: %s", str ? str : "(null)");
        } else {
            str = sj_get_string_value(terrainJson);
            if (str) slog("world_load: got terrain via sj_get_string_value: %s", str);
        }
    } else {
        slog("world_load: no terrain node in config");
    }
    if (!str) {
        // Try alternative API
        SJson* terrainJson = sj_object_get_value(config, "terrain");
        if (terrainJson && terrainJson->sjtype == SJVT_String) {
            str = terrainJson->v.string->text;
        }
    }
    if (str) {
        slog("world_load: loading terrain mesh from %s", str);
        world->terrain = gf3d_mesh_load(str);
        if (!world->terrain) {
            slog("failed to load terrain mesh: %s", str);
        } else {
            slog("loaded terrain mesh: %s", str);
        }
    } else {
        slog("world_load: no terrain mesh path specified, skipping");
    }
    
    // Load terrain texture
    slog("world_load: attempting to read terrain texture path");
    SJson* textureJson = sj_object_get_value(config, "terrainTexture");
    str = NULL;
    if (textureJson) {
        slog("world_load: texture JSON node found, type=%d", textureJson->sjtype);
        if (textureJson->sjtype == SJVT_String && textureJson->v.string) {
            str = textureJson->v.string->text;
            slog("world_load: extracted texture path: %s", str ? str : "(null)");
        } else {
            str = sj_get_string_value(textureJson);
            if (str) slog("world_load: got texture via sj_get_string_value: %s", str);
        }
    } else {
        slog("world_load: no terrainTexture node in config");
    }
    if (str) {
        slog("world_load: loading terrain texture from %s", str);
        world->texture = gf3d_texture_load(str);
        if (!world->texture) {
            slog("failed to load terrain texture: %s", str);
        } else {
            slog("loaded terrain texture: %s", str);
        }
    } else {
        slog("world_load: no terrain texture path specified, skipping");
    }
    
    // Load light settings
    slog("world_load: setting default light parameters");
    world->lightcolor = GFC_COLOR_WHITE;
    world->lightpos = gfc_vector3d(8, 15, 12);
    
    slog("world_load: skipping JSON free (workaround for crash)");
    // TODO: Fix sj_free crash - seems to be an issue with binary-mode loaded JSON
    // if (json) {
    //     sj_free(json);
    //     slog("world_load: JSON freed successfully");
    // }
    slog("world_load: returning world=%p", world);
    return world;
}

void world_free(World* world) {
    if (!world) return;
    
    if (world->terrain) {
        gf3d_mesh_free(world->terrain);
    }
    if (world->texture) {
        gf3d_texture_free(world->texture);
    }
    if (world->entities) {
        gfc_list_delete(world->entities);
    }
    
    free(world);
}

void world_draw(World* world) {
    GFC_Matrix4 modelMat;
    
    if (!world) return;
    
    if (world->terrain && world->texture) {
        gfc_matrix4_identity(modelMat);
        gf3d_mesh_draw(
            world->terrain,
            modelMat,
            GFC_COLOR_WHITE,
            world->texture,
            world->lightpos,
            world->lightcolor
        );
    }
}
