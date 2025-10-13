/**
 * @purpose vulkan graphics setup and abstraction
*/

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "simple_logger.h"
#include "simple_json.h"

#include "gfc_types.h"
#include "gfc_vector.h"
#include "gfc_matrix.h"
#include "gfc_pak.h"
#include "gfc_config.h"

#include "gf3d_device.h"
#include "gf3d_debug.h"
#include "gf3d_validation.h"
#include "gf3d_extensions.h"
#include "gf3d_vqueues.h"
#include "gf3d_swapchain.h"
#include "gf3d_pipeline.h"
#include "gf3d_commands.h"
#include "gf3d_texture.h"
#include "gf3d_mesh.h"
#include "gf2d_sprite.h"

#include "gf3d_vgraphics.h"


typedef struct
{
    SDL_Window                 *main_window;

    VkApplicationInfo           vk_app_info;

    VkInstance                  vk_instance;
    VkInstanceCreateInfo        vk_instance_info;

    Uint32                      sdl_extension_count;
    const char                **sdl_extension_names;
    Bool                        enableValidationLayers;
    
    unsigned int                enabled_layer_count;

    //devices
    VkPhysicalDevice            gpu;
    Bool                        logicalDeviceCreated;
    
    VkDevice                    device;
    VkSurfaceKHR                surface;

    // color space
    GFC_Color                   bgcolor;
    VkFormat                    color_format;
    VkColorSpaceKHR             color_space;
    
    VkSemaphore                 imageAvailableSemaphore;
    VkSemaphore                 renderFinishedSemaphore;

    Command                 *   graphicsCommandPool; 
    ModelViewProjection         ubo;
    
    //render frame and command buffer for the current render pass
    Uint32                      bufferFrame;
    
    SDL_Surface                *screen;
    Sint32                      bitdepth;
    Uint32                      rmask;
    Uint32                      gmask;
    Uint32                      bmask;
    Uint32                      amask;
    Bool                        enable_3d;
    Bool                        enable_2d;
}vGraphics;

static vGraphics gf3d_vgraphics = {0};

int __DEBUG = 0;

void gf3d_vgraphics_close();
void gf3d_vgraphics_logical_device_close();
void gf3d_vgraphics_extension_init();
void gf3d_vgraphics_semaphores_create();

VkDeviceCreateInfo gf3d_vgraphics_get_device_info(Bool enableValidationLayers);

void gf3d_vgraphics_debug_close();
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator);

void gf3d_vgraphics_setup(
    const char *windowName,
    int renderWidth,
    int renderHeight,
    Bool fullscreen,
    Bool enableValidation,
    Bool enableDebug,
    const char *config
);

