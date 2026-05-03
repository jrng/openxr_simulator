#ifndef TRACE_OPENXR_CALLS
#  define TRACE_OPENXR_CALLS 0
#endif

#ifndef PLATFORM_XLIB
#  define PLATFORM_XLIB 0
#endif

#ifndef PLATFORM_WIN32
#  define PLATFORM_WIN32 0
#endif

#ifndef PLATFORM_WAYLAND
#  define PLATFORM_WAYLAND 0
#endif

#ifndef GRAPHICS_API_D3D11
#  define GRAPHICS_API_D3D11 0
#endif

#ifndef GRAPHICS_API_OPENGL
#  define GRAPHICS_API_OPENGL 0
#endif

#if defined(_WIN32)

#  pragma comment(linker, "/export:xrNegotiateLoaderRuntimeInterface")

#  include <windows.h>

#endif

#if PLATFORM_XLIB

#define Font __Font

#  include <GL/glx.h>

#undef Font

#  define XR_USE_PLATFORM_XLIB

#endif

#if PLATFORM_WIN32

#  define XR_USE_PLATFORM_WIN32

#endif

#if PLATFORM_WAYLAND

#  include <wayland-client.h>

#  define XR_USE_PLATFORM_WAYLAND

#endif

#if GRAPHICS_API_D3D11

#  define COBJMACROS

#  include <d3d11.h>
#  include <dxgi1_2.h>
#  include <d3dcompiler.h>

#  undef COBJMACROS

#  define XR_USE_GRAPHICS_API_D3D11

#endif

#if GRAPHICS_API_OPENGL

#  include <GL/gl.h>

#  define XR_USE_GRAPHICS_API_OPENGL

#endif

#include <openxr/openxr_platform.h>
#include <openxr/openxr_loader_negotiation.h>

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

static FILE *log_file = NULL;

static inline void
msg(const char *format, ...)
{
    if (!log_file)
    {
        log_file = fopen("xrsim.log", "ab");
    }

    char buffer[255];

    va_list args;
    va_start(args, format);

    size_t ret = snprintf(buffer, sizeof(buffer), "[xrsim] ");
    char *file_buffer = buffer + ret;
    vsnprintf(buffer + ret, sizeof(buffer) - ret, format, args);

    va_end(args);

#if defined(_WIN32)
    OutputDebugStringA(buffer);
#else
    fputs(buffer, stderr);
#endif

    if (log_file)
    {
        fputs(file_buffer, log_file);
        fflush(log_file);
    }
}

#if TRACE_OPENXR_CALLS
#  define TRACE_ENTER() msg("enter %s\n", __func__)
// TODO: print result
#  define TRACE_LEAVE_RESULT(result) msg("leave %s\n", __func__); return result
#else
#  define TRACE_ENTER()
#  define TRACE_LEAVE_RESULT(result) return result
#endif

#define ArrayCount(array) (sizeof(array)/sizeof((array)[0]))

typedef struct
{
    uint64_t count;
    uint8_t *data;
} String;

#define S(str) (String) { .count = sizeof(str) - 1, .data = (uint8_t *) (str) }
#define C(str) (String) { .count = get_c_string_length(str), .data = (uint8_t *) (str) }

static inline uint64_t
get_c_string_length(const char *str)
{
    uint64_t length = 0;

    if (str)
    {
        while (*str++) length += 1;
    }

    return length;
}

static inline bool
strings_are_equal(String a, String b)
{
    if (a.count != b.count)
    {
        return false;
    }

    for (uint64_t i = 0; i < a.count; i += 1)
    {
        if (a.data[i] != b.data[i])
        {
            return false;
        }
    }

    return true;
}

static inline XrVector3f
vec3_add(XrVector3f a, XrVector3f b)
{
    return (XrVector3f) { a.x + b.x, a.y + b.y, a.z + b.z };
}

static inline XrVector3f
vec3_scale(float s, XrVector3f v)
{
    return (XrVector3f) { s * v.x, s * v.y, s * v.z };
}

