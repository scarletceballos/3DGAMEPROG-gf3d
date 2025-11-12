#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "simple_logger.h"
#include "simple_json.h"
#include "gfc_types.h"
#include "gfc_list.h"
#include "gfc_vector.h"
#include "gfc_matrix.h"
#include "gfc_color.h"
#include "gf3d_mesh.h"
#include "gf3d_swapchain.h"
#include "gf3d_obj_load.h"
#include "gf3d_pipeline.h"
#include "gf3d_vgraphics.h"
#include "gf3d_camera.h"
#include "gf3d_texture.h"
#include "gf3d_buffers.h"

#define MESH_ATTRIBUTE_COUNT 3

extern int __DEBUG;

typedef struct {
    Mesh* mesh_list;
    Uint32 mesh_count;
    Uint32 mesh_max;
    Uint32 chain_length;
    VkDevice device;
    Pipeline* skypipe;
    Pipeline* pipe;
    Texture* defaultTexture;
    VkVertexInputAttributeDescription attributeDescriptions[MESH_ATTRIBUTE_COUNT];
    VkVertexInputBindingDescription bindingDescription;
} MeshManager;

static MeshManager mesh_manager = { 0 };

// Internal function declarations
static void gf3d_mesh_delete(Mesh* mesh);
static void gf3d_mesh_manager_close(void);
static void gf3d_mesh_primitive_create_vertex_buffers(MeshPrimitive* prim);
static void gf3d_mesh_setup_face_buffers(MeshPrimitive* prim);
static VkVertexInputBindingDescription* gf3d_mesh_manager_get_bind_description(void);

// Public API

void gf3d_mesh_manager_init(Uint32 mesh_max, Uint32 chain_length, VkDevice device) {
    if (mesh_max == 0) {
        slog("Cannot initialize mesh manager with zero meshes");
        return;
    }

    mesh_manager.mesh_list = gfc_allocate_array(sizeof(Mesh), mesh_max);
    if (!mesh_manager.mesh_list) {
        slog("Failed to allocate %i meshes for the system", mesh_max);
        return;
    }

    mesh_manager.mesh_max = mesh_max;
    mesh_manager.mesh_count = 0;
    mesh_manager.chain_length = chain_length;
    mesh_manager.device = device;

    atexit(gf3d_mesh_manager_close);
    slog("Mesh manager initialized with %i meshes.", mesh_max);
}

void gf3d_mesh_init(Uint32 mesh_max)
{
    Uint32 count = 0;
    if (mesh_max == 0)
    {
        slog("cannot intilizat sprite manager for 0 sprites");
        return;
    }
    mesh_manager.chain_length = gf3d_swapchain_get_chain_length();
    mesh_manager.mesh_list = (Mesh *)gfc_allocate_array(sizeof(Mesh),mesh_max);
    if (!mesh_manager.mesh_list)
    {
        slog("failed to allocate mesh list for %u meshes",mesh_max);
        return;
    }
    memset(mesh_manager.mesh_list,0,sizeof(Mesh) * mesh_max);
    mesh_manager.mesh_max = mesh_max;
    mesh_manager.mesh_count = 0;
    mesh_manager.device = gf3d_vgraphics_get_default_logical_device();

    gf3d_mesh_get_attribute_descriptions(&count);
    
    mesh_manager.skypipe = gf3d_pipeline_create_from_config(
        gf3d_vgraphics_get_default_logical_device(),
        "config/sky_pipeline.cfg",
        gf3d_vgraphics_get_view_extent(),
        mesh_max,
        gf3d_mesh_manager_get_bind_description(),
        gf3d_mesh_get_attribute_descriptions(NULL),
        count,
        sizeof(MeshUBO),
        VK_INDEX_TYPE_UINT16
    );
    
    mesh_manager.pipe = gf3d_pipeline_create_from_config(
        gf3d_vgraphics_get_default_logical_device(),
        "config/model_pipeline.cfg",
        gf3d_vgraphics_get_view_extent(),
        mesh_max,
        gf3d_mesh_manager_get_bind_description(),
        gf3d_mesh_get_attribute_descriptions(NULL),
        count,
        sizeof(MeshUBO),
        VK_INDEX_TYPE_UINT16
    );
    mesh_manager.defaultTexture = gf3d_texture_load("images/default.png");
    if(__DEBUG)slog("mesh manager initiliazed");
    atexit(gf3d_mesh_manager_close);
}