void gf3d_vgraphics_init(const char *config)
{
    Pipeline *renderPipe= NULL;
    SJson *json,*setup;
    const char *windowName = NULL;
    GFC_Vector2D resolution = {1024,768};
    short int fullscreen = 0;
    short int enableValidation = 0;
    short int enableDebug = 0;
    
    json = gfc_pak_load_json(config);
    if (!json)
    {
        slog("graphics config file load failed, exiting");
        exit(0);
        return;
    }
    setup = sj_object_get_value(json,"setup");
    
    if (!setup)
    {
        slog("graphics config file missing setup data, exiting");
        sj_free(json);
        exit(0);
        return;
    }
    
    windowName = sj_object_get_value_as_string(setup,"application_name");
    sj_value_as_vector2d(sj_object_get_value(setup,"resolution"),&resolution);
    gf3d_vgraphics.bgcolor = sj_value_as_color(sj_object_get_value(setup,"background"));
    sj_get_bool_value(sj_object_get_value(setup,"fullscreen"),&fullscreen);
    sj_get_bool_value(sj_object_get_value(json,"enable_debug"),&enableDebug);
    sj_get_bool_value(sj_object_get_value(json,"enable_validation"),&enableValidation);
    
    if (resolution.y == 0)
    {
        slog("invalid resolution (%f,%f), closing",resolution.x,resolution.y);
        sj_free(json);
        exit(0);
        return;
    }

    gfc_matrix4_identity(gf3d_vgraphics.ubo.model);
    gfc_matrix4_identity(gf3d_vgraphics.ubo.view);
    gfc_matrix4_identity(gf3d_vgraphics.ubo.proj);
    
    gfc_matrix4_perspective(
        gf3d_vgraphics.ubo.proj,
        45 * GFC_DEGTORAD,
        resolution.x/resolution.y,
        0.1f,
        100000
    );
    
    gf3d_vgraphics.ubo.proj[1][1] *= -1;

    gf3d_vgraphics_setup(
        windowName,
        resolution.x,
        resolution.y,
        fullscreen,
        enableValidation,
        enableDebug,
        config
        );
    
    gf3d_vgraphics.device = gf3d_vgraphics_get_default_logical_device();

    gf3d_vqueues_setup_device_queues(gf3d_vgraphics.device);
    // swap chain!!!
    gf3d_swapchain_init(gf3d_vgraphics.gpu,gf3d_vgraphics.device,gf3d_vgraphics.surface,resolution.x,resolution.y);
    gf3d_pipeline_init(16);// how many different rendering pipelines we need
    
    // 2D stuff
    SDL_PixelFormatEnumToMasks(SDL_PIXELFORMAT_RGBA32,
                                &gf3d_vgraphics.bitdepth,
                                &gf3d_vgraphics.rmask,
                                &gf3d_vgraphics.gmask,
                                &gf3d_vgraphics.bmask,
                                &gf3d_vgraphics.amask);
    
    gf3d_vgraphics.screen = SDL_CreateRGBSurface(
        0,
        resolution.x,
        resolution.y,
        gf3d_vgraphics.bitdepth,
        gf3d_vgraphics.rmask,
        gf3d_vgraphics.gmask,
        gf3d_vgraphics.bmask,
        gf3d_vgraphics.amask);

    gf3d_texture_init(1024);

    gf3d_command_system_init(16 * gf3d_swapchain_get_swap_image_count(), gf3d_vgraphics.device);
    gf3d_vgraphics.graphicsCommandPool = gf3d_command_graphics_pool_setup(gf3d_swapchain_get_swap_image_count());

    gf3d_vgraphics.enable_2d = 1;
    gf3d_mesh_init(1024);
    gf2d_sprite_manager_init(1024);
    renderPipe = gf3d_mesh_get_pipeline();

    gf3d_swapchain_create_depth_image();
    gf3d_swapchain_setup_frame_buffers(renderPipe);
    gf3d_vgraphics_semaphores_create();
}