static inline XrVector3f
vec3_cross(XrVector3f a, XrVector3f b)
{
    return (XrVector3f) { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

static inline XrVector3f
quaternion_apply(XrQuaternionf q, XrVector3f v)
{
    XrVector3f xyz = { q.x, q.y, q.z };
    return vec3_add(v, vec3_scale(2.0f, vec3_cross(xyz, vec3_add(vec3_cross(xyz, v), vec3_scale(q.w, v)))));
}

static inline XrQuaternionf
quaternion_from_orbit_and_pitch(float orbit, float pitch)
{
    float cos_orbit = cosf(0.5f * orbit);
    float sin_orbit = sinf(0.5f * orbit);
    float cos_pitch = cosf(0.5f * pitch);
    float sin_pitch = sinf(0.5f * pitch);

    XrQuaternionf quaternion;
    quaternion.x =  cos_orbit * sin_pitch;
    quaternion.y =  sin_orbit * cos_pitch;
    quaternion.z = -sin_orbit * sin_pitch;
    quaternion.w =  cos_orbit * cos_pitch;

    return quaternion;
}

#define SWAPCHAIN_IMAGE_COUNT 3

#if GRAPHICS_API_D3D11
#  include "graphics_api_d3d11.h"
#endif

#if GRAPHICS_API_OPENGL
#  include "graphics_api_opengl.h"
#endif

#if PLATFORM_XLIB
#  include "platform_xlib.h"
#endif

#if PLATFORM_WIN32
#  include "platform_win32.h"
#endif

#if PLATFORM_WAYLAND
#  include "platform_wayland.c"
#endif

#include "terminus_16_bold.h"

#define INSTANCE_HANDLE ((XrInstance) 0x0000ABCD00000000)
#define SYSTEM_HANDLE   ((XrSystemId) 0x0000ABCD00000001)
#define SESSION_HANDLE  ((XrSession)  0x0000ABCD00000002)

#define ACTION_HANDLE_OFFSET     0x0000ABCD00A00000
#define ACTION_SET_HANDLE_OFFSET 0x0000ABCD00A50000

#define SPACE_HANDLE_OFFSET      0x0000ABCD00500000

#define MAX_PATH_STRING_COUNT 128

static const int32_t EYE_WIDTH  = 960;
static const int32_t EYE_HEIGHT = 960;
static const int32_t TARGET_FRAME_RATE = 90;

typedef enum
{
    PlatformXlib    = 0,
    PlatformWin32   = 1,
    PlatformWayland = 2,
} Platform;

typedef enum
{
    GraphicsApiUnknown = 0,
    GraphicsApiD3D11   = 1,
    GraphicsApiOpenGl  = 2,
} GraphicsApi;

typedef struct
{
    float x, y;
    float u, v;
    uint32_t color;
} Vertex;

typedef union
{
    uint64_t _data;

#if GRAPHICS_API_D3D11
    struct
    {
        ID3D11Texture2D *texture;
        ID3D11ShaderResourceView *texture_view;
    } d3d11;
#endif
#if GRAPHICS_API_OPENGL
    GLuint opengl;
#endif
} Texture;

typedef struct
{
    uint32_t vertex_offset;
    uint32_t vertex_count;
    Texture texture;
} DrawCommand;

#define MAX_VERTEX_COUNT (6 * 128)

typedef struct
{
    uint32_t vertex_count;
    uint32_t max_vertex_count;
    Vertex *vertices;

    uint32_t command_count;
    uint32_t max_command_count;
    DrawCommand *commands;

    float x_scale;
    float y_scale;
} DrawContext;

static void
set_texture(DrawContext *ctx, Texture texture)
{
    if ((ctx->command_count + 1) > ctx->max_command_count)
    {
        return;
    }

    DrawCommand *command = ctx->commands + ctx->command_count;
    ctx->command_count += 1;

    command->vertex_offset = ctx->vertex_count;
    command->vertex_count = 0;
    command->texture = texture;
}

static void
push_quad(DrawContext *ctx, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t color)
{
    if (ctx->command_count == 0)
    {
        return;
    }

    if ((ctx->vertex_count + 6) > ctx->max_vertex_count)
    {
        return;
    }

    x0 = ctx->x_scale * x0 - 1.0f;
    y0 = ctx->y_scale * y0 + 1.0f;
    x1 = ctx->x_scale * x1 - 1.0f;
    y1 = ctx->y_scale * y1 + 1.0f;

    DrawCommand *command = ctx->commands + (ctx->command_count - 1);
    command->vertex_count += 6;

    Vertex *vertices = ctx->vertices + ctx->vertex_count;
    ctx->vertex_count += 6;

    vertices[0].x = x0;
    vertices[0].y = y1;
    vertices[0].u = u0;
    vertices[0].v = v1;
    vertices[0].color = color;

    vertices[1].x = x0;
    vertices[1].y = y0;
    vertices[1].u = u0;
    vertices[1].v = v0;
    vertices[1].color = color;

    vertices[2].x = x1;
    vertices[2].y = y0;
    vertices[2].u = u1;
    vertices[2].v = v0;
    vertices[2].color = color;

    vertices[3].x = x0;
    vertices[3].y = y1;
    vertices[3].u = u0;
    vertices[3].v = v1;
    vertices[3].color = color;

    vertices[4].x = x1;
    vertices[4].y = y0;
    vertices[4].u = u1;
    vertices[4].v = v0;
    vertices[4].color = color;

    vertices[5].x = x1;
    vertices[5].y = y1;
    vertices[5].u = u1;
    vertices[5].v = v1;
    vertices[5].color = color;
}

static void
draw_string(DrawContext *ctx, Font *font, float x, float y, const char *str, uint32_t color)
{
    float u_scale = 1.0f / (float) font->texture_width;
    float v_scale = 1.0f / (float) font->texture_height;

    while (*str)
    {
        uint32_t codepoint = *str++;

        Glyph *glyph = NULL;

        for (uint32_t i = 0; i < font->glyph_count; i += 1)
        {
            Glyph *g = font->glyphs + i;

            if (g->codepoint == codepoint)
            {
                glyph = g;
                break;
            }
        }

        if (glyph)
        {
            float u0 = u_scale * (float) glyph->u;
            float v0 = v_scale * (float) glyph->v;
            float u1 = u_scale * (float) (glyph->u + glyph->bound_width);
            float v1 = v_scale * (float) (glyph->v + glyph->bound_height);

            push_quad(ctx, x + glyph->x_offset, y - glyph->y_offset - glyph->bound_height, x + glyph->x_offset + glyph->bound_width, y - glyph->y_offset, u0, v0, u1, v1, color);
            x += (float) glyph->x_advance;
        }
    }
}

typedef struct
{
    String name;
    uint32_t version;
} Extension;

typedef struct
{
    bool active;
    uint16_t generation;

    uint32_t next_image_index;

    union
    {
#if GRAPHICS_API_D3D11
        D3D11Swapchain d3d11;
#endif
#if GRAPHICS_API_OPENGL
        OpenGlSwapchain opengl;
#endif
    };
} Swapchain;

typedef struct
{
    bool active;

#if PLATFORM_WIN32
    bool enabled_XR_KHR_win32_convert_performance_counter_time;
#endif

#if GRAPHICS_API_D3D11
    bool enabled_XR_KHR_d3d11_enable;
#endif

#if GRAPHICS_API_OPENGL
    bool enabled_XR_KHR_opengl_enable;
#endif

    GraphicsApi graphics_api;

    uintptr_t next_action_handle;
    uintptr_t next_action_set_handle;

    uint32_t event_read;
    uint32_t event_write;

    XrEventDataBuffer event_queue[4];

    // TODO: actually store these
    // String path_strings[MAX_PATH_STRING_COUNT];
} Instance;

typedef struct
{
    bool active;

    XrSessionState state;

    int32_t width;
    int32_t height;

    uintptr_t next_space_handle;

    Texture font_texture;

    Platform platform;

    union
    {
#if PLATFORM_XLIB
        PlatformXlibState xlib;
#endif

#if PLATFORM_WIN32
        PlatformWin32State win32;
#endif

#if PLATFORM_WAYLAND
        PlatformWaylandState wayland;
#endif
    };

    float head_orbit;
    float head_pitch;

    XrVector3f head_position;

    Swapchain swapchains[8];
} Session;

typedef struct
{
    bool initialized;

    Instance instance;
    Session session;

    uint32_t supported_extension_count;
    Extension supported_extensions[8];
} RuntimeState;

static RuntimeState state;

#if GRAPHICS_API_D3D11
#  include "graphics_api_d3d11.c"
#endif

#if GRAPHICS_API_OPENGL
#  include "graphics_api_opengl.c"
#endif

#if PLATFORM_XLIB
#  include "platform_xlib.c"
#endif

#if PLATFORM_WIN32
#  include "platform_win32.c"
#endif

static XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateInstanceExtensionProperties_impl(const char *layer_name, uint32_t property_capacity, uint32_t *property_count, XrExtensionProperties *properties)
{
    TRACE_ENTER();

    if (!property_count)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    *property_count = state.supported_extension_count;

    if (property_capacity > 0)
    {
        if (property_capacity < state.supported_extension_count)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_SIZE_INSUFFICIENT);
        }

        if (!properties)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
        }

        for (uint32_t i = 0; i < state.supported_extension_count; i += 1)
        {
            String name = state.supported_extensions[i].name;

            if (name.count >= XR_MAX_EXTENSION_NAME_SIZE)
            {
                name.count = XR_MAX_EXTENSION_NAME_SIZE - 1;
            }

            properties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
            properties[i].next = NULL;
            memcpy(properties[i].extensionName, name.data, name.count);
            properties[i].extensionName[name.count] = 0;
            properties[i].extensionVersion = state.supported_extensions[i].version;
        }
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateApiLayerProperties_impl(uint32_t property_capacity, uint32_t *property_count, XrApiLayerProperties *properties)
{
    TRACE_ENTER();

    if (property_count)
    {
        *property_count = 0;
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrCreateInstance_impl(const XrInstanceCreateInfo *create_info, XrInstance *instance)
{
    TRACE_ENTER();

    if (!create_info || !instance)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((create_info->type != XR_TYPE_INSTANCE_CREATE_INFO) ||
        create_info->next ||
        (create_info->createFlags != 0))
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if (create_info->enabledApiLayerCount > 0)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_API_LAYER_NOT_PRESENT);
    }

    if ((create_info->enabledExtensionCount > 0) &&
        !create_info->enabledExtensionNames)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if (state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_LIMIT_REACHED);
    }

#if PLATFORM_WIN32
    state.instance.enabled_XR_KHR_win32_convert_performance_counter_time = false;
#endif

#if GRAPHICS_API_D3D11
    state.instance.enabled_XR_KHR_d3d11_enable = false;
#endif

#if GRAPHICS_API_OPENGL
    state.instance.enabled_XR_KHR_opengl_enable = false;
#endif

    msg("app requested %u extensions to be enabled:\n", create_info->enabledExtensionCount);

    for (uint32_t i = 0; i < create_info->enabledExtensionCount; i += 1)
    {
        Extension *extension = state.supported_extensions + i;
        msg("  - %.*s\n", (int) extension->name.count, extension->name.data);
    }

    for (uint32_t i = 0; i < create_info->enabledExtensionCount; i += 1)
    {
        bool supported = false;
        String enabled_extension = C(create_info->enabledExtensionNames[i]);

#if PLATFORM_WIN32
        if (strings_are_equal(enabled_extension, S(XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME)))
        {
            state.instance.enabled_XR_KHR_win32_convert_performance_counter_time = true;
        }
#endif

#if GRAPHICS_API_D3D11
        if (strings_are_equal(enabled_extension, S(XR_KHR_D3D11_ENABLE_EXTENSION_NAME)))
        {
            state.instance.enabled_XR_KHR_d3d11_enable = true;
        }
#endif

#if GRAPHICS_API_OPENGL
        if (strings_are_equal(enabled_extension, S(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME)))
        {
            state.instance.enabled_XR_KHR_opengl_enable = true;
        }
#endif

        for (uint32_t j = 0; j < state.supported_extension_count; j += 1)
        {
            if (strings_are_equal(enabled_extension, state.supported_extensions[j].name))
            {
                supported = true;
                break;
            }
        }

        if (!supported)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_EXTENSION_NOT_PRESENT);
        }
    }

    state.instance.active = true;
    state.instance.graphics_api = GraphicsApiUnknown;
    state.instance.next_action_handle = ACTION_HANDLE_OFFSET;
    state.instance.next_action_set_handle = ACTION_SET_HANDLE_OFFSET;

    *instance = INSTANCE_HANDLE;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrDestroyInstance_impl(XrInstance instance)
{
    TRACE_ENTER();

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    // TODO: implement

    state.instance.active = false;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetInstanceProperties_impl(XrInstance instance, XrInstanceProperties* instance_properties)
{
    TRACE_ENTER();

    if (!instance_properties)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    instance_properties->type           = XR_TYPE_INSTANCE_PROPERTIES;
    instance_properties->next           = NULL;
    instance_properties->runtimeVersion = XR_MAKE_VERSION(1, 0, 0);
    strncpy(instance_properties->runtimeName, "openxr_simulator/runtime", XR_MAX_RUNTIME_NAME_SIZE);
    instance_properties->runtimeName[XR_MAX_RUNTIME_NAME_SIZE - 1] = 0;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XrEventDataBuffer *
push_event(void)
{
    XrEventDataBuffer *event = NULL;

    if ((state.instance.event_write - state.instance.event_read) < ArrayCount(state.instance.event_queue))
    {
        uint32_t index = state.instance.event_write & (ArrayCount(state.instance.event_queue) - 1);
        state.instance.event_write += 1;

        event = state.instance.event_queue + index;
    }
    else
    {
        msg("EVENT QUEUE OVERFLOW\n");
    }

    return event;
}

static void
change_state(XrSessionState session_state)
{
    if (!state.session.active)
    {
        return;
    }

    state.session.state = session_state;

    XrEventDataBuffer *event = push_event();

    if (event)
    {
        XrEventDataSessionStateChanged *state_changed_event = (XrEventDataSessionStateChanged *) event;

        state_changed_event->type    = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
        state_changed_event->next    = NULL;
        state_changed_event->session = SESSION_HANDLE;
        state_changed_event->state   = session_state;
        state_changed_event->time    = 0;
    }
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrPollEvent_impl(XrInstance instance, XrEventDataBuffer *event)
{
    TRACE_ENTER();

    if (!event)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (state.instance.event_read < state.instance.event_write)
    {
        uint32_t index = state.instance.event_read & (ArrayCount(state.instance.event_queue) - 1);

        state.instance.event_read += 1;

        if (state.instance.event_read >= ArrayCount(state.instance.event_queue))
        {
            state.instance.event_read -= ArrayCount(state.instance.event_queue);
            state.instance.event_write -= ArrayCount(state.instance.event_queue);
        }

        *event = state.instance.event_queue[index];

        msg("event !!!\n");
        TRACE_LEAVE_RESULT(XR_SUCCESS);
    }
    else
    {
        static int count = 0;
        if (count < 10)
        {
            msg("event unavailable\n");
            count += 1;
        }
        TRACE_LEAVE_RESULT(XR_EVENT_UNAVAILABLE);
    }
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrResultToString_impl(XrInstance instance, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE])
{
    TRACE_ENTER();

    (void) instance;

#define RESULT(result) case result: strncpy(buffer, #result, XR_MAX_RESULT_STRING_SIZE); break

    switch (value)
    {
        RESULT(XR_SUCCESS);
        RESULT(XR_TIMEOUT_EXPIRED);
        RESULT(XR_SESSION_LOSS_PENDING);
        RESULT(XR_EVENT_UNAVAILABLE);
        RESULT(XR_SPACE_BOUNDS_UNAVAILABLE);
        RESULT(XR_SESSION_NOT_FOCUSED);
        RESULT(XR_FRAME_DISCARDED);
        RESULT(XR_ERROR_VALIDATION_FAILURE);
        RESULT(XR_ERROR_RUNTIME_FAILURE);
        RESULT(XR_ERROR_OUT_OF_MEMORY);
        RESULT(XR_ERROR_API_VERSION_UNSUPPORTED);
        RESULT(XR_ERROR_INITIALIZATION_FAILED);
        RESULT(XR_ERROR_FUNCTION_UNSUPPORTED);
        RESULT(XR_ERROR_FEATURE_UNSUPPORTED);
        RESULT(XR_ERROR_EXTENSION_NOT_PRESENT);
        RESULT(XR_ERROR_LIMIT_REACHED);
        RESULT(XR_ERROR_SIZE_INSUFFICIENT);
        RESULT(XR_ERROR_HANDLE_INVALID);
        RESULT(XR_ERROR_INSTANCE_LOST);
        RESULT(XR_ERROR_SESSION_RUNNING);
        RESULT(XR_ERROR_SESSION_NOT_RUNNING);
        RESULT(XR_ERROR_SESSION_LOST);
        RESULT(XR_ERROR_SYSTEM_INVALID);
        RESULT(XR_ERROR_PATH_INVALID);
        RESULT(XR_ERROR_PATH_COUNT_EXCEEDED);
        RESULT(XR_ERROR_PATH_FORMAT_INVALID);
        RESULT(XR_ERROR_PATH_UNSUPPORTED);
        RESULT(XR_ERROR_LAYER_INVALID);
        RESULT(XR_ERROR_LAYER_LIMIT_EXCEEDED);
        RESULT(XR_ERROR_SWAPCHAIN_RECT_INVALID);
        RESULT(XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED);
        RESULT(XR_ERROR_ACTION_TYPE_MISMATCH);
        RESULT(XR_ERROR_SESSION_NOT_READY);
        RESULT(XR_ERROR_SESSION_NOT_STOPPING);
        RESULT(XR_ERROR_TIME_INVALID);
        RESULT(XR_ERROR_REFERENCE_SPACE_UNSUPPORTED);
        RESULT(XR_ERROR_FILE_ACCESS_ERROR);
        RESULT(XR_ERROR_FILE_CONTENTS_INVALID);
        RESULT(XR_ERROR_FORM_FACTOR_UNSUPPORTED);
        RESULT(XR_ERROR_FORM_FACTOR_UNAVAILABLE);
        RESULT(XR_ERROR_API_LAYER_NOT_PRESENT);
        RESULT(XR_ERROR_CALL_ORDER_INVALID);
        RESULT(XR_ERROR_GRAPHICS_DEVICE_INVALID);
        RESULT(XR_ERROR_POSE_INVALID);
        RESULT(XR_ERROR_INDEX_OUT_OF_RANGE);
        RESULT(XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED);
        RESULT(XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED);
        RESULT(XR_ERROR_NAME_DUPLICATED);
        RESULT(XR_ERROR_NAME_INVALID);
        RESULT(XR_ERROR_ACTIONSET_NOT_ATTACHED);
        RESULT(XR_ERROR_ACTIONSETS_ALREADY_ATTACHED);
        RESULT(XR_ERROR_LOCALIZED_NAME_DUPLICATED);
        RESULT(XR_ERROR_LOCALIZED_NAME_INVALID);
        RESULT(XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING);
        RESULT(XR_ERROR_RUNTIME_UNAVAILABLE);
        RESULT(XR_ERROR_EXTENSION_DEPENDENCY_NOT_ENABLED);
        RESULT(XR_ERROR_PERMISSION_INSUFFICIENT);
        RESULT(XR_ERROR_ANDROID_THREAD_SETTINGS_ID_INVALID_KHR);
        RESULT(XR_ERROR_ANDROID_THREAD_SETTINGS_FAILURE_KHR);
        RESULT(XR_ERROR_CREATE_SPATIAL_ANCHOR_FAILED_MSFT);
        RESULT(XR_ERROR_SECONDARY_VIEW_CONFIGURATION_TYPE_NOT_ENABLED_MSFT);
        RESULT(XR_ERROR_CONTROLLER_MODEL_KEY_INVALID_MSFT);
        RESULT(XR_ERROR_REPROJECTION_MODE_UNSUPPORTED_MSFT);
        RESULT(XR_ERROR_COMPUTE_NEW_SCENE_NOT_COMPLETED_MSFT);
        RESULT(XR_ERROR_SCENE_COMPONENT_ID_INVALID_MSFT);
        RESULT(XR_ERROR_SCENE_COMPONENT_TYPE_MISMATCH_MSFT);
        RESULT(XR_ERROR_SCENE_MESH_BUFFER_ID_INVALID_MSFT);
        RESULT(XR_ERROR_SCENE_COMPUTE_FEATURE_INCOMPATIBLE_MSFT);
        RESULT(XR_ERROR_SCENE_COMPUTE_CONSISTENCY_MISMATCH_MSFT);
        RESULT(XR_ERROR_DISPLAY_REFRESH_RATE_UNSUPPORTED_FB);
        RESULT(XR_ERROR_COLOR_SPACE_UNSUPPORTED_FB);
        RESULT(XR_ERROR_SPACE_COMPONENT_NOT_SUPPORTED_FB);
        RESULT(XR_ERROR_SPACE_COMPONENT_NOT_ENABLED_FB);
        RESULT(XR_ERROR_SPACE_COMPONENT_STATUS_PENDING_FB);
        RESULT(XR_ERROR_SPACE_COMPONENT_STATUS_ALREADY_SET_FB);
        RESULT(XR_ERROR_UNEXPECTED_STATE_PASSTHROUGH_FB);
        RESULT(XR_ERROR_FEATURE_ALREADY_CREATED_PASSTHROUGH_FB);
        RESULT(XR_ERROR_FEATURE_REQUIRED_PASSTHROUGH_FB);
        RESULT(XR_ERROR_NOT_PERMITTED_PASSTHROUGH_FB);
        RESULT(XR_ERROR_INSUFFICIENT_RESOURCES_PASSTHROUGH_FB);
        RESULT(XR_ERROR_UNKNOWN_PASSTHROUGH_FB);
        RESULT(XR_ERROR_RENDER_MODEL_KEY_INVALID_FB);
        RESULT(XR_RENDER_MODEL_UNAVAILABLE_FB);
        RESULT(XR_ERROR_MARKER_NOT_TRACKED_VARJO);
        RESULT(XR_ERROR_MARKER_ID_INVALID_VARJO);
        RESULT(XR_ERROR_MARKER_DETECTOR_PERMISSION_DENIED_ML);
        RESULT(XR_ERROR_MARKER_DETECTOR_LOCATE_FAILED_ML);
        RESULT(XR_ERROR_MARKER_DETECTOR_INVALID_DATA_QUERY_ML);
        RESULT(XR_ERROR_MARKER_DETECTOR_INVALID_CREATE_INFO_ML);
        RESULT(XR_ERROR_MARKER_INVALID_ML);
        RESULT(XR_ERROR_LOCALIZATION_MAP_INCOMPATIBLE_ML);
        RESULT(XR_ERROR_LOCALIZATION_MAP_UNAVAILABLE_ML);
        RESULT(XR_ERROR_LOCALIZATION_MAP_FAIL_ML);
        RESULT(XR_ERROR_LOCALIZATION_MAP_IMPORT_EXPORT_PERMISSION_DENIED_ML);
        RESULT(XR_ERROR_LOCALIZATION_MAP_PERMISSION_DENIED_ML);
        RESULT(XR_ERROR_LOCALIZATION_MAP_ALREADY_EXISTS_ML);
        RESULT(XR_ERROR_LOCALIZATION_MAP_CANNOT_EXPORT_CLOUD_MAP_ML);
        RESULT(XR_ERROR_SPATIAL_ANCHORS_PERMISSION_DENIED_ML);
        RESULT(XR_ERROR_SPATIAL_ANCHORS_NOT_LOCALIZED_ML);
        RESULT(XR_ERROR_SPATIAL_ANCHORS_OUT_OF_MAP_BOUNDS_ML);
        RESULT(XR_ERROR_SPATIAL_ANCHORS_SPACE_NOT_LOCATABLE_ML);
        RESULT(XR_ERROR_SPATIAL_ANCHORS_ANCHOR_NOT_FOUND_ML);
        RESULT(XR_ERROR_SPATIAL_ANCHOR_NAME_NOT_FOUND_MSFT);
        RESULT(XR_ERROR_SPATIAL_ANCHOR_NAME_INVALID_MSFT);
        RESULT(XR_SCENE_MARKER_DATA_NOT_STRING_MSFT);
        RESULT(XR_ERROR_SPACE_MAPPING_INSUFFICIENT_FB);
        RESULT(XR_ERROR_SPACE_LOCALIZATION_FAILED_FB);
        RESULT(XR_ERROR_SPACE_NETWORK_TIMEOUT_FB);
        RESULT(XR_ERROR_SPACE_NETWORK_REQUEST_FAILED_FB);
        RESULT(XR_ERROR_SPACE_CLOUD_STORAGE_DISABLED_FB);
        RESULT(XR_ERROR_SPACE_INSUFFICIENT_RESOURCES_META);
        RESULT(XR_ERROR_SPACE_STORAGE_AT_CAPACITY_META);
        RESULT(XR_ERROR_SPACE_INSUFFICIENT_VIEW_META);
        RESULT(XR_ERROR_SPACE_PERMISSION_INSUFFICIENT_META);
        RESULT(XR_ERROR_SPACE_RATE_LIMITED_META);
        RESULT(XR_ERROR_SPACE_TOO_DARK_META);
        RESULT(XR_ERROR_SPACE_TOO_BRIGHT_META);
        RESULT(XR_ERROR_PASSTHROUGH_COLOR_LUT_BUFFER_SIZE_MISMATCH_META);
        RESULT(XR_ENVIRONMENT_DEPTH_NOT_AVAILABLE_META);
        RESULT(XR_ERROR_RENDER_MODEL_ID_INVALID_EXT);
        RESULT(XR_ERROR_RENDER_MODEL_ASSET_UNAVAILABLE_EXT);
        RESULT(XR_ERROR_RENDER_MODEL_GLTF_EXTENSION_REQUIRED_EXT);
        RESULT(XR_ERROR_NOT_INTERACTION_RENDER_MODEL_EXT);
        RESULT(XR_ERROR_HINT_ALREADY_SET_QCOM);
        RESULT(XR_ERROR_NOT_AN_ANCHOR_HTC);
        RESULT(XR_ERROR_SPATIAL_ENTITY_ID_INVALID_BD);
        RESULT(XR_ERROR_SPATIAL_SENSING_SERVICE_UNAVAILABLE_BD);
        RESULT(XR_ERROR_ANCHOR_NOT_SUPPORTED_FOR_ENTITY_BD);
        RESULT(XR_ERROR_SPATIAL_ANCHOR_NOT_FOUND_BD);
        RESULT(XR_ERROR_SPATIAL_ANCHOR_SHARING_NETWORK_TIMEOUT_BD);
        RESULT(XR_ERROR_SPATIAL_ANCHOR_SHARING_AUTHENTICATION_FAILURE_BD);
        RESULT(XR_ERROR_SPATIAL_ANCHOR_SHARING_NETWORK_FAILURE_BD);
        RESULT(XR_ERROR_SPATIAL_ANCHOR_SHARING_LOCALIZATION_FAIL_BD);
        RESULT(XR_ERROR_SPATIAL_ANCHOR_SHARING_MAP_INSUFFICIENT_BD);
        RESULT(XR_ERROR_SCENE_CAPTURE_FAILURE_BD);
        RESULT(XR_ERROR_SPACE_NOT_LOCATABLE_EXT);
        RESULT(XR_ERROR_PLANE_DETECTION_PERMISSION_DENIED_EXT);
        RESULT(XR_ERROR_MISMATCHING_TRACKABLE_TYPE_ANDROID);
        RESULT(XR_ERROR_TRACKABLE_TYPE_NOT_SUPPORTED_ANDROID);
        RESULT(XR_ERROR_ANCHOR_ID_NOT_FOUND_ANDROID);
        RESULT(XR_ERROR_ANCHOR_ALREADY_PERSISTED_ANDROID);
        RESULT(XR_ERROR_ANCHOR_NOT_TRACKING_ANDROID);
        RESULT(XR_ERROR_PERSISTED_DATA_NOT_READY_ANDROID);
        RESULT(XR_ERROR_FUTURE_PENDING_EXT);
        RESULT(XR_ERROR_FUTURE_INVALID_EXT);
        RESULT(XR_ERROR_SYSTEM_NOTIFICATION_PERMISSION_DENIED_ML);
        RESULT(XR_ERROR_SYSTEM_NOTIFICATION_INCOMPATIBLE_SKU_ML);
        RESULT(XR_ERROR_WORLD_MESH_DETECTOR_PERMISSION_DENIED_ML);
        RESULT(XR_ERROR_WORLD_MESH_DETECTOR_SPACE_NOT_LOCATABLE_ML);
        RESULT(XR_ERROR_FACIAL_EXPRESSION_PERMISSION_DENIED_ML);
        RESULT(XR_ERROR_COLOCATION_DISCOVERY_NETWORK_FAILED_META);
        RESULT(XR_ERROR_COLOCATION_DISCOVERY_NO_DISCOVERY_METHOD_META);
        RESULT(XR_COLOCATION_DISCOVERY_ALREADY_ADVERTISING_META);
        RESULT(XR_COLOCATION_DISCOVERY_ALREADY_DISCOVERING_META);
        RESULT(XR_ERROR_SPACE_GROUP_NOT_FOUND_META);
        RESULT(XR_ERROR_ANCHOR_NOT_OWNED_BY_CALLER_ANDROID);
        RESULT(XR_ERROR_SPATIAL_CAPABILITY_UNSUPPORTED_EXT);
        RESULT(XR_ERROR_SPATIAL_ENTITY_ID_INVALID_EXT);
        RESULT(XR_ERROR_SPATIAL_BUFFER_ID_INVALID_EXT);
        RESULT(XR_ERROR_SPATIAL_COMPONENT_UNSUPPORTED_FOR_CAPABILITY_EXT);
        RESULT(XR_ERROR_SPATIAL_CAPABILITY_CONFIGURATION_INVALID_EXT);
        RESULT(XR_ERROR_SPATIAL_COMPONENT_NOT_ENABLED_EXT);
        RESULT(XR_ERROR_SPATIAL_PERSISTENCE_SCOPE_UNSUPPORTED_EXT);
        RESULT(XR_ERROR_SPATIAL_PERSISTENCE_SCOPE_INCOMPATIBLE_EXT);

        default:
        {
            if (value < 0)
            {
                snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_FAILURE_%d", value);
            }
            else
            {
                snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_SUCCESS_%d", value);
            }
        } break;
    }

#undef RESULT

    buffer[XR_MAX_RESULT_STRING_SIZE - 1] = 0;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrStructureTypeToString_impl(XrInstance instance, XrStructureType value, char buffer[XR_MAX_STRUCTURE_NAME_SIZE])
{
    TRACE_ENTER();

    (void) instance;

#define TYPE(type) case type: strncpy(buffer, #type, XR_MAX_STRUCTURE_NAME_SIZE); break

    switch (value)
    {
        TYPE(XR_TYPE_UNKNOWN);
        TYPE(XR_TYPE_API_LAYER_PROPERTIES);
        TYPE(XR_TYPE_EXTENSION_PROPERTIES);
        TYPE(XR_TYPE_INSTANCE_CREATE_INFO);
        TYPE(XR_TYPE_SYSTEM_GET_INFO);
        TYPE(XR_TYPE_SYSTEM_PROPERTIES);
        TYPE(XR_TYPE_VIEW_LOCATE_INFO);
        TYPE(XR_TYPE_VIEW);
        TYPE(XR_TYPE_SESSION_CREATE_INFO);
        TYPE(XR_TYPE_SWAPCHAIN_CREATE_INFO);
        TYPE(XR_TYPE_SESSION_BEGIN_INFO);
        TYPE(XR_TYPE_VIEW_STATE);
        TYPE(XR_TYPE_FRAME_END_INFO);
        TYPE(XR_TYPE_HAPTIC_VIBRATION);
        TYPE(XR_TYPE_EVENT_DATA_BUFFER);
        TYPE(XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING);
        TYPE(XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED);
        TYPE(XR_TYPE_ACTION_STATE_BOOLEAN);
        TYPE(XR_TYPE_ACTION_STATE_FLOAT);
        TYPE(XR_TYPE_ACTION_STATE_VECTOR2F);
        TYPE(XR_TYPE_ACTION_STATE_POSE);
        TYPE(XR_TYPE_ACTION_SET_CREATE_INFO);
        TYPE(XR_TYPE_ACTION_CREATE_INFO);
        TYPE(XR_TYPE_INSTANCE_PROPERTIES);
        TYPE(XR_TYPE_FRAME_WAIT_INFO);
        TYPE(XR_TYPE_COMPOSITION_LAYER_PROJECTION);
        TYPE(XR_TYPE_COMPOSITION_LAYER_QUAD);
        TYPE(XR_TYPE_REFERENCE_SPACE_CREATE_INFO);
        TYPE(XR_TYPE_ACTION_SPACE_CREATE_INFO);
        TYPE(XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING);
        TYPE(XR_TYPE_VIEW_CONFIGURATION_VIEW);
        TYPE(XR_TYPE_SPACE_LOCATION);
        TYPE(XR_TYPE_SPACE_VELOCITY);
        TYPE(XR_TYPE_FRAME_STATE);
        TYPE(XR_TYPE_VIEW_CONFIGURATION_PROPERTIES);
        TYPE(XR_TYPE_FRAME_BEGIN_INFO);
        TYPE(XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW);
        TYPE(XR_TYPE_EVENT_DATA_EVENTS_LOST);
        TYPE(XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING);
        TYPE(XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED);
        TYPE(XR_TYPE_INTERACTION_PROFILE_STATE);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO);
        TYPE(XR_TYPE_ACTION_STATE_GET_INFO);
        TYPE(XR_TYPE_HAPTIC_ACTION_INFO);
        TYPE(XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO);
        TYPE(XR_TYPE_ACTIONS_SYNC_INFO);
        TYPE(XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO);
        TYPE(XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO);
        TYPE(XR_TYPE_SPACES_LOCATE_INFO);
        TYPE(XR_TYPE_SPACE_LOCATIONS);
        TYPE(XR_TYPE_SPACE_VELOCITIES);
        TYPE(XR_TYPE_COMPOSITION_LAYER_CUBE_KHR);
        TYPE(XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR);
        TYPE(XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR);
        TYPE(XR_TYPE_VULKAN_SWAPCHAIN_FORMAT_LIST_CREATE_INFO_KHR);
        TYPE(XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT);
        TYPE(XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR);
        TYPE(XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR);
        TYPE(XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
        TYPE(XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT);
        TYPE(XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
        TYPE(XR_TYPE_DEBUG_UTILS_LABEL_EXT);
        TYPE(XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR);
        TYPE(XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR);
        TYPE(XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR);
        TYPE(XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR);
        TYPE(XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR);
        TYPE(XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR);
        TYPE(XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR);
        TYPE(XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR);
        TYPE(XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR);
        TYPE(XR_TYPE_GRAPHICS_BINDING_D3D11_KHR);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR);
        TYPE(XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR);
        TYPE(XR_TYPE_GRAPHICS_BINDING_D3D12_KHR);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR);
        TYPE(XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR);
        TYPE(XR_TYPE_GRAPHICS_BINDING_METAL_KHR);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR);
        TYPE(XR_TYPE_GRAPHICS_REQUIREMENTS_METAL_KHR);
        TYPE(XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT);
        TYPE(XR_TYPE_EYE_GAZE_SAMPLE_TIME_EXT);
        TYPE(XR_TYPE_VISIBILITY_MASK_KHR);
        TYPE(XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR);
        TYPE(XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX);
        TYPE(XR_TYPE_EVENT_DATA_MAIN_SESSION_VISIBILITY_CHANGED_EXTX);
        TYPE(XR_TYPE_COMPOSITION_LAYER_COLOR_SCALE_BIAS_KHR);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_SPACE_CREATE_INFO_MSFT);
        TYPE(XR_TYPE_COMPOSITION_LAYER_IMAGE_LAYOUT_FB);
        TYPE(XR_TYPE_COMPOSITION_LAYER_ALPHA_BLEND_FB);
        TYPE(XR_TYPE_VIEW_CONFIGURATION_DEPTH_RANGE_EXT);
        TYPE(XR_TYPE_GRAPHICS_BINDING_EGL_MNDX);
        TYPE(XR_TYPE_SPATIAL_GRAPH_NODE_SPACE_CREATE_INFO_MSFT);
        TYPE(XR_TYPE_SPATIAL_GRAPH_STATIC_NODE_BINDING_CREATE_INFO_MSFT);
        TYPE(XR_TYPE_SPATIAL_GRAPH_NODE_BINDING_PROPERTIES_GET_INFO_MSFT);
        TYPE(XR_TYPE_SPATIAL_GRAPH_NODE_BINDING_PROPERTIES_MSFT);
        TYPE(XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT);
        TYPE(XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT);
        TYPE(XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT);
        TYPE(XR_TYPE_HAND_JOINT_LOCATIONS_EXT);
        TYPE(XR_TYPE_HAND_JOINT_VELOCITIES_EXT);
        TYPE(XR_TYPE_SYSTEM_HAND_TRACKING_MESH_PROPERTIES_MSFT);
        TYPE(XR_TYPE_HAND_MESH_SPACE_CREATE_INFO_MSFT);
        TYPE(XR_TYPE_HAND_MESH_UPDATE_INFO_MSFT);
        TYPE(XR_TYPE_HAND_MESH_MSFT);
        TYPE(XR_TYPE_HAND_POSE_TYPE_INFO_MSFT);
        TYPE(XR_TYPE_SECONDARY_VIEW_CONFIGURATION_SESSION_BEGIN_INFO_MSFT);
        TYPE(XR_TYPE_SECONDARY_VIEW_CONFIGURATION_STATE_MSFT);
        TYPE(XR_TYPE_SECONDARY_VIEW_CONFIGURATION_FRAME_STATE_MSFT);
        TYPE(XR_TYPE_SECONDARY_VIEW_CONFIGURATION_FRAME_END_INFO_MSFT);
        TYPE(XR_TYPE_SECONDARY_VIEW_CONFIGURATION_LAYER_INFO_MSFT);
        TYPE(XR_TYPE_SECONDARY_VIEW_CONFIGURATION_SWAPCHAIN_CREATE_INFO_MSFT);
        TYPE(XR_TYPE_CONTROLLER_MODEL_KEY_STATE_MSFT);
        TYPE(XR_TYPE_CONTROLLER_MODEL_NODE_PROPERTIES_MSFT);
        TYPE(XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT);
        TYPE(XR_TYPE_CONTROLLER_MODEL_NODE_STATE_MSFT);
        TYPE(XR_TYPE_CONTROLLER_MODEL_STATE_MSFT);
        TYPE(XR_TYPE_VIEW_CONFIGURATION_VIEW_FOV_EPIC);
        TYPE(XR_TYPE_HOLOGRAPHIC_WINDOW_ATTACHMENT_MSFT);
        TYPE(XR_TYPE_COMPOSITION_LAYER_REPROJECTION_INFO_MSFT);
        TYPE(XR_TYPE_COMPOSITION_LAYER_REPROJECTION_PLANE_OVERRIDE_MSFT);
        TYPE(XR_TYPE_ANDROID_SURFACE_SWAPCHAIN_CREATE_INFO_FB);
        TYPE(XR_TYPE_COMPOSITION_LAYER_SECURE_CONTENT_FB);
        TYPE(XR_TYPE_BODY_TRACKER_CREATE_INFO_FB);
        TYPE(XR_TYPE_BODY_JOINTS_LOCATE_INFO_FB);
        TYPE(XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_FB);
        TYPE(XR_TYPE_BODY_JOINT_LOCATIONS_FB);
        TYPE(XR_TYPE_BODY_SKELETON_FB);
        TYPE(XR_TYPE_INTERACTION_PROFILE_DPAD_BINDING_EXT);
        TYPE(XR_TYPE_INTERACTION_PROFILE_ANALOG_THRESHOLD_VALVE);
        TYPE(XR_TYPE_HAND_JOINTS_MOTION_RANGE_INFO_EXT);
        TYPE(XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR);
        TYPE(XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR);
        TYPE(XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR);
        TYPE(XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR);
        TYPE(XR_TYPE_COMPOSITION_LAYER_EQUIRECT2_KHR);
        TYPE(XR_TYPE_SCENE_OBSERVER_CREATE_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_CREATE_INFO_MSFT);
        TYPE(XR_TYPE_NEW_SCENE_COMPUTE_INFO_MSFT);
        TYPE(XR_TYPE_VISUAL_MESH_COMPUTE_LOD_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_COMPONENTS_MSFT);
        TYPE(XR_TYPE_SCENE_COMPONENTS_GET_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_COMPONENT_LOCATIONS_MSFT);
        TYPE(XR_TYPE_SCENE_COMPONENTS_LOCATE_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_OBJECTS_MSFT);
        TYPE(XR_TYPE_SCENE_COMPONENT_PARENT_FILTER_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_OBJECT_TYPES_FILTER_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_PLANES_MSFT);
        TYPE(XR_TYPE_SCENE_PLANE_ALIGNMENT_FILTER_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_MESHES_MSFT);
        TYPE(XR_TYPE_SCENE_MESH_BUFFERS_GET_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_MESH_BUFFERS_MSFT);
        TYPE(XR_TYPE_SCENE_MESH_VERTEX_BUFFER_MSFT);
        TYPE(XR_TYPE_SCENE_MESH_INDICES_UINT32_MSFT);
        TYPE(XR_TYPE_SCENE_MESH_INDICES_UINT16_MSFT);
        TYPE(XR_TYPE_SERIALIZED_SCENE_FRAGMENT_DATA_GET_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_DESERIALIZE_INFO_MSFT);
        TYPE(XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB);
        TYPE(XR_TYPE_VIVE_TRACKER_PATHS_HTCX);
        TYPE(XR_TYPE_EVENT_DATA_VIVE_TRACKER_CONNECTED_HTCX);
        TYPE(XR_TYPE_SYSTEM_FACIAL_TRACKING_PROPERTIES_HTC);
        TYPE(XR_TYPE_FACIAL_TRACKER_CREATE_INFO_HTC);
        TYPE(XR_TYPE_FACIAL_EXPRESSIONS_HTC);
        TYPE(XR_TYPE_SYSTEM_COLOR_SPACE_PROPERTIES_FB);
        TYPE(XR_TYPE_HAND_TRACKING_MESH_FB);
        TYPE(XR_TYPE_HAND_TRACKING_SCALE_FB);
        TYPE(XR_TYPE_HAND_TRACKING_AIM_STATE_FB);
        TYPE(XR_TYPE_HAND_TRACKING_CAPSULES_STATE_FB);
        TYPE(XR_TYPE_SYSTEM_SPATIAL_ENTITY_PROPERTIES_FB);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_FB);
        TYPE(XR_TYPE_SPACE_COMPONENT_STATUS_SET_INFO_FB);
        TYPE(XR_TYPE_SPACE_COMPONENT_STATUS_FB);
        TYPE(XR_TYPE_EVENT_DATA_SPATIAL_ANCHOR_CREATE_COMPLETE_FB);
        TYPE(XR_TYPE_EVENT_DATA_SPACE_SET_STATUS_COMPLETE_FB);
        TYPE(XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB);
        TYPE(XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB);
        TYPE(XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB);
        TYPE(XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB);
        TYPE(XR_TYPE_KEYBOARD_SPACE_CREATE_INFO_FB);
        TYPE(XR_TYPE_KEYBOARD_TRACKING_QUERY_FB);
        TYPE(XR_TYPE_SYSTEM_KEYBOARD_TRACKING_PROPERTIES_FB);
        TYPE(XR_TYPE_TRIANGLE_MESH_CREATE_INFO_FB);
        TYPE(XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES_FB);
        TYPE(XR_TYPE_PASSTHROUGH_CREATE_INFO_FB);
        TYPE(XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB);
        TYPE(XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB);
        TYPE(XR_TYPE_GEOMETRY_INSTANCE_CREATE_INFO_FB);
        TYPE(XR_TYPE_GEOMETRY_INSTANCE_TRANSFORM_FB);
        TYPE(XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES2_FB);
        TYPE(XR_TYPE_PASSTHROUGH_STYLE_FB);
        TYPE(XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_RGBA_FB);
        TYPE(XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_MONO_FB);
        TYPE(XR_TYPE_PASSTHROUGH_BRIGHTNESS_CONTRAST_SATURATION_FB);
        TYPE(XR_TYPE_EVENT_DATA_PASSTHROUGH_STATE_CHANGED_FB);
        TYPE(XR_TYPE_RENDER_MODEL_PATH_INFO_FB);
        TYPE(XR_TYPE_RENDER_MODEL_PROPERTIES_FB);
        TYPE(XR_TYPE_RENDER_MODEL_BUFFER_FB);
        TYPE(XR_TYPE_RENDER_MODEL_LOAD_INFO_FB);
        TYPE(XR_TYPE_SYSTEM_RENDER_MODEL_PROPERTIES_FB);
        TYPE(XR_TYPE_RENDER_MODEL_CAPABILITIES_REQUEST_FB);
        TYPE(XR_TYPE_BINDING_MODIFICATIONS_KHR);
        TYPE(XR_TYPE_VIEW_LOCATE_FOVEATED_RENDERING_VARJO);
        TYPE(XR_TYPE_FOVEATED_VIEW_CONFIGURATION_VIEW_VARJO);
        TYPE(XR_TYPE_SYSTEM_FOVEATED_RENDERING_PROPERTIES_VARJO);
        TYPE(XR_TYPE_COMPOSITION_LAYER_DEPTH_TEST_VARJO);
        TYPE(XR_TYPE_SYSTEM_MARKER_TRACKING_PROPERTIES_VARJO);
        TYPE(XR_TYPE_EVENT_DATA_MARKER_TRACKING_UPDATE_VARJO);
        TYPE(XR_TYPE_MARKER_SPACE_CREATE_INFO_VARJO);
        TYPE(XR_TYPE_FRAME_END_INFO_ML);
        TYPE(XR_TYPE_GLOBAL_DIMMER_FRAME_END_INFO_ML);
        TYPE(XR_TYPE_COORDINATE_SPACE_CREATE_INFO_ML);
        TYPE(XR_TYPE_SYSTEM_MARKER_UNDERSTANDING_PROPERTIES_ML);
        TYPE(XR_TYPE_MARKER_DETECTOR_CREATE_INFO_ML);
        TYPE(XR_TYPE_MARKER_DETECTOR_ARUCO_INFO_ML);
        TYPE(XR_TYPE_MARKER_DETECTOR_SIZE_INFO_ML);
        TYPE(XR_TYPE_MARKER_DETECTOR_APRIL_TAG_INFO_ML);
        TYPE(XR_TYPE_MARKER_DETECTOR_CUSTOM_PROFILE_INFO_ML);
        TYPE(XR_TYPE_MARKER_DETECTOR_SNAPSHOT_INFO_ML);
        TYPE(XR_TYPE_MARKER_DETECTOR_STATE_ML);
        TYPE(XR_TYPE_MARKER_SPACE_CREATE_INFO_ML);
        TYPE(XR_TYPE_LOCALIZATION_MAP_ML);
        TYPE(XR_TYPE_EVENT_DATA_LOCALIZATION_CHANGED_ML);
        TYPE(XR_TYPE_MAP_LOCALIZATION_REQUEST_INFO_ML);
        TYPE(XR_TYPE_LOCALIZATION_MAP_IMPORT_INFO_ML);
        TYPE(XR_TYPE_LOCALIZATION_ENABLE_EVENTS_INFO_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_CREATE_INFO_FROM_POSE_ML);
        TYPE(XR_TYPE_CREATE_SPATIAL_ANCHORS_COMPLETION_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_STATE_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_CREATE_STORAGE_INFO_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_QUERY_INFO_RADIUS_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_QUERY_COMPLETION_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_CREATE_INFO_FROM_UUIDS_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_PUBLISH_INFO_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_PUBLISH_COMPLETION_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_DELETE_INFO_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_DELETE_COMPLETION_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_UPDATE_EXPIRATION_INFO_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_UPDATE_EXPIRATION_COMPLETION_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_PUBLISH_COMPLETION_DETAILS_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_DELETE_COMPLETION_DETAILS_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHORS_UPDATE_EXPIRATION_COMPLETION_DETAILS_ML);
        TYPE(XR_TYPE_EVENT_DATA_HEADSET_FIT_CHANGED_ML);
        TYPE(XR_TYPE_EVENT_DATA_EYE_CALIBRATION_CHANGED_ML);
        TYPE(XR_TYPE_USER_CALIBRATION_ENABLE_EVENTS_INFO_ML);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_PERSISTENCE_INFO_MSFT);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_FROM_PERSISTED_ANCHOR_CREATE_INFO_MSFT);
        TYPE(XR_TYPE_SCENE_MARKERS_MSFT);
        TYPE(XR_TYPE_SCENE_MARKER_TYPE_FILTER_MSFT);
        TYPE(XR_TYPE_SCENE_MARKER_QR_CODES_MSFT);
        TYPE(XR_TYPE_SPACE_QUERY_INFO_FB);
        TYPE(XR_TYPE_SPACE_QUERY_RESULTS_FB);
        TYPE(XR_TYPE_SPACE_STORAGE_LOCATION_FILTER_INFO_FB);
        TYPE(XR_TYPE_SPACE_UUID_FILTER_INFO_FB);
        TYPE(XR_TYPE_SPACE_COMPONENT_FILTER_INFO_FB);
        TYPE(XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB);
        TYPE(XR_TYPE_EVENT_DATA_SPACE_QUERY_COMPLETE_FB);
        TYPE(XR_TYPE_SPACE_SAVE_INFO_FB);
        TYPE(XR_TYPE_SPACE_ERASE_INFO_FB);
        TYPE(XR_TYPE_EVENT_DATA_SPACE_SAVE_COMPLETE_FB);
        TYPE(XR_TYPE_EVENT_DATA_SPACE_ERASE_COMPLETE_FB);
        TYPE(XR_TYPE_SWAPCHAIN_IMAGE_FOVEATION_VULKAN_FB);
        TYPE(XR_TYPE_SWAPCHAIN_STATE_ANDROID_SURFACE_DIMENSIONS_FB);
        TYPE(XR_TYPE_SWAPCHAIN_STATE_SAMPLER_OPENGL_ES_FB);
        TYPE(XR_TYPE_SWAPCHAIN_STATE_SAMPLER_VULKAN_FB);
        TYPE(XR_TYPE_SPACE_SHARE_INFO_FB);
        TYPE(XR_TYPE_EVENT_DATA_SPACE_SHARE_COMPLETE_FB);
        TYPE(XR_TYPE_COMPOSITION_LAYER_SPACE_WARP_INFO_FB);
        TYPE(XR_TYPE_SYSTEM_SPACE_WARP_PROPERTIES_FB);
        TYPE(XR_TYPE_HAPTIC_AMPLITUDE_ENVELOPE_VIBRATION_FB);
        TYPE(XR_TYPE_SEMANTIC_LABELS_FB);
        TYPE(XR_TYPE_ROOM_LAYOUT_FB);
        TYPE(XR_TYPE_BOUNDARY_2D_FB);
        TYPE(XR_TYPE_SEMANTIC_LABELS_SUPPORT_INFO_FB);
        TYPE(XR_TYPE_DIGITAL_LENS_CONTROL_ALMALENCE);
        TYPE(XR_TYPE_EVENT_DATA_SCENE_CAPTURE_COMPLETE_FB);
        TYPE(XR_TYPE_SCENE_CAPTURE_REQUEST_INFO_FB);
        TYPE(XR_TYPE_SPACE_CONTAINER_FB);
        TYPE(XR_TYPE_FOVEATION_EYE_TRACKED_PROFILE_CREATE_INFO_META);
        TYPE(XR_TYPE_FOVEATION_EYE_TRACKED_STATE_META);
        TYPE(XR_TYPE_SYSTEM_FOVEATION_EYE_TRACKED_PROPERTIES_META);
        TYPE(XR_TYPE_SYSTEM_FACE_TRACKING_PROPERTIES_FB);
        TYPE(XR_TYPE_FACE_TRACKER_CREATE_INFO_FB);
        TYPE(XR_TYPE_FACE_EXPRESSION_INFO_FB);
        TYPE(XR_TYPE_FACE_EXPRESSION_WEIGHTS_FB);
        TYPE(XR_TYPE_EYE_TRACKER_CREATE_INFO_FB);
        TYPE(XR_TYPE_EYE_GAZES_INFO_FB);
        TYPE(XR_TYPE_EYE_GAZES_FB);
        TYPE(XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_FB);
        TYPE(XR_TYPE_PASSTHROUGH_KEYBOARD_HANDS_INTENSITY_FB);
        TYPE(XR_TYPE_COMPOSITION_LAYER_SETTINGS_FB);
        TYPE(XR_TYPE_HAPTIC_PCM_VIBRATION_FB);
        TYPE(XR_TYPE_DEVICE_PCM_SAMPLE_RATE_STATE_FB);
        TYPE(XR_TYPE_FRAME_SYNTHESIS_INFO_EXT);
        TYPE(XR_TYPE_FRAME_SYNTHESIS_CONFIG_VIEW_EXT);
        TYPE(XR_TYPE_COMPOSITION_LAYER_DEPTH_TEST_FB);
        TYPE(XR_TYPE_LOCAL_DIMMING_FRAME_END_INFO_META);
        TYPE(XR_TYPE_PASSTHROUGH_PREFERENCES_META);
        TYPE(XR_TYPE_SYSTEM_VIRTUAL_KEYBOARD_PROPERTIES_META);
        TYPE(XR_TYPE_VIRTUAL_KEYBOARD_CREATE_INFO_META);
        TYPE(XR_TYPE_VIRTUAL_KEYBOARD_SPACE_CREATE_INFO_META);
        TYPE(XR_TYPE_VIRTUAL_KEYBOARD_LOCATION_INFO_META);
        TYPE(XR_TYPE_VIRTUAL_KEYBOARD_MODEL_VISIBILITY_SET_INFO_META);
        TYPE(XR_TYPE_VIRTUAL_KEYBOARD_ANIMATION_STATE_META);
        TYPE(XR_TYPE_VIRTUAL_KEYBOARD_MODEL_ANIMATION_STATES_META);
        TYPE(XR_TYPE_VIRTUAL_KEYBOARD_TEXTURE_DATA_META);
        TYPE(XR_TYPE_VIRTUAL_KEYBOARD_INPUT_INFO_META);
        TYPE(XR_TYPE_VIRTUAL_KEYBOARD_TEXT_CONTEXT_CHANGE_INFO_META);
        TYPE(XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_COMMIT_TEXT_META);
        TYPE(XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_BACKSPACE_META);
        TYPE(XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_ENTER_META);
        TYPE(XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_SHOWN_META);
        TYPE(XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_HIDDEN_META);
        TYPE(XR_TYPE_EXTERNAL_CAMERA_OCULUS);
        TYPE(XR_TYPE_VULKAN_SWAPCHAIN_CREATE_INFO_META);
        TYPE(XR_TYPE_PERFORMANCE_METRICS_STATE_META);
        TYPE(XR_TYPE_PERFORMANCE_METRICS_COUNTER_META);
        TYPE(XR_TYPE_SPACE_LIST_SAVE_INFO_FB);
        TYPE(XR_TYPE_EVENT_DATA_SPACE_LIST_SAVE_COMPLETE_FB);
        TYPE(XR_TYPE_SPACE_USER_CREATE_INFO_FB);
        TYPE(XR_TYPE_SYSTEM_HEADSET_ID_PROPERTIES_META);
        TYPE(XR_TYPE_RECOMMENDED_LAYER_RESOLUTION_META);
        TYPE(XR_TYPE_RECOMMENDED_LAYER_RESOLUTION_GET_INFO_META);
        TYPE(XR_TYPE_SYSTEM_SPACE_PERSISTENCE_PROPERTIES_META);
        TYPE(XR_TYPE_SPACES_SAVE_INFO_META);
        TYPE(XR_TYPE_EVENT_DATA_SPACES_SAVE_RESULT_META);
        TYPE(XR_TYPE_SPACES_ERASE_INFO_META);
        TYPE(XR_TYPE_EVENT_DATA_SPACES_ERASE_RESULT_META);
        TYPE(XR_TYPE_SYSTEM_PASSTHROUGH_COLOR_LUT_PROPERTIES_META);
        TYPE(XR_TYPE_PASSTHROUGH_COLOR_LUT_CREATE_INFO_META);
        TYPE(XR_TYPE_PASSTHROUGH_COLOR_LUT_UPDATE_INFO_META);
        TYPE(XR_TYPE_PASSTHROUGH_COLOR_MAP_LUT_META);
        TYPE(XR_TYPE_PASSTHROUGH_COLOR_MAP_INTERPOLATED_LUT_META);
        TYPE(XR_TYPE_SPACE_TRIANGLE_MESH_GET_INFO_META);
        TYPE(XR_TYPE_SPACE_TRIANGLE_MESH_META);
        TYPE(XR_TYPE_SYSTEM_PROPERTIES_BODY_TRACKING_FULL_BODY_META);
        TYPE(XR_TYPE_EVENT_DATA_PASSTHROUGH_LAYER_RESUMED_META);
        TYPE(XR_TYPE_BODY_TRACKING_CALIBRATION_INFO_META);
        TYPE(XR_TYPE_BODY_TRACKING_CALIBRATION_STATUS_META);
        TYPE(XR_TYPE_SYSTEM_PROPERTIES_BODY_TRACKING_CALIBRATION_META);
        TYPE(XR_TYPE_SYSTEM_FACE_TRACKING_PROPERTIES2_FB);
        TYPE(XR_TYPE_FACE_TRACKER_CREATE_INFO2_FB);
        TYPE(XR_TYPE_FACE_EXPRESSION_INFO2_FB);
        TYPE(XR_TYPE_FACE_EXPRESSION_WEIGHTS2_FB);
        TYPE(XR_TYPE_SYSTEM_SPATIAL_ENTITY_SHARING_PROPERTIES_META);
        TYPE(XR_TYPE_SHARE_SPACES_INFO_META);
        TYPE(XR_TYPE_EVENT_DATA_SHARE_SPACES_COMPLETE_META);
        TYPE(XR_TYPE_ENVIRONMENT_DEPTH_PROVIDER_CREATE_INFO_META);
        TYPE(XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_CREATE_INFO_META);
        TYPE(XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_STATE_META);
        TYPE(XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_ACQUIRE_INFO_META);
        TYPE(XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_VIEW_META);
        TYPE(XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_META);
        TYPE(XR_TYPE_ENVIRONMENT_DEPTH_HAND_REMOVAL_SET_INFO_META);
        TYPE(XR_TYPE_SYSTEM_ENVIRONMENT_DEPTH_PROPERTIES_META);
        TYPE(XR_TYPE_RENDER_MODEL_CREATE_INFO_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_PROPERTIES_GET_INFO_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_PROPERTIES_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_SPACE_CREATE_INFO_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_STATE_GET_INFO_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_STATE_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_ASSET_CREATE_INFO_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_ASSET_DATA_GET_INFO_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_ASSET_DATA_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_ASSET_PROPERTIES_GET_INFO_EXT);
        TYPE(XR_TYPE_RENDER_MODEL_ASSET_PROPERTIES_EXT);
        TYPE(XR_TYPE_INTERACTION_RENDER_MODEL_IDS_ENUMERATE_INFO_EXT);
        TYPE(XR_TYPE_INTERACTION_RENDER_MODEL_SUBACTION_PATH_INFO_EXT);
        TYPE(XR_TYPE_EVENT_DATA_INTERACTION_RENDER_MODELS_CHANGED_EXT);
        TYPE(XR_TYPE_INTERACTION_RENDER_MODEL_TOP_LEVEL_USER_PATH_GET_INFO_EXT);
        TYPE(XR_TYPE_PASSTHROUGH_CREATE_INFO_HTC);
        TYPE(XR_TYPE_PASSTHROUGH_COLOR_HTC);
        TYPE(XR_TYPE_PASSTHROUGH_MESH_TRANSFORM_INFO_HTC);
        TYPE(XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_HTC);
        TYPE(XR_TYPE_FOVEATION_APPLY_INFO_HTC);
        TYPE(XR_TYPE_FOVEATION_DYNAMIC_MODE_INFO_HTC);
        TYPE(XR_TYPE_FOVEATION_CUSTOM_MODE_INFO_HTC);
        TYPE(XR_TYPE_SYSTEM_ANCHOR_PROPERTIES_HTC);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_HTC);
        TYPE(XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_HTC);
        TYPE(XR_TYPE_BODY_TRACKER_CREATE_INFO_HTC);
        TYPE(XR_TYPE_BODY_JOINTS_LOCATE_INFO_HTC);
        TYPE(XR_TYPE_BODY_JOINT_LOCATIONS_HTC);
        TYPE(XR_TYPE_BODY_SKELETON_HTC);
        TYPE(XR_TYPE_ACTIVE_ACTION_SET_PRIORITIES_EXT);
        TYPE(XR_TYPE_SYSTEM_FORCE_FEEDBACK_CURL_PROPERTIES_MNDX);
        TYPE(XR_TYPE_FORCE_FEEDBACK_CURL_APPLY_LOCATIONS_MNDX);
        TYPE(XR_TYPE_BODY_TRACKER_CREATE_INFO_BD);
        TYPE(XR_TYPE_BODY_JOINTS_LOCATE_INFO_BD);
        TYPE(XR_TYPE_BODY_JOINT_LOCATIONS_BD);
        TYPE(XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_BD);
        TYPE(XR_TYPE_SYSTEM_SPATIAL_SENSING_PROPERTIES_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_COMPONENT_GET_INFO_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_LOCATION_GET_INFO_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_COMPONENT_DATA_LOCATION_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_COMPONENT_DATA_SEMANTIC_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_COMPONENT_DATA_BOUNDING_BOX_2D_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_COMPONENT_DATA_POLYGON_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_COMPONENT_DATA_BOUNDING_BOX_3D_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_COMPONENT_DATA_TRIANGLE_MESH_BD);
        TYPE(XR_TYPE_SENSE_DATA_PROVIDER_CREATE_INFO_BD);
        TYPE(XR_TYPE_SENSE_DATA_PROVIDER_START_INFO_BD);
        TYPE(XR_TYPE_EVENT_DATA_SENSE_DATA_PROVIDER_STATE_CHANGED_BD);
        TYPE(XR_TYPE_EVENT_DATA_SENSE_DATA_UPDATED_BD);
        TYPE(XR_TYPE_SENSE_DATA_QUERY_INFO_BD);
        TYPE(XR_TYPE_SENSE_DATA_QUERY_COMPLETION_BD);
        TYPE(XR_TYPE_SENSE_DATA_FILTER_UUID_BD);
        TYPE(XR_TYPE_SENSE_DATA_FILTER_SEMANTIC_BD);
        TYPE(XR_TYPE_QUERIED_SENSE_DATA_GET_INFO_BD);
        TYPE(XR_TYPE_QUERIED_SENSE_DATA_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_STATE_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_ANCHOR_CREATE_INFO_BD);
        TYPE(XR_TYPE_ANCHOR_SPACE_CREATE_INFO_BD);
        TYPE(XR_TYPE_SYSTEM_SPATIAL_ANCHOR_PROPERTIES_BD);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_BD);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_CREATE_COMPLETION_BD);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_PERSIST_INFO_BD);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_UNPERSIST_INFO_BD);
        TYPE(XR_TYPE_SYSTEM_SPATIAL_ANCHOR_SHARING_PROPERTIES_BD);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_SHARE_INFO_BD);
        TYPE(XR_TYPE_SHARED_SPATIAL_ANCHOR_DOWNLOAD_INFO_BD);
        TYPE(XR_TYPE_SYSTEM_SPATIAL_SCENE_PROPERTIES_BD);
        TYPE(XR_TYPE_SCENE_CAPTURE_INFO_BD);
        TYPE(XR_TYPE_SYSTEM_SPATIAL_MESH_PROPERTIES_BD);
        TYPE(XR_TYPE_SENSE_DATA_PROVIDER_CREATE_INFO_SPATIAL_MESH_BD);
        TYPE(XR_TYPE_FUTURE_POLL_RESULT_PROGRESS_BD);
        TYPE(XR_TYPE_SYSTEM_SPATIAL_PLANE_PROPERTIES_BD);
        TYPE(XR_TYPE_SPATIAL_ENTITY_COMPONENT_DATA_PLANE_ORIENTATION_BD);
        TYPE(XR_TYPE_SENSE_DATA_FILTER_PLANE_ORIENTATION_BD);
        TYPE(XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT);
        TYPE(XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT);
        TYPE(XR_TYPE_PLANE_DETECTOR_CREATE_INFO_EXT);
        TYPE(XR_TYPE_PLANE_DETECTOR_BEGIN_INFO_EXT);
        TYPE(XR_TYPE_PLANE_DETECTOR_GET_INFO_EXT);
        TYPE(XR_TYPE_PLANE_DETECTOR_LOCATIONS_EXT);
        TYPE(XR_TYPE_PLANE_DETECTOR_LOCATION_EXT);
        TYPE(XR_TYPE_PLANE_DETECTOR_POLYGON_BUFFER_EXT);
        TYPE(XR_TYPE_SYSTEM_PLANE_DETECTION_PROPERTIES_EXT);
        TYPE(XR_TYPE_TRACKABLE_GET_INFO_ANDROID);
        TYPE(XR_TYPE_ANCHOR_SPACE_CREATE_INFO_ANDROID);
        TYPE(XR_TYPE_TRACKABLE_PLANE_ANDROID);
        TYPE(XR_TYPE_TRACKABLE_TRACKER_CREATE_INFO_ANDROID);
        TYPE(XR_TYPE_SYSTEM_TRACKABLES_PROPERTIES_ANDROID);
        TYPE(XR_TYPE_PERSISTED_ANCHOR_SPACE_CREATE_INFO_ANDROID);
        TYPE(XR_TYPE_PERSISTED_ANCHOR_SPACE_INFO_ANDROID);
        TYPE(XR_TYPE_DEVICE_ANCHOR_PERSISTENCE_CREATE_INFO_ANDROID);
        TYPE(XR_TYPE_SYSTEM_DEVICE_ANCHOR_PERSISTENCE_PROPERTIES_ANDROID);
        TYPE(XR_TYPE_PASSTHROUGH_CAMERA_STATE_GET_INFO_ANDROID);
        TYPE(XR_TYPE_SYSTEM_PASSTHROUGH_CAMERA_STATE_PROPERTIES_ANDROID);
        TYPE(XR_TYPE_RAYCAST_INFO_ANDROID);
        TYPE(XR_TYPE_RAYCAST_HIT_RESULTS_ANDROID);
        TYPE(XR_TYPE_TRACKABLE_OBJECT_ANDROID);
        TYPE(XR_TYPE_TRACKABLE_OBJECT_CONFIGURATION_ANDROID);
        TYPE(XR_TYPE_FUTURE_CANCEL_INFO_EXT);
        TYPE(XR_TYPE_FUTURE_POLL_INFO_EXT);
        TYPE(XR_TYPE_FUTURE_COMPLETION_EXT);
        TYPE(XR_TYPE_FUTURE_POLL_RESULT_EXT);
        TYPE(XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT);
        TYPE(XR_TYPE_SYSTEM_USER_PRESENCE_PROPERTIES_EXT);
        TYPE(XR_TYPE_SYSTEM_NOTIFICATIONS_SET_INFO_ML);
        TYPE(XR_TYPE_WORLD_MESH_DETECTOR_CREATE_INFO_ML);
        TYPE(XR_TYPE_WORLD_MESH_STATE_REQUEST_INFO_ML);
        TYPE(XR_TYPE_WORLD_MESH_BLOCK_STATE_ML);
        TYPE(XR_TYPE_WORLD_MESH_STATE_REQUEST_COMPLETION_ML);
        TYPE(XR_TYPE_WORLD_MESH_BUFFER_RECOMMENDED_SIZE_INFO_ML);
        TYPE(XR_TYPE_WORLD_MESH_BUFFER_SIZE_ML);
        TYPE(XR_TYPE_WORLD_MESH_BUFFER_ML);
        TYPE(XR_TYPE_WORLD_MESH_BLOCK_REQUEST_ML);
        TYPE(XR_TYPE_WORLD_MESH_GET_INFO_ML);
        TYPE(XR_TYPE_WORLD_MESH_BLOCK_ML);
        TYPE(XR_TYPE_WORLD_MESH_REQUEST_COMPLETION_ML);
        TYPE(XR_TYPE_WORLD_MESH_REQUEST_COMPLETION_INFO_ML);
        TYPE(XR_TYPE_SYSTEM_FACIAL_EXPRESSION_PROPERTIES_ML);
        TYPE(XR_TYPE_FACIAL_EXPRESSION_CLIENT_CREATE_INFO_ML);
        TYPE(XR_TYPE_FACIAL_EXPRESSION_BLEND_SHAPE_GET_INFO_ML);
        TYPE(XR_TYPE_FACIAL_EXPRESSION_BLEND_SHAPE_PROPERTIES_ML);
        TYPE(XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_AND_CONTROLLERS_PROPERTIES_META);
        TYPE(XR_TYPE_SIMULTANEOUS_HANDS_AND_CONTROLLERS_TRACKING_RESUME_INFO_META);
        TYPE(XR_TYPE_SIMULTANEOUS_HANDS_AND_CONTROLLERS_TRACKING_PAUSE_INFO_META);
        TYPE(XR_TYPE_COLOCATION_DISCOVERY_START_INFO_META);
        TYPE(XR_TYPE_COLOCATION_DISCOVERY_STOP_INFO_META);
        TYPE(XR_TYPE_COLOCATION_ADVERTISEMENT_START_INFO_META);
        TYPE(XR_TYPE_COLOCATION_ADVERTISEMENT_STOP_INFO_META);
        TYPE(XR_TYPE_EVENT_DATA_START_COLOCATION_ADVERTISEMENT_COMPLETE_META);
        TYPE(XR_TYPE_EVENT_DATA_STOP_COLOCATION_ADVERTISEMENT_COMPLETE_META);
        TYPE(XR_TYPE_EVENT_DATA_COLOCATION_ADVERTISEMENT_COMPLETE_META);
        TYPE(XR_TYPE_EVENT_DATA_START_COLOCATION_DISCOVERY_COMPLETE_META);
        TYPE(XR_TYPE_EVENT_DATA_COLOCATION_DISCOVERY_RESULT_META);
        TYPE(XR_TYPE_EVENT_DATA_COLOCATION_DISCOVERY_COMPLETE_META);
        TYPE(XR_TYPE_EVENT_DATA_STOP_COLOCATION_DISCOVERY_COMPLETE_META);
        TYPE(XR_TYPE_SYSTEM_COLOCATION_DISCOVERY_PROPERTIES_META);
        TYPE(XR_TYPE_SHARE_SPACES_RECIPIENT_GROUPS_META);
        TYPE(XR_TYPE_SPACE_GROUP_UUID_FILTER_INFO_META);
        TYPE(XR_TYPE_SYSTEM_SPATIAL_ENTITY_GROUP_SHARING_PROPERTIES_META);
        TYPE(XR_TYPE_ANCHOR_SHARING_INFO_ANDROID);
        TYPE(XR_TYPE_ANCHOR_SHARING_TOKEN_ANDROID);
        TYPE(XR_TYPE_SYSTEM_ANCHOR_SHARING_EXPORT_PROPERTIES_ANDROID);
        TYPE(XR_TYPE_SYSTEM_MARKER_TRACKING_PROPERTIES_ANDROID);
        TYPE(XR_TYPE_TRACKABLE_MARKER_CONFIGURATION_ANDROID);
        TYPE(XR_TYPE_TRACKABLE_MARKER_ANDROID);
        TYPE(XR_TYPE_SPATIAL_CAPABILITY_COMPONENT_TYPES_EXT);
        TYPE(XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT);
        TYPE(XR_TYPE_CREATE_SPATIAL_CONTEXT_COMPLETION_EXT);
        TYPE(XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT);
        TYPE(XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_INFO_EXT);
        TYPE(XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT);
        TYPE(XR_TYPE_SPATIAL_BUFFER_GET_INFO_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_BOUNDED_2D_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_BOUNDED_3D_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_PARENT_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_MESH_3D_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_ENTITY_FROM_ID_CREATE_INFO_EXT);
        TYPE(XR_TYPE_SPATIAL_UPDATE_SNAPSHOT_CREATE_INFO_EXT);
        TYPE(XR_TYPE_EVENT_DATA_SPATIAL_DISCOVERY_RECOMMENDED_EXT);
        TYPE(XR_TYPE_SPATIAL_FILTER_TRACKING_STATE_EXT);
        TYPE(XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_PLANE_TRACKING_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_PLANE_ALIGNMENT_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_MESH_2D_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_POLYGON_2D_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_PLANE_SEMANTIC_LABEL_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_QR_CODE_EXT);
        TYPE(XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_MICRO_QR_CODE_EXT);
        TYPE(XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ARUCO_MARKER_EXT);
        TYPE(XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_APRIL_TAG_EXT);
        TYPE(XR_TYPE_SPATIAL_MARKER_SIZE_EXT);
        TYPE(XR_TYPE_SPATIAL_MARKER_STATIC_OPTIMIZATION_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_MARKER_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ANCHOR_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_ANCHOR_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_EXT);
        TYPE(XR_TYPE_SPATIAL_PERSISTENCE_CONTEXT_CREATE_INFO_EXT);
        TYPE(XR_TYPE_CREATE_SPATIAL_PERSISTENCE_CONTEXT_COMPLETION_EXT);
        TYPE(XR_TYPE_SPATIAL_CONTEXT_PERSISTENCE_CONFIG_EXT);
        TYPE(XR_TYPE_SPATIAL_DISCOVERY_PERSISTENCE_UUID_FILTER_EXT);
        TYPE(XR_TYPE_SPATIAL_COMPONENT_PERSISTENCE_LIST_EXT);
        TYPE(XR_TYPE_SPATIAL_ENTITY_PERSIST_INFO_EXT);
        TYPE(XR_TYPE_PERSIST_SPATIAL_ENTITY_COMPLETION_EXT);
        TYPE(XR_TYPE_SPATIAL_ENTITY_UNPERSIST_INFO_EXT);
        TYPE(XR_TYPE_UNPERSIST_SPATIAL_ENTITY_COMPLETION_EXT);
        TYPE(XR_TYPE_LOADER_INIT_INFO_PROPERTIES_EXT);

        default:
        {
            snprintf(buffer, XR_MAX_STRUCTURE_NAME_SIZE, "XR_UNKNOWN_STRUCTURE_TYPE_%u", value);
        } break;
    }

