#include <string.h>
#include "simple_logger.h"
#include "gf3d_particle.h"
#include "gf3d_pipeline.h"
#include "gf3d_swapchain.h"
#include "gf3d_commands.h"
#include "gf3d_vgraphics.h"
#include "gf3d_camera.h"

typedef struct
{
    GFC_Vector3D position;
    GFC_Color color;
}ParticleVertex;

typedef struct
{
    GFC_Matrix4 model;
    GFC_Matrix4 view;
    GFC_Matrix4 proj;
}ParticleUBO;

typedef struct
{
    Uint32          max_particles;
    Particle       *particle_list;
    Uint32          particle_count;
    Pipeline       *pipe;
    VkBuffer        buffer;
    VkDeviceMemory  bufferMemory;
}ParticleManager;

static ParticleManager gf3d_particle_manager = {0};

void gf3d_particle_manager_close();

static VkVertexInputBindingDescription* particle_get_bind_description()
{
    static VkVertexInputBindingDescription bindingDescription = {0};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(ParticleVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return &bindingDescription;
}

static VkVertexInputAttributeDescription* particle_get_attribute_descriptions(Uint32 *count)
{
    static VkVertexInputAttributeDescription attributeDescriptions[2] = {0};
    if (count)*count = 2;
    
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(ParticleVertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(ParticleVertex, color);

    return attributeDescriptions;
}

void gf3d_particle_manager_init(Uint32 max_particles)
{
    if (max_particles == 0)
    {
        slog("cannot initialize particle manager with zero particles");
        return;
    }
    memset(&gf3d_particle_manager, 0, sizeof(ParticleManager));
    gf3d_particle_manager.max_particles = max_particles;
    gf3d_particle_manager.particle_list = (Particle*)malloc(sizeof(Particle) * max_particles);
    if (!gf3d_particle_manager.particle_list)
    {
        slog("failed to allocate particle list");
        return;
    }
    memset(gf3d_particle_manager.particle_list, 0, sizeof(Particle) * max_particles);
    
    // Create particle pipeline using sprite shaders
    Uint32 attrCount = 0;
    particle_get_attribute_descriptions(&attrCount);
    
    gf3d_particle_manager.pipe = gf3d_pipeline_create_from_config(
        gf3d_vgraphics_get_default_logical_device(),
        "config/overlay_pipeline.cfg",
        gf3d_vgraphics_get_view_extent(),
        max_particles,
        particle_get_bind_description(),
        particle_get_attribute_descriptions(NULL),
        attrCount,
        sizeof(ParticleUBO),
        VK_INDEX_TYPE_UINT16
    );
    
    if (!gf3d_particle_manager.pipe)
    {
        slog("warning: failed to create particle pipeline, particles will not render");
    }
    
    atexit(gf3d_particle_manager_close);
    slog("particle manager initialized with %i max particles", max_particles);
}

void gf3d_particle_manager_close()
{
    if (gf3d_particle_manager.particle_list)
    {
        free(gf3d_particle_manager.particle_list);
        gf3d_particle_manager.particle_list = NULL;
    }
    gf3d_particle_manager.max_particles = 0;
    gf3d_particle_manager.particle_count = 0;
    slog("particle manager closed");
}

Particle gf3d_particle(GFC_Vector3D position, GFC_Color color, float size)
{
    Particle p = {0};
    p.position = position;
    p.color = color;
    p.color2 = color;
    p.size = size;
    return p;
}

void gf3d_particle_reset_pipes()
{
    gf3d_particle_manager.particle_count = 0;
}

void gf3d_particle_submit_pipe_commands()
{
    if (!gf3d_particle_manager.pipe)
    {
        return; // no pipeline, nothing to submit
    }
    if (gf3d_particle_manager.particle_count == 0)
    {
        return; // nothing to draw
    }
    
    // TODO: Implement actual Vulkan command buffer submission
    // For now, just log that particles would be drawn
    // slog("submitting %i particles for drawing", gf3d_particle_manager.particle_count);
}

void gf3d_particle_draw(Particle particle)
{
    if (gf3d_particle_manager.particle_count >= gf3d_particle_manager.max_particles)
    {
        slog("particle manager at capacity, dropping particle");
        return;
    }
    gf3d_particle_manager.particle_list[gf3d_particle_manager.particle_count++] = particle;
}

void gf3d_particle_draw_textured(Particle particle, Texture *texture)
{
    // For now, just draw without texture
    gf3d_particle_draw(particle);
}

void gf3d_particle_draw_sprite(Particle particle, Sprite *sprite, int frame)
{
    // For now, just draw without sprite
    gf3d_particle_draw(particle);
}

void gf3d_particle_trail_draw(GFC_Color color, float size, Uint8 count, GFC_Edge3D trail)
{
    if (count == 0) return;
    
    GFC_Vector3D delta;
    gfc_vector3d_sub(delta, trail.b, trail.a);
    gfc_vector3d_scale(delta, delta, 1.0f / (float)count);
    
    for (Uint8 i = 0; i < count; i++)
    {
        GFC_Vector3D pos;
        gfc_vector3d_scale(pos, delta, (float)i);
        gfc_vector3d_add(pos, pos, trail.a);
        gf3d_particle_draw(gf3d_particle(pos, color, size));
    }
}

void gf3d_particle_draw_array(Particle *particle, Uint32 count)
{
    if (!particle) return;
    for (Uint32 i = 0; i < count; i++)
    {
        gf3d_particle_draw(particle[i]);
    }
}

void gf3d_particle_draw_list(GFC_List *list)
{
    if (!list) return;
    Uint32 count = gfc_list_get_count(list);
    for (Uint32 i = 0; i < count; i++)
    {
        Particle *p = (Particle*)gfc_list_get_nth(list, i);
        if (p)
        {
            gf3d_particle_draw(*p);
        }
    }
}

Pipeline *gf3d_particle_get_pipeline()
{
    return gf3d_particle_manager.pipe;
}

void draw_guiding_lights(GFC_Vector3D position, GFC_Vector3D rotation, float width, float length)
{
    // Draw red line (X-axis)
    GFC_Edge3D redLine;
    redLine.a = position;
    redLine.b = position;
    redLine.b.x += length;
    gf3d_particle_trail_draw(GFC_COLOR_RED, 5.0f, 10, redLine);
    
    // Draw green line (Y-axis)
    GFC_Edge3D greenLine;
    greenLine.a = position;
    greenLine.b = position;
    greenLine.b.y += length;
    gf3d_particle_trail_draw(GFC_COLOR_GREEN, 5.0f, 10, greenLine);
}

/*eol@eof*/