void gf3d_vgraphics_setup(
    const char *windowName,
    int renderWidth,
    int renderHeight,
    Bool fullscreen,
    Bool enableValidation,
    Bool enableDebug,
    const char *config
)
{
    Uint32 flags = SDL_WINDOW_VULKAN;
    Uint32 i;
    Uint32 enabledExtensionCount = 0;
    
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        slog("Unable to initilaize SDL system: %s",SDL_GetError());
        return;
    }
    atexit(SDL_Quit);
    SDL_ShowCursor(SDL_DISABLE);
    if (fullscreen)
    {
        if (renderWidth == 0)
        {
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else
        {
            flags |= SDL_WINDOW_FULLSCREEN;
        }
    }
	slog_sync();
    gf3d_vgraphics.main_window = SDL_CreateWindow(windowName,
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             renderWidth, renderHeight,
                             flags);
	slog_sync();
    if (!gf3d_vgraphics.main_window)
    {
        slog("failed to create main window: %s",SDL_GetError());
        gf3d_vgraphics_close();
        exit(0);
        return;
    }
	slog_sync();
    // instance extension configuration
    
    gf3d_extensions_instance_init(config);
    
	slog_sync();
    // get the extensions that are needed for rendering to an SDL Window
    SDL_Vulkan_GetInstanceExtensions(gf3d_vgraphics.main_window, &(gf3d_vgraphics.sdl_extension_count), NULL);
    if (gf3d_vgraphics.sdl_extension_count > 0)
    {
        gf3d_vgraphics.sdl_extension_names = gfc_allocate_array(sizeof(const char *),gf3d_vgraphics.sdl_extension_count);
        
        SDL_Vulkan_GetInstanceExtensions(gf3d_vgraphics.main_window, &(gf3d_vgraphics.sdl_extension_count), gf3d_vgraphics.sdl_extension_names);
        for (i = 0; i < gf3d_vgraphics.sdl_extension_count;i++)
        {
            if (__DEBUG)slog("SDL Vulkan extensions support: %s",gf3d_vgraphics.sdl_extension_names[i]);
            gf3d_extensions_enable(ET_Instance, gf3d_vgraphics.sdl_extension_names[i]);
        }
    }
    else
    {
        slog("SDL / Vulkan not supported");
        gf3d_vgraphics_close();
        exit(0);
        return;
    }
	slog_sync();
    // setup app info
    gf3d_vgraphics.vk_app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    gf3d_vgraphics.vk_app_info.pNext = NULL;
    gf3d_vgraphics.vk_app_info.pApplicationName = windowName;
    gf3d_vgraphics.vk_app_info.applicationVersion = 0;
    gf3d_vgraphics.vk_app_info.pEngineName = windowName;
    gf3d_vgraphics.vk_app_info.engineVersion = 0;
    gf3d_vgraphics.vk_app_info.apiVersion = VK_API_VERSION_1_2;
    
    gf3d_vgraphics.vk_instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    gf3d_vgraphics.vk_instance_info.pNext = NULL;
    gf3d_vgraphics.vk_instance_info.pApplicationInfo = &gf3d_vgraphics.vk_app_info;
    
    if (enableValidation)
    {
        gf3d_validation_init(config);
        gf3d_vgraphics.vk_instance_info.enabledLayerCount = gf3d_validation_get_enabled_layer_count();
        gf3d_vgraphics.vk_instance_info.ppEnabledLayerNames = gf3d_validation_get_enabled_layer_names();
        gf3d_extensions_enable(ET_Instance,"VK_EXT_debug_utils");
    }
    else
    {
        //setup instance info
        gf3d_vgraphics.vk_instance_info.enabledLayerCount = 0;
        gf3d_vgraphics.vk_instance_info.ppEnabledLayerNames = NULL;
    }
    gf3d_vgraphics.vk_instance_info.ppEnabledExtensionNames = gf3d_extensions_get_instance_enabled_names(&enabledExtensionCount);
    gf3d_vgraphics.vk_instance_info.enabledExtensionCount = enabledExtensionCount;

	slog_sync();

    // create instance
    vkCreateInstance(&gf3d_vgraphics.vk_instance_info, NULL, &gf3d_vgraphics.vk_instance);

    if (!gf3d_vgraphics.vk_instance)
    {
        slog("failed to create a vulkan instance");
        gf3d_vgraphics_close();
        return;
    }
	slog_sync();
    if (enableDebug)
    {
        gf3d_debug_setup(gf3d_vgraphics.vk_instance);
    }
    atexit(gf3d_vgraphics_close);
    
    // create a surface for the window
    SDL_Vulkan_CreateSurface(gf3d_vgraphics.main_window, gf3d_vgraphics.vk_instance, &gf3d_vgraphics.surface);
    
    if (gf3d_vgraphics.surface == VK_NULL_HANDLE)
    {
        slog("failed to create render target surface");
        gf3d_vgraphics_close();
        return;
    }
    
    gf3d_device_manager_init(config, gf3d_vgraphics.vk_instance,gf3d_vgraphics.surface);


    gf3d_vgraphics.gpu = gf3d_devices_get_best_device();
    if(!gf3d_vgraphics.gpu){
        slog("Failed to select graphics card.");
        gf3d_vgraphics_close();
        return;
    }
    
    gf3d_vgraphics.device = gf3d_device_get();
    if (gf3d_vgraphics.device != VK_NULL_HANDLE)
    {
        gf3d_vgraphics.logicalDeviceCreated = true;
    }
    else
    {
        exit(0);
    }
    slog_sync();
}

void gf3d_vgraphics_close()
{
    if (gf3d_vgraphics.sdl_extension_names)
    {
        free(gf3d_vgraphics.sdl_extension_names);
    }
    gf3d_debug_close();
        
    if(gf3d_vgraphics.surface && gf3d_vgraphics.vk_instance)
    {
        vkDestroySurfaceKHR(gf3d_vgraphics.vk_instance,gf3d_vgraphics.surface, NULL);
    }
    if (gf3d_vgraphics.main_window)
    {
        SDL_DestroyWindow(gf3d_vgraphics.main_window);
    }
    if (gf3d_vgraphics.vk_instance)
    {
        vkDestroyInstance(gf3d_vgraphics.vk_instance, NULL);
    }
    memset(&gf3d_vgraphics,0,sizeof(vGraphics));
}

VkDevice gf3d_vgraphics_get_default_logical_device()
{
    return gf3d_vgraphics.device;
}