static VkVertexInputBindingDescription* gf3d_mesh_manager_get_bind_description(void)
{
    mesh_manager.bindingDescription.binding = 0;
    mesh_manager.bindingDescription.stride = sizeof(Vertex);
    mesh_manager.bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return &mesh_manager.bindingDescription;
}

VkVertexInputBindingDescription* gf3d_mesh_get_bind_description()
{
    return gf3d_mesh_manager_get_bind_description();
}


void gf3d_mesh_manager_close(void) {
    if (mesh_manager.mesh_list) {
        for (Uint32 i = 0; i < mesh_manager.mesh_max; i++) {
            if (mesh_manager.mesh_list[i]._refCount) {
                gf3d_mesh_delete(&mesh_manager.mesh_list[i]);
            }
        }
        free(mesh_manager.mesh_list);
        mesh_manager.mesh_list = NULL;
    }
    mesh_manager.mesh_max = 0;
    mesh_manager.mesh_count = 0;
    slog("Mesh manager closed.");
}

VkVertexInputAttributeDescription* gf3d_mesh_get_attribute_descriptions(Uint32* count) {
    mesh_manager.attributeDescriptions[0].binding = 0;
    mesh_manager.attributeDescriptions[0].location = 0;
    mesh_manager.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    mesh_manager.attributeDescriptions[0].offset = offsetof(Vertex, vertex);

    mesh_manager.attributeDescriptions[1].binding = 0;
    mesh_manager.attributeDescriptions[1].location = 1;
    mesh_manager.attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    mesh_manager.attributeDescriptions[1].offset = offsetof(Vertex, normal);

    mesh_manager.attributeDescriptions[2].binding = 0;
    mesh_manager.attributeDescriptions[2].location = 2;
    mesh_manager.attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    mesh_manager.attributeDescriptions[2].offset = offsetof(Vertex, texel);

    if (count) *count = MESH_ATTRIBUTE_COUNT;
    return mesh_manager.attributeDescriptions;
}

Mesh* gf3d_mesh_new(void) {
    for (Uint32 i = 0; i < mesh_manager.mesh_max; i++) {
        if (mesh_manager.mesh_list[i]._refCount) continue;
        memset(&mesh_manager.mesh_list[i], 0, sizeof(Mesh));
        mesh_manager.mesh_list[i]._refCount = 1;
        mesh_manager.mesh_list[i].primitives = gfc_list_new();
        mesh_manager.mesh_count++;
        return &mesh_manager.mesh_list[i];
    }
    slog("No available mesh slots.");
    return NULL;
}

MeshPrimitive* gf3d_mesh_primitive_new(void) {
    MeshPrimitive* prim = gfc_allocate_array(sizeof(MeshPrimitive), 1);
    if (!prim) slog("Failed to allocate MeshPrimitive.");
    return prim;
}

void gf3d_mesh_primitive_free(MeshPrimitive* prim) {
    if (!prim) return;
    if (prim->vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mesh_manager.device, prim->vertexBuffer, NULL);
    }
    if (prim->vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mesh_manager.device, prim->vertexBufferMemory, NULL);
    }
    if (prim->faceBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(mesh_manager.device, prim->faceBuffer, NULL);
    }
    if (prim->faceBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(mesh_manager.device, prim->faceBufferMemory, NULL);
    }
    free(prim);
}

