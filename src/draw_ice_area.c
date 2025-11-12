// Draw a semi-transparent blue rectangle for each ice area entity
#include "entity.h"
#include "simple_logger.h"
#include <SDL.h>

void draw_ice_areas() {
    Entity* ents = entity_system_get_all_entities();
    Uint32 ent_max = entity_system_get_count();
    for (Uint32 i = 0; i < ent_max; ++i) {
        if (!ents[i]._inuse) continue;
        if (strncmp(ents[i].name, "ice_area", 8) != 0) continue;
        // Draw as a filled rectangle (quad) at ents[i].position, with ents[i].scale.x/y as size, ents[i].color as color
        // Use a 2D overlay draw for now
        // For now, use gf2d_sprite_draw if available, or stub
        // TODO: Replace with 3D quad draw for proper Z
        // Example: gf2d_draw_rect(pos, size, color, alpha)
        // Placeholder: log
        //slog("[ICE AREA] Draw at (%.2f, %.2f, %.2f) size=(%.2f, %.2f) color=(%.2f, %.2f, %.2f, %.2f)", ents[i].position.x, ents[i].position.y, ents[i].position.z, ents[i].scale.x, ents[i].scale.y, ents[i].color.r, ents[i].color.g, ents[i].color.b, ents[i].color.a);
    }
}