VkPhysicalDevice gf3d_vgraphics_get_default_physical_device()
{
    return gf3d_vgraphics.gpu;
}

VkExtent2D gf3d_vgraphics_get_view_extent()
{
    return gf3d_swapchain_get_extent();
}

GFC_Vector2D gf3d_vgraphics_get_resolution()
{
    return gf3d_vgraphics_get_view_extent_as_vector2d();
}

GFC_Vector2D gf3d_vgraphics_get_view_extent_as_vector2d()
{
    VkExtent2D extent;
    extent = gf3d_swapchain_get_extent();
    return gfc_vector2d(extent.width,extent.height);
}

SDL_Surface *gf3d_vgraphics_create_surface(Uint32 w,Uint32 h)
{
    SDL_Surface *surface;
    surface = SDL_CreateRGBSurface(
        0,w, h,
        gf3d_vgraphics.bitdepth,
        gf3d_vgraphics.rmask,
        gf3d_vgraphics.gmask,
        gf3d_vgraphics.bmask,
        gf3d_vgraphics.amask);
    return surface;
}


SDL_Surface *gf3d_vgraphics_screen_convert(SDL_Surface **surface)
{
    SDL_Surface *convert;
    if (!(*surface))
    {
        slog("surface provided was NULL");
        return NULL;
    }
    if (!gf3d_vgraphics.screen)
    {
        slog("graphics not yet initialized");
        return NULL;
    }
    convert = SDL_ConvertSurface(*surface,
                       gf3d_vgraphics.screen->format,
                       0);
    if (!convert)
    {
        slog("failed to convert surface: %s",SDL_GetError());
        return NULL;
    }
    SDL_FreeSurface(*surface);
    *surface = NULL;
    return convert;
}


Uint32 gf3d_vgraphics_render_begin()
{
    Uint32 imageIndex;
    VkSwapchainKHR swapChains[1] = {0};

    /*
    Acquire an image from the swap chain
    Execute the command buffer with that image as attachment in the framebuffer
    Return the image to the swap chain for presentation
    */
    swapChains[0] = gf3d_swapchain_get();
    
    vkAcquireNextImageKHR(
        gf3d_vgraphics.device,
        swapChains[0],
        UINT_MAX,
        gf3d_vgraphics.imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex);
    
    return imageIndex;
}

/**
 * Rendering wrapper
 * 
 */

void gf3d_vgraphics_render_start()
{
    gf3d_vgraphics.bufferFrame = gf3d_vgraphics_render_begin();
    gf3d_pipeline_reset_all_pipes();
}

Uint32  gf3d_vgraphics_get_current_buffer_frame()
{
    return gf3d_vgraphics.bufferFrame;
}