static int gf3d_mesh_primitive_build_from_obj(MeshPrimitive* prim, ObjData* obj) {
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkDevice device;
    VkDeviceSize bufferSize;
    void* data = NULL;

    slog("gf3d_mesh_primitive_build_from_obj: entry");
    if (!prim || !obj) {
        slog("gf3d_mesh_primitive_build_from_obj: NULL prim or obj");
        return 0;
    }
    slog("gf3d_mesh_primitive_build_from_obj: checking face data, face_vert_count=%d", obj->face_vert_count);
    if (!obj->faceVertices || !obj->face_vert_count) {
        slog("ObjData missing face vertex data, cannot build mesh primitive");
        return 0;
    }

    device = mesh_manager.device;
    bufferSize = sizeof(Vertex) * obj->face_vert_count;
    slog("gf3d_mesh_primitive_build_from_obj: creating staging buffer, size=%llu bytes", (unsigned long long)bufferSize);

    if (!gf3d_buffer_create(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory)) {
        slog("Failed to create staging buffer for mesh primitive");
        return 0;
    }
    slog("gf3d_mesh_primitive_build_from_obj: staging buffer created");

    slog("gf3d_mesh_primitive_build_from_obj: mapping staging buffer memory");
    if (vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &data) != VK_SUCCESS) {
        slog("Failed to map staging buffer memory for mesh primitive");
        vkDestroyBuffer(device, stagingBuffer, NULL);
        vkFreeMemory(device, stagingMemory, NULL);
        return 0;
    }

    slog("gf3d_mesh_primitive_build_from_obj: copying %llu bytes to staging buffer", (unsigned long long)bufferSize);
    memcpy(data, obj->faceVertices, bufferSize);
    vkUnmapMemory(device, stagingMemory);
    slog("gf3d_mesh_primitive_build_from_obj: staging buffer unmapped");

    slog("gf3d_mesh_primitive_build_from_obj: creating device-local vertex buffer");
    if (!gf3d_buffer_create(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &prim->vertexBuffer, &prim->vertexBufferMemory)) {
        slog("Failed to create vertex buffer for mesh primitive");
        vkDestroyBuffer(device, stagingBuffer, NULL);
        vkFreeMemory(device, stagingMemory, NULL);
        return 0;
    }
    slog("gf3d_mesh_primitive_build_from_obj: device buffer created");

    slog("gf3d_mesh_primitive_build_from_obj: copying staging to device buffer");
    gf3d_buffer_copy(stagingBuffer, prim->vertexBuffer, bufferSize);
    slog("gf3d_mesh_primitive_build_from_obj: buffer copy complete");

    vkDestroyBuffer(device, stagingBuffer, NULL);
    vkFreeMemory(device, stagingMemory, NULL);
    slog("gf3d_mesh_primitive_build_from_obj: staging resources freed");

    prim->vertexCount = obj->face_vert_count;
    prim->faceCount = obj->face_count;
    prim->faceBuffer = VK_NULL_HANDLE;
    prim->faceBufferMemory = VK_NULL_HANDLE;

    slog("gf3d_mesh_primitive_build_from_obj: success, vertexCount=%d, faceCount=%d", prim->vertexCount, prim->faceCount);
    return 1;
}

Mesh* gf3d_mesh_get_by_filename(const char* filename) {
    if (!filename) return NULL;
    for (Uint32 i = 0; i < mesh_manager.mesh_max; i++) {
        if (!mesh_manager.mesh_list[i]._refCount) continue;
        if (gfc_strlcmp(mesh_manager.mesh_list[i].filename, filename) == 0) {
            return &mesh_manager.mesh_list[i];
        }
    }
    return NULL;
}

Mesh* gf3d_mesh_load(const char* filename) {
    if (!filename) {
        slog("gf3d_mesh_load: NULL filename");
        return NULL;
    }

    slog("gf3d_mesh_load: attempting to load %s", filename);
    Mesh* mesh = gf3d_mesh_get_by_filename(filename);
    if (mesh) {
        slog("gf3d_mesh_load: found cached mesh for %s", filename);
        mesh->_refCount++;
        return mesh;
    }

    slog("gf3d_mesh_load: loading OBJ data from file");
    ObjData* obj = gf3d_obj_load_from_file(filename);
    if (!obj) {
        slog("Failed to load obj file: %s", filename);
        return NULL;
    }
    slog("gf3d_mesh_load: OBJ data loaded, %d vertices, %d faces", obj->vertex_count, obj->face_count);

    slog("gf3d_mesh_load: creating new mesh structure");
    mesh = gf3d_mesh_new();
    if (!mesh) {
        slog("Failed to create new mesh");
        gf3d_obj_free(obj);
        return NULL;
    }

    gfc_line_cpy(mesh->filename, filename);

    slog("gf3d_mesh_load: creating mesh primitive");
    MeshPrimitive* primitive = gf3d_mesh_primitive_new();
    if (!primitive) {
        slog("Failed to create mesh primitive");
        gf3d_mesh_free(mesh);
        gf3d_obj_free(obj);
        return NULL;
    }
    slog("gf3d_mesh_load: mesh primitive created");

    primitive->objData = obj;
    slog("gf3d_mesh_load: building GPU buffers from OBJ data");
    if (!gf3d_mesh_primitive_build_from_obj(primitive, obj)) {
        slog("Failed to build GPU buffers for mesh %s", filename);
        gf3d_mesh_primitive_free(primitive);
        gf3d_mesh_free(mesh);
        gf3d_obj_free(obj);
        return NULL;
    }
    slog("gf3d_mesh_load: GPU buffers created successfully");

    mesh->bounds = obj->bounds;
    gfc_list_append(mesh->primitives, primitive);

    slog("gf3d_mesh_load: mesh load complete for %s", filename);
    return mesh;
}