#undef TYPE

    buffer[XR_MAX_STRUCTURE_NAME_SIZE - 1] = 0;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetSystem_impl(XrInstance instance, const XrSystemGetInfo *system_info, XrSystemId *system_id)
{
    TRACE_ENTER();

    if (!system_info || !system_id)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (system_info->formFactor != XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_FORM_FACTOR_UNSUPPORTED);
    }

    *system_id = SYSTEM_HANDLE;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetSystemProperties_impl(XrInstance instance, XrSystemId system_id, XrSystemProperties* properties)
{
    TRACE_ENTER();

    if (!properties)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (system_id != SYSTEM_HANDLE)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_SYSTEM_INVALID);
    }

    properties->systemId                                   = SYSTEM_HANDLE;
    properties->vendorId                                   = 0x1F00D;
    strncpy(properties->systemName, "openxr_simulator/system", XR_MAX_SYSTEM_NAME_SIZE);
    properties->systemName[XR_MAX_SYSTEM_NAME_SIZE - 1]    = 0;
    properties->graphicsProperties.maxSwapchainImageHeight = 4096;
    properties->graphicsProperties.maxSwapchainImageWidth  = 4096;
    properties->graphicsProperties.maxLayerCount           = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
    properties->trackingProperties.orientationTracking     = XR_TRUE;
    properties->trackingProperties.positionTracking        = XR_TRUE;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateEnvironmentBlendModes_impl(XrInstance instance, XrSystemId system_id, XrViewConfigurationType view_configuration_type, uint32_t blend_mode_capacity,
                                      uint32_t *blend_mode_count, XrEnvironmentBlendMode *blend_modes)
{
    TRACE_ENTER();

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (system_id != SYSTEM_HANDLE)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_SYSTEM_INVALID);
    }

    if (!blend_mode_count)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    *blend_mode_count = 1;

    if (blend_mode_capacity > 0)
    {
        if (blend_mode_capacity < 1)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_SIZE_INSUFFICIENT);
        }

        if (!blend_modes)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
        }

        blend_modes[0] = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrCreateSession_impl(XrInstance instance, const XrSessionCreateInfo *create_info, XrSession *session)
{
    TRACE_ENTER();

    if (!create_info || !session)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((create_info->type != XR_TYPE_SESSION_CREATE_INFO) || (create_info->createFlags != 0))
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (create_info->systemId != SYSTEM_HANDLE)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_SYSTEM_INVALID);
    }

    if (state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_LIMIT_REACHED);
    }

    if (state.instance.graphics_api == GraphicsApiUnknown)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING);
    }

    const XrBaseInStructure *graphics_binding = (struct XrBaseInStructure *) create_info->next;

    if (!graphics_binding)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_GRAPHICS_DEVICE_INVALID);
    }

    int32_t px4 = 4;
    int32_t px6 = 6;
    int32_t px8 = 8;

    int32_t window_width  = (2 * EYE_WIDTH) + (2 * px8);
    int32_t window_height = EYE_HEIGHT + px8 + px6 + px4 + terminus_16_bold_font.size;

    switch (state.instance.graphics_api)
    {

#if GRAPHICS_API_D3D11
        case GraphicsApiD3D11:
        {

#  if PLATFORM_WIN32
            if (graphics_binding->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR)
            {
                XrGraphicsBindingD3D11KHR *d3d11 = (XrGraphicsBindingD3D11KHR *) graphics_binding;

                state.session.active = true;
                state.session.platform = PlatformWin32;
                state.session.width = window_width;
                state.session.height = window_height;

                initialize_platform_win32_d3d11(&state.session.win32, d3d11->device, state.session.width, state.session.height);
                state.session.font_texture = create_d3d11_texture(&state.session.win32.d3d11, terminus_16_bold_font.texture_width, terminus_16_bold_font.texture_height, terminus_16_bold_font.texture_data);
            }
#  endif

        } break;
#endif

#if GRAPHICS_API_OPENGL
        case GraphicsApiOpenGl:
        {

#  if PLATFORM_XLIB
            if (graphics_binding->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR)
            {
                XrGraphicsBindingOpenGLXlibKHR *opengl_xlib = (XrGraphicsBindingOpenGLXlibKHR *) graphics_binding;

                state.session.active = true;
                state.session.platform = PlatformXlib;
                state.session.width = window_width;
                state.session.height = window_height;

                initialize_platform_xlib_opengl(&state.session.xlib, opengl_xlib->xDisplay, opengl_xlib->glxDrawable, opengl_xlib->glxContext, state.session.width, state.session.height);
                state.session.font_texture = create_opengl_texture(&state.session.xlib.opengl, terminus_16_bold_font.texture_width, terminus_16_bold_font.texture_height, terminus_16_bold_font.texture_data);
            }
#  endif

#  if PLATFORM_WIN32
            if (graphics_binding->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR)
            {
                XrGraphicsBindingOpenGLWin32KHR *opengl_win32 = (XrGraphicsBindingOpenGLWin32KHR *) graphics_binding;

                state.session.active = true;
                state.session.platform = PlatformWin32;
                state.session.width = window_width;
                state.session.height = window_height;

                initialize_platform_win32_opengl(&state.session.win32, opengl_win32->hDC, opengl_win32->hGLRC, state.session.width, state.session.height);
                state.session.font_texture = create_opengl_texture(&state.session.win32.opengl, terminus_16_bold_font.texture_width, terminus_16_bold_font.texture_height, terminus_16_bold_font.texture_data);
            }
#  endif

#  if PLATFORM_WAYLAND
            if (graphics_binding->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR)
            {
                XrGraphicsBindingOpenGLWaylandKHR *wayland_opengl = (XrGraphicsBindingOpenGLWaylandKHR *) graphics_binding;

                state.session.active = true;
                state.session.platform = PlatformWayland;
                state.session.width = window_width;
                state.session.height = window_height;

                initialize_platform_wayland_opengl(&state.session.wayland, state.session.width, state.session.height);
                // TODO: state.session.font_texture = create_opengl_texture(&state.session.win32.opengl, terminus_16_bold_font.texture_width, terminus_16_bold_font.texture_height, terminus_16_bold_font.texture_data);
            }
#  endif

        } break;
#endif

        default:
        {
            TRACE_LEAVE_RESULT(XR_ERROR_GRAPHICS_DEVICE_INVALID);
        } break;
    }

    state.session.next_space_handle = SPACE_HANDLE_OFFSET;
    state.session.head_orbit = 0.0f;
    state.session.head_pitch = 0.0f;
    state.session.head_position = (XrVector3f) { 0.0f, 1.75f, 0.0f };

    change_state(XR_SESSION_STATE_IDLE);
    change_state(XR_SESSION_STATE_READY);

    *session = SESSION_HANDLE;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrDestroySession_impl(XrSession session)
{
    TRACE_ENTER();

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    switch (state.instance.graphics_api)
    {

#if GRAPHICS_API_D3D11
        case GraphicsApiD3D11:
        {
#  if PLATFORM_WIN32
            if (state.session.platform != PlatformWin32)
#  endif
            {
                TRACE_LEAVE_RESULT(XR_ERROR_RUNTIME_FAILURE);
            }

#  if PLATFORM_WIN32
            deinitialize_platform_win32_d3d11(&state.session.win32);
#  endif
        } break;
#endif

#if GRAPHICS_API_OPENGL
        case GraphicsApiOpenGl:
        {
            switch (state.session.platform)
            {

#  if PLATFORM_XLIB
                case PlatformXlib:
                {
                    destroy_opengl_texture(&state.session.xlib.opengl, state.session.font_texture);
                    deinitialize_platform_xlib_opengl(&state.session.xlib);
                } break;
#  endif

#  if PLATFORM_WIN32
                case PlatformWin32:
                {
                    destroy_opengl_texture(&state.session.win32.opengl, state.session.font_texture);
                    msg("TODO: deinitialize_platform_win32_opengl\n");
                    // deinitialize_platform_win32_opengl(&state.session.win32);
                } break;
#  endif

#  if PLATFORM_WAYLAND
                case PlatformWayland:
                {
                    // TODO: destroy_opengl_texture(&state.session.wayland.opengl, state.session.font_texture);
                    msg("TODO: deinitialize_platform_wayland_opengl\n");
                    // deinitialize_platform_wayland_opengl(&state.session.wayland);
                } break;
#  endif

            }
        } break;
#endif

        default:
        {
            // TODO: return error
        } break;
    }

    state.session.active = false;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateReferenceSpaces_impl(XrSession session, uint32_t space_capacity, uint32_t *space_count, XrReferenceSpaceType *spaces)
{
    TRACE_ENTER();

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (!space_count)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    *space_count = 3;

    if (space_capacity > 0)
    {
        if (space_capacity < 3)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_SIZE_INSUFFICIENT);
        }

        if (!spaces)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
        }

        spaces[0] = XR_REFERENCE_SPACE_TYPE_VIEW;
        spaces[1] = XR_REFERENCE_SPACE_TYPE_LOCAL;
        spaces[2] = XR_REFERENCE_SPACE_TYPE_STAGE;
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrCreateReferenceSpace_impl(XrSession session, const XrReferenceSpaceCreateInfo *create_info, XrSpace *space)
{
    TRACE_ENTER();

    if (!create_info || !space)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if (create_info->type != XR_TYPE_REFERENCE_SPACE_CREATE_INFO)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    *space = (XrSpace) state.session.next_space_handle;
    state.session.next_space_handle += 1;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetReferenceSpaceBoundsRect_impl(XrSession session, XrReferenceSpaceType reference_space_type, XrExtent2Df *bounds)
{
    TRACE_ENTER();

    if (!bounds)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    bounds->width  = 2.0f;
    bounds->height = 2.0f;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrCreateActionSpace_impl(XrSession session, const XrActionSpaceCreateInfo *create_info, XrSpace *space)
{
    TRACE_ENTER();

    if (!create_info || !space)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if (create_info->type != XR_TYPE_ACTION_SPACE_CREATE_INFO)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    *space = (XrSpace) state.session.next_space_handle;
    state.session.next_space_handle += 1;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrLocateSpace_impl(XrSpace space, XrSpace base_space, XrTime time, XrSpaceLocation *location)
{
    TRACE_ENTER();

    if (!location)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    XrQuaternionf orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
    XrVector3f position = { 0.0f, 0.0f, 0.0f };

    location->locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    location->pose.orientation = orientation;
    location->pose.position = position;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrDestroySpace_impl(XrSpace space)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateViewConfigurations_impl(XrInstance instance, XrSystemId system_id, uint32_t configuration_capacity, uint32_t *configuration_count, XrViewConfigurationType *configurations)
{
    TRACE_ENTER();

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (system_id != SYSTEM_HANDLE)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_SYSTEM_INVALID);
    }

    if (!configuration_count)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    *configuration_count = 1;

    if (configuration_capacity > 0)
    {
        if (configuration_capacity < 1)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_SIZE_INSUFFICIENT);
        }

        if (!configurations)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
        }

        configurations[0] = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetViewConfigurationProperties_impl(XrInstance instance, XrSystemId system_id, XrViewConfigurationType view_configuration_type, XrViewConfigurationProperties *configuration_properties)
{
    TRACE_ENTER();

    if (!configuration_properties)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (system_id != SYSTEM_HANDLE)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_SYSTEM_INVALID);
    }

    if (view_configuration_type != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED);
    }

    configuration_properties->viewConfigurationType = view_configuration_type;
    configuration_properties->fovMutable = XR_FALSE;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateViewConfigurationViews_impl(XrInstance instance, XrSystemId system_id, XrViewConfigurationType view_configuration_type,
                                       uint32_t view_capacity, uint32_t *view_count, XrViewConfigurationView *views)
{
    TRACE_ENTER();

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (system_id != SYSTEM_HANDLE)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_SYSTEM_INVALID);
    }

    if (view_configuration_type != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED);
    }

    if (!view_count)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    *view_count = 2;

    if (view_capacity > 0)
    {
        if (view_capacity < 2)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_SIZE_INSUFFICIENT);
        }

        if (!views)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
        }

        views[0].recommendedImageRectWidth       = EYE_WIDTH;
        views[0].maxImageRectWidth               = 4096;
        views[0].recommendedImageRectHeight      = EYE_HEIGHT;
        views[0].maxImageRectHeight              = 4096;
        views[0].recommendedSwapchainSampleCount = 1;
        views[0].maxSwapchainSampleCount         = 1;

        views[1].recommendedImageRectWidth       = EYE_WIDTH;
        views[1].maxImageRectWidth               = 4096;
        views[1].recommendedImageRectHeight      = EYE_HEIGHT;
        views[1].maxImageRectHeight              = 4096;
        views[1].recommendedSwapchainSampleCount = 1;
        views[1].maxSwapchainSampleCount         = 1;
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateSwapchainFormats_impl(XrSession session, uint32_t format_capacity, uint32_t *format_count, int64_t *formats)
{
    TRACE_ENTER();

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (!format_count)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    uint32_t swapchain_format_count = 0;
    const int64_t *swapchain_formats = NULL;

#if GRAPHICS_API_D3D11
    const int64_t d3d11_formats[] = {
        // color
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        // depth (+stencil)
        DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        DXGI_FORMAT_D16_UNORM,
    };

    if (state.instance.graphics_api == GraphicsApiD3D11)
    {
        swapchain_format_count = ArrayCount(d3d11_formats);
        swapchain_formats = d3d11_formats;
    }
#endif

#if GRAPHICS_API_OPENGL
    const int64_t opengl_formats[] = {
        // color
        GL_SRGB8_ALPHA8,
        GL_RGBA8,
        // depth (+stencil)
        GL_DEPTH_COMPONENT32F,
        GL_DEPTH_COMPONENT32,
        GL_DEPTH_COMPONENT24,
    };

    if (state.instance.graphics_api == GraphicsApiOpenGl)
    {
        swapchain_format_count = ArrayCount(opengl_formats);
        swapchain_formats = opengl_formats;
    }
#endif

    *format_count = swapchain_format_count;

    if (format_capacity > 0)
    {
        if (format_capacity < swapchain_format_count)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_SIZE_INSUFFICIENT);
        }

        if (!formats)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
        }

        for (uint32_t i = 0; i < swapchain_format_count; i += 1)
        {
            formats[i] = swapchain_formats[i];
        }
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrCreateSwapchain_impl(XrSession session, const XrSwapchainCreateInfo *create_info, XrSwapchain *swapchain)
{
    TRACE_ENTER();

    if (!create_info || !swapchain)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if (create_info->type != XR_TYPE_SWAPCHAIN_CREATE_INFO)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (create_info->faceCount != 1)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_FEATURE_UNSUPPORTED);
    }

    // if (create_info->sampleCount != 1)
    // {
    //     TRACE_LEAVE_RESULT(XR_ERROR_FEATURE_UNSUPPORTED);
    // }

    if (create_info->arraySize == 0)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_FEATURE_UNSUPPORTED);
    }

    uint16_t swapchain_id = 0;

    for (uint16_t i = 0; i < ArrayCount(state.session.swapchains); i += 1)
    {
        if (!state.session.swapchains[i].active)
        {
            swapchain_id = i + 1;
            break;
        }
    }

    if (swapchain_id == 0)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_LIMIT_REACHED);
    }

    msg("create swapchain width = %u, height = %u, arraySize = %u, sampleCount = %u\n",
        create_info->width, create_info->height, create_info->arraySize, create_info->sampleCount);

    Swapchain *swch = state.session.swapchains + (swapchain_id - 1);

    // TODO: handle XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT
    // TODO: handle XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT

    switch (state.instance.graphics_api)
    {

#if GRAPHICS_API_D3D11
        case GraphicsApiD3D11:
        {
#  if PLATFORM_WIN32
            if (state.session.platform != PlatformWin32)
#  endif
            {
                TRACE_LEAVE_RESULT(XR_ERROR_RUNTIME_FAILURE);
            }

#  if PLATFORM_WIN32
            bool is_depth_format = d3d11_is_depth_format(create_info->format);
            DXGI_FORMAT texture_format = create_info->format;
            DXGI_FORMAT view_format = create_info->format;

            if (!is_depth_format)
            {
                texture_format = d3d11_get_typeless_format(texture_format);
                view_format = d3d11_get_non_srgb_format(view_format);
            }

            D3D11_TEXTURE2D_DESC texture_description;
            texture_description.Width              = create_info->width;
            texture_description.Height             = create_info->height;
            texture_description.MipLevels          = create_info->mipCount;
            texture_description.ArraySize          = create_info->arraySize;
            texture_description.Format             = texture_format;
            texture_description.SampleDesc.Count   = create_info->sampleCount;
            texture_description.SampleDesc.Quality = 0; // TODO: set based on SampleDesc.Count
            texture_description.Usage              = D3D11_USAGE_DEFAULT;
            texture_description.BindFlags          = 0;
            texture_description.CPUAccessFlags     = 0;
            texture_description.MiscFlags          = 0;

            if (is_depth_format)
            {
                if (create_info->usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                {
                    texture_description.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
                }
            }
            else
            {
                texture_description.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

                if (create_info->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT)
                {
                    texture_description.BindFlags |= D3D11_BIND_RENDER_TARGET;
                }
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC texture_view_description;
            texture_view_description.Format = view_format;
            texture_view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            texture_view_description.Texture2D.MostDetailedMip = 0;
            texture_view_description.Texture2D.MipLevels = -1;

            for (size_t i = 0; i < ArrayCount(swch->d3d11.textures); i += 1)
            {
                // TODO: check for errors
                if (FAILED(ID3D11Device_CreateTexture2D(state.session.win32.d3d11.device, &texture_description, NULL, swch->d3d11.textures + i)))
                {
                    msg("error: CreateTexture2D failed\n");
                }

                if (is_depth_format)
                {
                    swch->d3d11.texture_views[i] = NULL;
                }
                else
                {
                    // TODO: check for errors
                    ID3D11Device_CreateShaderResourceView(state.session.win32.d3d11.device, (ID3D11Resource *) swch->d3d11.textures[i],
                                                          &texture_view_description, swch->d3d11.texture_views + i);
                }
            }
#  endif
        } break;
#endif

#if GRAPHICS_API_OPENGL
        case GraphicsApiOpenGl:
        {
            glGenTextures(ArrayCount(swch->opengl.textures), swch->opengl.textures);

            for (size_t i = 0; i < ArrayCount(swch->opengl.textures); i += 1)
            {
                glBindTexture(GL_TEXTURE_2D, swch->opengl.textures[i]);
                // TODO: this was only introduced with opengl 4.2
                glTexStorage2D(GL_TEXTURE_2D, create_info->mipCount, create_info->format, create_info->width, create_info->height);
            }
        } break;
#endif

        default:
        {
            // TODO: return error
        } break;
    }

    swch->active = true;
    swch->next_image_index = 0;

    *swapchain = (XrSwapchain) (uintptr_t) (((uintptr_t) swapchain_id << 16) | (uintptr_t) swch->generation);

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static Swapchain *
get_swapchain(XrSwapchain swapchain)
{
    uint16_t swapchain_id = (uint16_t) ((uintptr_t) swapchain >> 16);
    uint16_t generation = (uint16_t) (uintptr_t) swapchain;

    if ((swapchain_id > 0) && (swapchain_id <= ArrayCount(state.session.swapchains)))
    {
        Swapchain *swch = state.session.swapchains + (swapchain_id - 1);

        if (swch->active && (swch->generation == generation))
        {
            return swch;
        }
    }

    return NULL;
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrDestroySwapchain_impl(XrSwapchain swapchain)
{
    TRACE_ENTER();

    Swapchain *swch = get_swapchain(swapchain);

    if (!swch)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    switch (state.instance.graphics_api)
    {

#if GRAPHICS_API_D3D11
        case GraphicsApiD3D11:
        {
#  if PLATFORM_WIN32
            if (state.session.platform != PlatformWin32)
#  endif
            {
                TRACE_LEAVE_RESULT(XR_ERROR_RUNTIME_FAILURE);
            }

#  if PLATFORM_WIN32
            for (size_t i = 0; i < ArrayCount(swch->d3d11.textures); i += 1)
            {
                if (swch->d3d11.texture_views[i])
                {
                    ID3D11ShaderResourceView_Release(swch->d3d11.texture_views[i]);
                }

                ID3D11Texture2D_Release(swch->d3d11.textures[i]);
            }
#  endif
        } break;
#endif

#if GRAPHICS_API_OPENGL
        case GraphicsApiOpenGl:
        {
            glDeleteTextures(ArrayCount(swch->opengl.textures), swch->opengl.textures);
        } break;
#endif

        default:
        {
            // TODO: return error
        } break;
    }

    if (swch->generation == 0xFFFF)
    {
        swch->generation = 0;
    }

    swch->generation += 1;
    swch->active = false;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateSwapchainImages_impl(XrSwapchain swapchain, uint32_t image_capacity, uint32_t *image_count, XrSwapchainImageBaseHeader *images)
{
    TRACE_ENTER();

    Swapchain *swch = get_swapchain(swapchain);

    if (!swch)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (!image_count)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    uint32_t graphics_api_image_count = 0;

    switch (state.instance.graphics_api)
    {

#if GRAPHICS_API_D3D11
        case GraphicsApiD3D11:
        {
            graphics_api_image_count = ArrayCount(swch->d3d11.textures);
        } break;
#endif

#if GRAPHICS_API_OPENGL
        case GraphicsApiOpenGl:
        {
            graphics_api_image_count = ArrayCount(swch->opengl.textures);
        } break;
#endif

        default:
        {
            // TODO: return error
        } break;
    }

    *image_count = graphics_api_image_count;

    if (image_capacity > 0)
    {
        if (image_capacity < graphics_api_image_count)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_SIZE_INSUFFICIENT);
        }

        if (!images)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
        }

        switch (state.instance.graphics_api)
        {

    #if GRAPHICS_API_D3D11
            case GraphicsApiD3D11:
            {
                XrSwapchainImageD3D11KHR *d3d11_swapchain_images = (XrSwapchainImageD3D11KHR *) images;

                for (size_t i = 0; i < ArrayCount(swch->d3d11.textures); i += 1)
                {
                    d3d11_swapchain_images[i].texture = swch->d3d11.textures[i];
                }
            } break;
    #endif

    #if GRAPHICS_API_OPENGL
            case GraphicsApiOpenGl:
            {
                XrSwapchainImageOpenGLKHR *opengl_swapchain_images = (XrSwapchainImageOpenGLKHR *) images;

                for (size_t i = 0; i < ArrayCount(swch->opengl.textures); i += 1)
                {
                    opengl_swapchain_images[i].image = swch->opengl.textures[i];
                }
            } break;
    #endif

            default:
            {
                // TODO: return error
            } break;
        }
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrAcquireSwapchainImage_impl(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo *acquire_info, uint32_t *index)
{
    TRACE_ENTER();

    Swapchain *swch = get_swapchain(swapchain);

    if (!swch)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (!index)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    *index = swch->next_image_index;
    swch->next_image_index = (swch->next_image_index + 1) % SWAPCHAIN_IMAGE_COUNT;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrWaitSwapchainImage_impl(XrSwapchain swapchain, const XrSwapchainImageWaitInfo *wait_info)
{
    TRACE_ENTER();

    Swapchain *swch = get_swapchain(swapchain);

    if (!swch)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (!wait_info)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrReleaseSwapchainImage_impl(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo *release_info)
{
    TRACE_ENTER();

    Swapchain *swch = get_swapchain(swapchain);

    if (!swch)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrBeginSession_impl(XrSession session, const XrSessionBeginInfo *begin_info)
{
    TRACE_ENTER();

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    change_state(XR_SESSION_STATE_SYNCHRONIZED);
    change_state(XR_SESSION_STATE_VISIBLE);
    // TODO: actually tie that to the window focus state
    change_state(XR_SESSION_STATE_FOCUSED);

    msg("TODO: implement %s\n", __func__);

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEndSession_impl(XrSession session)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrRequestExitSession_impl(XrSession session)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrWaitFrame_impl(XrSession session, const XrFrameWaitInfo *frame_wait_info, XrFrameState *frame_state)
{
    TRACE_ENTER();

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    switch (state.session.platform)
    {

#if PLATFORM_XLIB
        case PlatformXlib:
        {
            platform_xlib_wait_frame(&state.session.xlib, &state.session);
        } break;
#endif

#if PLATFORM_WIN32
        case PlatformWin32:
        {
            platform_win32_wait_frame(&state.session.win32, &state.session);
        } break;
#endif

#if PLATFORM_WAYLAND
        case PlatformWayland:
        {
        } break;
#endif

    }

    frame_state->shouldRender = XR_TRUE;
    frame_state->predictedDisplayPeriod = 1000000000 / TARGET_FRAME_RATE;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrBeginFrame_impl(XrSession session, const XrFrameBeginInfo *frame_begin_info)
{
    TRACE_ENTER();

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEndFrame_impl(XrSession session, const XrFrameEndInfo *frame_end_info)
{
    TRACE_ENTER();

    if (!frame_end_info)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    const XrCompositionLayerProjection *projection_layer = NULL;

    for (uint32_t i = 0; i < frame_end_info->layerCount; i += 1)
    {
        const XrCompositionLayerBaseHeader * const layer = frame_end_info->layers[i];

        if (layer->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
        {
            projection_layer = (XrCompositionLayerProjection *) layer;
            break;
        }
    }

    if (projection_layer)
    {
        const XrCompositionLayerProjectionView *left_view = projection_layer->views + 0;

        Swapchain *left_swapchain = get_swapchain(left_view->subImage.swapchain);

        if (!left_swapchain)
        {
            // TODO: return ;
        }

        const XrCompositionLayerProjectionView *right_view = projection_layer->views + 1;

        Swapchain *right_swapchain = get_swapchain(right_view->subImage.swapchain);

        if (!right_swapchain)
        {
            // TODO: return ;
        }

        Texture left_texture = { ._data = 0 };
        Texture right_texture = { ._data = 0 };
        Vertex *vertices = NULL;

        uint32_t graphics_api_color = 0xFFFFFFFF;
        const char *graphics_api_name = "UNKNOWN GRAPHICS API";

        float y0 = 1.0f;
        float y1 = 0.0f;

        switch (state.instance.graphics_api)
        {

#if GRAPHICS_API_D3D11
            case GraphicsApiD3D11:
            {
#  if PLATFORM_WIN32
                if (state.session.platform != PlatformWin32)
#  endif
                {
                    TRACE_LEAVE_RESULT(XR_ERROR_RUNTIME_FAILURE);
                }

#  if PLATFORM_WIN32
                vertices = platform_win32_d3d11_begin_drawing(&state.session.win32);
#  endif

                left_texture.d3d11.texture = left_swapchain->d3d11.textures[left_view->subImage.imageArrayIndex];
                left_texture.d3d11.texture_view = left_swapchain->d3d11.texture_views[left_view->subImage.imageArrayIndex];
                right_texture.d3d11.texture = right_swapchain->d3d11.textures[right_view->subImage.imageArrayIndex];
                right_texture.d3d11.texture_view = right_swapchain->d3d11.texture_views[right_view->subImage.imageArrayIndex];

                graphics_api_name = "Direct3D 11";

                y0 = 0.0f;
                y1 = 1.0f;
            } break;
#endif

#if GRAPHICS_API_OPENGL
            case GraphicsApiOpenGl:
            {
                switch (state.session.platform)
                {

#  if PLATFORM_XLIB
                    case PlatformXlib:
                    {
                        vertices = platform_xlib_opengl_begin_drawing(&state.session.xlib);
                    } break;
#  endif

#  if PLATFORM_WIN32
                    case PlatformWin32:
                    {
                        vertices = platform_win32_opengl_begin_drawing(&state.session.win32);
                    } break;
#  endif

#  if PLATFORM_WAYLAND
                    case PlatformWayland:
                    {
                    } break;
#  endif

                }

                left_texture.opengl = left_swapchain->opengl.textures[left_view->subImage.imageArrayIndex];
                right_texture.opengl = right_swapchain->opengl.textures[right_view->subImage.imageArrayIndex];

                graphics_api_name = "OpenGL";
            } break;
#endif

            default:
            {
                // TODO: return error
            } break;
        }

        float px4 = 4.0f;
        float px6 = 6.0f;
        float px8 = 8.0f;

        float width = (float) state.session.width;
        float height = (float) state.session.height;

        float eye_x = px8;
        float eye_y = px6 + px4 + (float) terminus_16_bold_font.size;

        DrawCommand commands[8];

        DrawContext ctx;
        ctx.vertex_count = 0;
        ctx.max_vertex_count = MAX_VERTEX_COUNT;
        ctx.vertices = vertices;
        ctx.command_count = 0;
        ctx.max_command_count = ArrayCount(commands);
        ctx.commands = commands;
        ctx.x_scale = 2.0f / width;
        ctx.y_scale = -2.0f / height;

        set_texture(&ctx, left_texture);
        push_quad(&ctx, eye_x, eye_y, eye_x + (float) EYE_WIDTH, eye_y + (float) EYE_HEIGHT, 0.0f, y0, 1.0f, y1, 0xFFFFFFFF);
        set_texture(&ctx, right_texture);
        push_quad(&ctx, eye_x + (float) EYE_WIDTH, eye_y, eye_x + (float) (2 * EYE_WIDTH), eye_y + (float) EYE_HEIGHT, 0.0f, y0, 1.0f, y1, 0xFFFFFFFF);

        set_texture(&ctx, state.session.font_texture);
        draw_string(&ctx, &terminus_16_bold_font, px8 + px4, px6 + terminus_16_bold_font.ascent, graphics_api_name, graphics_api_color);

        switch (state.instance.graphics_api)
        {

#if GRAPHICS_API_D3D11
            case GraphicsApiD3D11:
            {
#  if PLATFORM_WIN32
                if (state.session.platform != PlatformWin32)
#  endif
                {
                    TRACE_LEAVE_RESULT(XR_ERROR_RUNTIME_FAILURE);
                }

#  if PLATFORM_WIN32
                platform_win32_d3d11_finish_drawing(&state.session.win32, &ctx);
#  endif
            } break;
#endif

#if GRAPHICS_API_OPENGL
            case GraphicsApiOpenGl:
            {
                switch (state.session.platform)
                {

#  if PLATFORM_XLIB
                    case PlatformXlib:
                    {
                        platform_xlib_opengl_finish_drawing(&state.session.xlib, &ctx);
                    } break;
#  endif

#  if PLATFORM_WIN32
                    case PlatformWin32:
                    {
                        platform_win32_opengl_finish_drawing(&state.session.win32, &ctx);
                    } break;
#  endif

#  if PLATFORM_WAYLAND
                    case PlatformWayland:
                    {
                    } break;
#  endif

                }
            } break;
#endif

            default:
            {
                // TODO: return error
            } break;
        }
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrLocateViews_impl(XrSession session, const XrViewLocateInfo *view_info, XrViewState *view_state, uint32_t view_capacity, uint32_t *view_count, XrView *views)
{
    TRACE_ENTER();

    if (!view_count)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    *view_count = 2;

    if (view_state)
    {
        view_state->viewStateFlags = XR_VIEW_STATE_ORIENTATION_VALID_BIT |
                                     XR_VIEW_STATE_POSITION_VALID_BIT |
                                     XR_VIEW_STATE_ORIENTATION_TRACKED_BIT |
                                     XR_VIEW_STATE_POSITION_TRACKED_BIT;
    }

    if (view_capacity > 0)
    {
        if (view_capacity < 2)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_SIZE_INSUFFICIENT);
        }

        if (!views)
        {
            TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
        }

        float eye_distance_m = 0.062f;

        XrQuaternionf orientation = quaternion_from_orbit_and_pitch(state.session.head_orbit, state.session.head_pitch);

        views[0].pose.orientation = orientation;
        views[0].pose.position    = vec3_add(state.session.head_position, quaternion_apply(orientation, (XrVector3f) { -0.5f * eye_distance_m, 0.0f, 0.0f }));
        views[0].fov.angleLeft    = -0.7f;
        views[0].fov.angleRight   =  0.7f;
        views[0].fov.angleUp      =  0.7f;
        views[0].fov.angleDown    = -0.7f;

        views[1].pose.orientation = orientation;
        views[1].pose.position    = vec3_add(state.session.head_position, quaternion_apply(orientation, (XrVector3f) { 0.5f * eye_distance_m, 0.0f, 0.0f }));
        views[1].fov.angleLeft    = -0.7f;
        views[1].fov.angleRight   =  0.7f;
        views[1].fov.angleUp      =  0.7f;
        views[1].fov.angleDown    = -0.7f;
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrStringToPath_impl(XrInstance instance, const char *str, XrPath *path)
{
    TRACE_ENTER();

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    // djb2 hash
    uint64_t hash = 5381;
    String path_str = C(str);

    for (uint64_t i = 0; i < path_str.count; i += 1)
    {
        hash = ((hash << 5) + hash) + path_str.data[i];
    }

    // TODO: store in hash map
    uint32_t index = (uint32_t) (hash % MAX_PATH_STRING_COUNT);
    *path = (XrPath) index;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrPathToString_impl(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrCreateActionSet_impl(XrInstance instance, const XrActionSetCreateInfo *create_info, XrActionSet *action_set)
{
    TRACE_ENTER();

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (create_info->type != XR_TYPE_ACTION_SET_CREATE_INFO)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    *action_set = (XrActionSet) state.instance.next_action_set_handle;
    state.instance.next_action_set_handle += 1;

    msg("create action set '%s'\n", create_info->actionSetName);

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrDestroyActionSet_impl(XrActionSet actionSet)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrCreateAction_impl(XrActionSet action_set, const XrActionCreateInfo *create_info, XrAction *action)
{
    TRACE_ENTER();

    if (!state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (create_info->type != XR_TYPE_ACTION_CREATE_INFO)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    *action = (XrAction) state.instance.next_action_handle;
    state.instance.next_action_handle += 1;

    msg("create action '%s'\n", create_info->actionName);

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrDestroyAction_impl(XrAction action)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrSuggestInteractionProfileBindings_impl(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrAttachSessionActionSets_impl(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetCurrentInteractionProfile_impl(XrSession session, XrPath toplevel_user_path, XrInteractionProfileState *interaction_profile)
{
    TRACE_ENTER();

    if (!interaction_profile)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((session != SESSION_HANDLE) || !state.session.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    interaction_profile->interactionProfile = XR_NULL_PATH;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetActionStateBoolean_impl(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetActionStateFloat_impl(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetActionStateVector2f_impl(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetActionStatePose_impl(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrSyncActions_impl(XrSession session, const XrActionsSyncInfo *sync_info)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrEnumerateBoundSourcesForAction_impl(XrSession session, const XrBoundSourcesForActionEnumerateInfo* enumerateInfo, uint32_t sourceCapacityInput, uint32_t* sourceCountOutput, XrPath* sources)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetInputSourceLocalizedName_impl(XrSession session, const XrInputSourceLocalizedNameGetInfo* getInfo, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrApplyHapticFeedback_impl(XrSession session, const XrHapticActionInfo* hapticActionInfo, const XrHapticBaseHeader* hapticFeedback)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrStopHapticFeedback_impl(XrSession session, const XrHapticActionInfo* hapticActionInfo)
{
    TRACE_ENTER();

    msg("TODO: implement %s\n", __func__);
    // TODO: implement
    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

typedef struct
{
    String name;
    PFN_xrVoidFunction func;
} FunctionRef;

static const FunctionRef core_procedures[] = {
#define CORE_FUNC(name) { { sizeof(#name) - 1, (uint8_t *) #name }, (PFN_xrVoidFunction) name##_impl }
    CORE_FUNC(xrDestroyInstance),
    CORE_FUNC(xrGetInstanceProperties),
    CORE_FUNC(xrPollEvent),
    CORE_FUNC(xrResultToString),
    CORE_FUNC(xrStructureTypeToString),
    CORE_FUNC(xrGetSystem),
    CORE_FUNC(xrGetSystemProperties),
    CORE_FUNC(xrEnumerateEnvironmentBlendModes),
    CORE_FUNC(xrCreateSession),
    CORE_FUNC(xrDestroySession),
    CORE_FUNC(xrEnumerateReferenceSpaces),
    CORE_FUNC(xrCreateReferenceSpace),
    CORE_FUNC(xrGetReferenceSpaceBoundsRect),
    CORE_FUNC(xrCreateActionSpace),
    CORE_FUNC(xrLocateSpace),
    CORE_FUNC(xrDestroySpace),
    CORE_FUNC(xrEnumerateViewConfigurations),
    CORE_FUNC(xrGetViewConfigurationProperties),
    CORE_FUNC(xrEnumerateViewConfigurationViews),
    CORE_FUNC(xrEnumerateSwapchainFormats),
    CORE_FUNC(xrCreateSwapchain),
    CORE_FUNC(xrDestroySwapchain),
    CORE_FUNC(xrEnumerateSwapchainImages),
    CORE_FUNC(xrAcquireSwapchainImage),
    CORE_FUNC(xrWaitSwapchainImage),
    CORE_FUNC(xrReleaseSwapchainImage),
    CORE_FUNC(xrBeginSession),
    CORE_FUNC(xrEndSession),
    CORE_FUNC(xrRequestExitSession),
    CORE_FUNC(xrWaitFrame),
    CORE_FUNC(xrBeginFrame),
    CORE_FUNC(xrEndFrame),
    CORE_FUNC(xrLocateViews),
    CORE_FUNC(xrStringToPath),
    CORE_FUNC(xrPathToString),
    CORE_FUNC(xrCreateActionSet),
    CORE_FUNC(xrDestroyActionSet),
    CORE_FUNC(xrCreateAction),
    CORE_FUNC(xrDestroyAction),
    CORE_FUNC(xrSuggestInteractionProfileBindings),
    CORE_FUNC(xrAttachSessionActionSets),
    CORE_FUNC(xrGetCurrentInteractionProfile),
    CORE_FUNC(xrGetActionStateBoolean),
    CORE_FUNC(xrGetActionStateFloat),
    CORE_FUNC(xrGetActionStateVector2f),
    CORE_FUNC(xrGetActionStatePose),
    CORE_FUNC(xrSyncActions),
    CORE_FUNC(xrEnumerateBoundSourcesForAction),
    CORE_FUNC(xrGetInputSourceLocalizedName),
    CORE_FUNC(xrApplyHapticFeedback),
    CORE_FUNC(xrStopHapticFeedback),
#undef CORE_FUNC
};

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetInstanceProcAddr_impl(XrInstance instance, const char *proc_name, PFN_xrVoidFunction *fn)
{
    TRACE_ENTER();

    if (!proc_name)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    if (!fn)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    String name = C(proc_name);

    if (strings_are_equal(name, S("xrEnumerateInstanceExtensionProperties")))
    {
        msg("    FOUND xrGetInstanceProcAddr(%s)\n", proc_name);
        *fn = (PFN_xrVoidFunction) xrEnumerateInstanceExtensionProperties_impl;
        TRACE_LEAVE_RESULT(XR_SUCCESS);
    }

    if (strings_are_equal(name, S("xrEnumerateApiLayerProperties")))
    {
        msg("    FOUND xrGetInstanceProcAddr(%s)\n", proc_name);
        *fn = (PFN_xrVoidFunction) xrEnumerateApiLayerProperties_impl;
        TRACE_LEAVE_RESULT(XR_SUCCESS);
    }

    if (strings_are_equal(name, S("xrCreateInstance")))
    {
        msg("    FOUND xrGetInstanceProcAddr(%s)\n", proc_name);
        *fn = (PFN_xrVoidFunction) xrCreateInstance_impl;
        TRACE_LEAVE_RESULT(XR_SUCCESS);
    }

    *fn = NULL;

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    for (size_t i = 0; i < ArrayCount(core_procedures); i += 1)
    {
        if (strings_are_equal(name, core_procedures[i].name))
        {
            msg("    FOUND xrGetInstanceProcAddr(%s)\n", proc_name);
            *fn = core_procedures[i].func;
            TRACE_LEAVE_RESULT(XR_SUCCESS);
        }
    }

#if PLATFORM_WIN32
    if (state.instance.enabled_XR_KHR_win32_convert_performance_counter_time)
    {
        if (strings_are_equal(name, S("xrConvertWin32PerformanceCounterToTimeKHR")))
        {
            msg("    FOUND xrGetInstanceProcAddr(%s)\n", proc_name);
            *fn = (PFN_xrVoidFunction) xrConvertWin32PerformanceCounterToTimeKHR_impl;
            TRACE_LEAVE_RESULT(XR_SUCCESS);
        }

        if (strings_are_equal(name, S("xrConvertTimeToWin32PerformanceCounterKHR")))
        {
            msg("    FOUND xrGetInstanceProcAddr(%s)\n", proc_name);
            *fn = (PFN_xrVoidFunction) xrConvertTimeToWin32PerformanceCounterKHR_impl;
            TRACE_LEAVE_RESULT(XR_SUCCESS);
        }
    }
#endif

#if GRAPHICS_API_D3D11
    if (state.instance.enabled_XR_KHR_d3d11_enable)
    {
        if (strings_are_equal(name, S("xrGetD3D11GraphicsRequirementsKHR")))
        {
            msg("    FOUND xrGetInstanceProcAddr(%s)\n", proc_name);
            *fn = (PFN_xrVoidFunction) xrGetD3D11GraphicsRequirementsKHR_impl;
            TRACE_LEAVE_RESULT(XR_SUCCESS);
        }
    }
#endif

#if GRAPHICS_API_OPENGL
    if (state.instance.enabled_XR_KHR_opengl_enable)
    {
        if (strings_are_equal(name, S("xrGetOpenGLGraphicsRequirementsKHR")))
        {
            msg("    FOUND xrGetInstanceProcAddr(%s)\n", proc_name);
            *fn = (PFN_xrVoidFunction) xrGetOpenGLGraphicsRequirementsKHR_impl;
            TRACE_LEAVE_RESULT(XR_SUCCESS);
        }
    }
#endif

    msg("NOT FOUND xrGetInstanceProcAddr(%s)\n", proc_name);

    TRACE_LEAVE_RESULT(XR_ERROR_FUNCTION_UNSUPPORTED);
}

XRAPI_ATTR XrResult XRAPI_CALL
xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo *loader_info, XrNegotiateRuntimeRequest *runtime_request)
{
    TRACE_ENTER();

    if (!loader_info || !runtime_request)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_INITIALIZATION_FAILED);
    }

    if ((loader_info->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO) ||
        (loader_info->structVersion != XR_LOADER_INFO_STRUCT_VERSION) ||
        (loader_info->structSize != sizeof(XrNegotiateLoaderInfo)))
    {
        TRACE_LEAVE_RESULT(XR_ERROR_INITIALIZATION_FAILED);
    }

    if ((runtime_request->structType != XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST) ||
        (runtime_request->structVersion != XR_RUNTIME_INFO_STRUCT_VERSION) ||
        (runtime_request->structSize != sizeof(XrNegotiateRuntimeRequest)))
    {
        TRACE_LEAVE_RESULT(XR_ERROR_INITIALIZATION_FAILED);
    }

    if (!state.initialized)
    {
        for (size_t i = 0; i < ArrayCount(state.session.swapchains); i += 1)
        {
            state.session.swapchains[i].active = false;
            state.session.swapchains[i].generation = 1;
        }

        state.supported_extension_count = 0;

#if PLATFORM_WIN32
        if (state.supported_extension_count < ArrayCount(state.supported_extensions))
        {
            Extension *extension = state.supported_extensions + state.supported_extension_count++;
            extension->name = S(XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME);
            extension->version = 1;
        }
#endif

#if GRAPHICS_API_D3D11
        if (state.supported_extension_count < ArrayCount(state.supported_extensions))
        {
            Extension *extension = state.supported_extensions + state.supported_extension_count++;
            extension->name = S(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
            extension->version = 1;
        }
#endif

#if GRAPHICS_API_OPENGL
        if (state.supported_extension_count < ArrayCount(state.supported_extensions))
        {
            Extension *extension = state.supported_extensions + state.supported_extension_count++;
            extension->name = S(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
            extension->version = 1;
        }
#endif

        msg("%u extensions supported:\n", state.supported_extension_count);

        for (uint32_t i = 0; i < state.supported_extension_count; i += 1)
        {
            Extension *extension = state.supported_extensions + i;
            msg("  - %.*s\n", (int) extension->name.count, extension->name.data);
        }

        state.initialized = true;
    }

    runtime_request->runtimeInterfaceVersion = 1;
    runtime_request->runtimeApiVersion       = XR_API_VERSION_1_0;
    runtime_request->getInstanceProcAddr     = xrGetInstanceProcAddr_impl;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}