void gf3d_vgraphics_render_end()
{
    VkPresentInfoKHR presentInfo = {0};
    VkSubmitInfo submitInfo = {0};
    VkSwapchainKHR swapChains[1] = {0};
    VkSemaphore waitSemaphores[] = {gf3d_vgraphics.imageAvailableSemaphore};
    VkSemaphore signalSemaphores[] = {gf3d_vgraphics.renderFinishedSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    
    gf3d_pipeline_submit_all_pipe_commands();
    
    swapChains[0] = gf3d_swapchain_get();

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    //get count of configured command buffers
    //get the list of command buffers
    
    submitInfo.commandBufferCount = gf3d_command_pool_get_used_buffer_count(gf3d_vgraphics.graphicsCommandPool);
    submitInfo.pCommandBuffers = gf3d_command_pool_get_used_buffers(gf3d_vgraphics.graphicsCommandPool);
    
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    if (vkQueueSubmit(gf3d_vqueues_get_graphics_queue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
    {
        slog("failed to submit draw command buffer!");
    }
    
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &gf3d_vgraphics.bufferFrame;
    presentInfo.pResults = NULL; // Optional
    
    vkQueuePresentKHR(gf3d_vqueues_get_present_queue(), &presentInfo);
}

void gf3d_vgraphics_semaphores_close()
{
    vkDestroySemaphore(gf3d_vgraphics.device, gf3d_vgraphics.renderFinishedSemaphore, NULL);
    vkDestroySemaphore(gf3d_vgraphics.device, gf3d_vgraphics.imageAvailableSemaphore, NULL);
}

void gf3d_vgraphics_semaphores_create()
{
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    if ((vkCreateSemaphore(gf3d_vgraphics.device, &semaphoreInfo, NULL, &gf3d_vgraphics.imageAvailableSemaphore) != VK_SUCCESS) ||
        (vkCreateSemaphore(gf3d_vgraphics.device, &semaphoreInfo, NULL, &gf3d_vgraphics.renderFinishedSemaphore) != VK_SUCCESS))
    {
        slog("failed to create semaphores!");
    }
    atexit(gf3d_vgraphics_semaphores_close);
}

uint32_t gf3d_vgraphics_find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    
    vkGetPhysicalDeviceMemoryProperties(gf3d_vgraphics_get_default_physical_device(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties))
        {
            return i;
        }
    }

    slog("failed to find suitable memory type!");
    return 0;
}

void gf3d_vgraphics_get_projection_matrix(GFC_Matrix4 *proj)
{
    if (!proj)return;
    memcpy(proj,gf3d_vgraphics.ubo.proj,sizeof(GFC_Matrix4));
}

void gf3d_vgraphics_set_view(GFC_Matrix4 view)
{
    gfc_matrix4_copy(gf3d_vgraphics.ubo.view,view);
}

void gf3d_vgraphics_get_view(GFC_Matrix4 *view)
{
    if (!view)return;
    memcpy(view,gf3d_vgraphics.ubo.view,sizeof(GFC_Matrix4));
}


GFC_Matrix4 *gf3d_vgraphics_get_view_matrix()
{
    return &gf3d_vgraphics.ubo.view;
}

void gf3d_vgraphics_rotate_camera(float degrees)
{
    gfc_matrix4_rotate(
        gf3d_vgraphics.ubo.view,
        gf3d_vgraphics.ubo.view,
        degrees,
        gfc_vector3d(0,0,1));

}

Command *gf3d_vgraphics_get_graphics_command_pool()
{
    return gf3d_vgraphics.graphicsCommandPool;
}

ModelViewProjection gf3d_vgraphics_get_mvp()
{
    return gf3d_vgraphics.ubo;
}

VkImageView gf3d_vgraphics_create_image_view(VkImage image, VkFormat format)
{
    VkImageView imageView;
    VkImageViewCreateInfo viewInfo = {0};

    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(gf3d_vgraphics.device, &viewInfo, NULL, &imageView) != VK_SUCCESS)
    {
        slog("failed to create texture image view!");
        return VK_NULL_HANDLE;
    }

    return imageView;
}

GFC_Vector2D vgraphics_3d_position_to_screen(GFC_Vector3D position)
{
    GFC_Vector2D res,out;
    GFC_Matrix4 mvp,model;
    GFC_Vector4D transformed = {0};
    ModelViewProjection graphics_mvp;
    
    res = gf3d_vgraphics_get_resolution();
    
    gfc_matrix4_make_translation(
        model,
        position);
    graphics_mvp = gf3d_vgraphics_get_mvp();
    gfc_matrix4_multiply(
        mvp,
        graphics_mvp.view,
        graphics_mvp.proj
    );
    
    gfc_matrix4_v_multiply(
        &transformed,
        gfc_vector4d(position.x,position.y,position.z,1.0),
        mvp);
    
    out.x = (0.5 *(transformed.x / transformed.w) + 0.5)*res.x;
    out.y = (0.5 *(transformed.y / transformed.w) + 0.5)*res.y;
        
    return out;
}

GFC_Vector3D vgraphics_3d_position_to_screen_depth(GFC_Vector3D position)
{
    GFC_Vector2D res;
    GFC_Vector3D out;
    GFC_Matrix4 mvp,model;
    GFC_Vector4D transformed = {0};
    ModelViewProjection graphics_mvp;
    
    res = gf3d_vgraphics_get_resolution();
    
    gfc_matrix4_make_translation(
        model,
        position);
    graphics_mvp = gf3d_vgraphics_get_mvp();
    gfc_matrix4_multiply(
        mvp,
        graphics_mvp.view,
        graphics_mvp.proj
    );
    
    gfc_matrix4_v_multiply(
        &transformed,
        gfc_vector4d(position.x,position.y,position.z,1.0),
        mvp);
    
    out.x = (0.5 *(transformed.x / transformed.w) + 0.5)*res.x;
    out.y = (0.5 *(transformed.y / transformed.w) + 0.5)*res.y;
    out.z = transformed.z;
    return out;
}


/*eol@eof*/