void gf3d_mesh_free(Mesh* mesh) {
    if (!mesh) return;
    if (mesh->_refCount > 0) mesh->_refCount--;
    if (mesh->_refCount == 0) gf3d_mesh_delete(mesh);
}

Pipeline* gf3d_mesh_get_pipeline(void) {
    return mesh_manager.pipe;
}

void gf3d_mesh_draw(Mesh *mesh,GFC_Matrix4 modelMat,GFC_Color mod,Texture *texture,GFC_Vector3D lightPos,GFC_Color lightColor)
{
    MeshUBO ubo = {0};
    
    if (!mesh)return;
    gfc_matrix4_copy(ubo.model,modelMat);
    gf3d_vgraphics_get_view(&ubo.view);
    gf3d_vgraphics_get_projection_matrix(&ubo.proj);

    ubo.color = gfc_color_to_vector4f(mod);
    ubo.lightColor = gfc_color_to_vector4f(lightColor);
    ubo.lightPos = gfc_vector3dw(lightPos,1.0);
    ubo.camera = gfc_vector3dw(gf3d_camera_get_position(),1.0);
    gf3d_mesh_queue_render(mesh,mesh_manager.pipe,&ubo,texture);
}

void gf3d_mesh_sky_draw(Mesh *mesh,GFC_Matrix4 modelMat,GFC_Color mod,Texture *texture)
{
    MeshUBO ubo = {0};
    
    if (!mesh)return;
    gfc_matrix4_copy(ubo.model,modelMat);
    gf3d_vgraphics_get_view(&ubo.view);
    gf3d_vgraphics_get_projection_matrix(&ubo.proj);

    ubo.color = gfc_color_to_vector4f(mod);
    // For sky rendering, we don't need lighting
    ubo.lightColor = gfc_color_to_vector4f(GFC_COLOR_WHITE);
    ubo.lightPos = gfc_vector4d(0,0,0,1);
    ubo.camera = gfc_vector4d(0,0,0,1);
    
    gf3d_mesh_queue_render(mesh,mesh_manager.skypipe,&ubo,texture);
}

// Internal functions

static void gf3d_mesh_delete(Mesh* mesh) {
    if (!mesh) return;
    int count = gfc_list_get_count(mesh->primitives);
    for (int i = 0; i < count; i++) {
        MeshPrimitive* prim = gfc_list_get_nth(mesh->primitives, i);
        if (prim) gf3d_mesh_primitive_free(prim);
    }
    if (mesh->primitives) gfc_list_delete(mesh->primitives);
    memset(mesh, 0, sizeof(Mesh));
    mesh_manager.mesh_count--;
}

void gf3d_mesh_primitive_queue_render(MeshPrimitive* prim, Pipeline* pipe, void* uboData, Texture* texture) {
    if (!prim || !pipe || !uboData) return;
    if (!texture) texture = mesh_manager.defaultTexture;
    gf3d_pipeline_queue_render(
        pipe,
        prim->vertexBuffer,
        prim->vertexCount,
        prim->faceBuffer,
        uboData,
        texture
    );
}

void gf3d_mesh_queue_render(Mesh* mesh, Pipeline* pipe, void* uboData, Texture* texture) {
    if (!mesh || !pipe || !uboData) return;
    int count = gfc_list_get_count(mesh->primitives);
    for (int i = 0; i < count; i++) {
        MeshPrimitive* prim = gfc_list_get_nth(mesh->primitives, i);
        if (prim) gf3d_mesh_primitive_queue_render(prim, pipe, uboData, texture);
    }
}
