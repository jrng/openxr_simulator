#include <wayland-client.h>

#include "xdg-shell.h"
#include "xdg-shell.c"

typedef struct
{
    struct wl_display *display;

    struct wl_compositor *compositor;
    struct xdg_wm_base *xdg_wm_base;

    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;

    uint32_t configure_serial;
} PlatformWaylandState;

static void
wayland_registry_add(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    PlatformWaylandState *platform_wayland = (PlatformWaylandState *) data;

    String interface_name = C(interface);

    if (strings_are_equal(interface_name, C(wl_compositor_interface.name)))
    {
        if (version >= 3)
        {
            platform_wayland->compositor = (struct wl_compositor *) wl_registry_bind(registry, name, &wl_compositor_interface, 3);
        }
    }
    else if (strings_are_equal(interface_name, C(xdg_wm_base_interface.name)))
    {
        if (version >= 1)
        {
            platform_wayland->xdg_wm_base = (struct xdg_wm_base *) wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        }
    }
}

static void
wayland_registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener wayland_registry_listener = {
    .global        = wayland_registry_add,
    .global_remove = wayland_registry_remove,
};

static void
wayland_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    (void) data;
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener wayland_xdg_wm_base_listener = {
    .ping = wayland_xdg_wm_base_ping,
};

static void
wayland_surface_enter(void *data, struct wl_surface *surface, struct wl_output *output)
{
}

static void
wayland_surface_leave(void *data, struct wl_surface *surface, struct wl_output *output)
{
}

static const struct wl_surface_listener wayland_surface_listener = {
    .enter = wayland_surface_enter,
    .leave = wayland_surface_leave,
};

static void
wayland_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    (void) xdg_surface;

    PlatformWaylandState *platform_wayland = (PlatformWaylandState *) data;
    platform_wayland->configure_serial = serial;
}

static const struct xdg_surface_listener wayland_xdg_surface_listener = {
    .configure = wayland_xdg_surface_configure,
};

static void
wayland_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states)
{
    (void) data;
    (void) xdg_toplevel;
    (void) width;
    (void) height;
    (void) states;
}

static void
wayland_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    (void) xdg_toplevel;

    PlatformWaylandState *platform_wayland = (PlatformWaylandState *) data;
}

static const struct xdg_toplevel_listener wayland_xdg_toplevel_listener = {
    .configure = wayland_xdg_toplevel_configure,
    .close     = wayland_xdg_toplevel_close,
};

static bool
initialize_platform_wayland_opengl(PlatformWaylandState *platform_wayland, int32_t window_width, int32_t window_height)
{
    platform_wayland->configure_serial = 0;
    platform_wayland->display = wl_display_connect(NULL);

    if (!platform_wayland->display)
    {
        return false;
    }

    struct wl_registry *registry = wl_display_get_registry(platform_wayland->display);

    wl_registry_add_listener(registry, &wayland_registry_listener, platform_wayland);
    wl_display_roundtrip(platform_wayland->display);

    if (!platform_wayland->compositor)
    {
        wl_display_disconnect(platform_wayland->display);
        platform_wayland->display = NULL;
        return false;
    }

    if (!platform_wayland->xdg_wm_base)
    {
        wl_display_disconnect(platform_wayland->display);
        platform_wayland->display = NULL;
        return false;
    }

    xdg_wm_base_add_listener(platform_wayland->xdg_wm_base, &wayland_xdg_wm_base_listener, platform_wayland);

    platform_wayland->surface = wl_compositor_create_surface(platform_wayland->compositor);
    platform_wayland->xdg_surface = xdg_wm_base_get_xdg_surface(platform_wayland->xdg_wm_base, platform_wayland->surface);
    platform_wayland->xdg_toplevel = xdg_surface_get_toplevel(platform_wayland->xdg_surface);

    wl_surface_add_listener(platform_wayland->surface, &wayland_surface_listener, platform_wayland);
    xdg_surface_add_listener(platform_wayland->xdg_surface, &wayland_xdg_surface_listener, platform_wayland);
    xdg_toplevel_add_listener(platform_wayland->xdg_toplevel, &wayland_xdg_toplevel_listener, platform_wayland);

    xdg_toplevel_set_title(platform_wayland->xdg_toplevel, "OpenXR Viewer");
    xdg_toplevel_set_app_id(platform_wayland->xdg_toplevel, "openxr_simulator");

    xdg_toplevel_set_min_size(platform_wayland->xdg_toplevel, window_width, window_height);
    xdg_toplevel_set_max_size(platform_wayland->xdg_toplevel, window_width, window_height);

    wl_surface_commit(platform_wayland->surface);

    // TOOD: initialize graphics here

    while (!platform_wayland->configure_serial)
    {
        // TODO: put flush before the loop?
        // TODO: use wl_display_roundtrip?
        wl_display_flush(platform_wayland->display);
        wl_display_dispatch(platform_wayland->display);
    }

    return true;
}
