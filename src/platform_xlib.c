static int32_t
parse_desktop_settings(String settings)
{
    if (!settings.count)
    {
        return 1;
    }

    bool is_little_endian = !settings.data[0];

    if (settings.count < 12)
    {
        return 1;
    }

    string_advance(&settings, 12);

    int32_t gdk_window_scaling_factor;
    int32_t xft_dpi_scaling;

    bool has_xft_dpi_scaling = false;
    bool has_gdk_window_scaling_factor = false;

    while (settings.count >= 4)
    {
        uint8_t type = settings.data[0];

        String name;

        if (is_little_endian)
        {
            name.count = ((uint16_t) settings.data[3] << 8) | (uint16_t) settings.data[2];
        }
        else
        {
            name.count = ((uint16_t) settings.data[2] << 8) | (uint16_t) settings.data[3];
        }

        name.data = settings.data + 4;

        if (settings.count < name.count)
        {
            break;
        }

        string_advance(&settings, 8 + Align(name.count, 4));

        switch (type)
        {
            case 0: /* XSettingsTypeInteger */
            {
                int32_t value;

                if (is_little_endian)
                {
                    value = ((int32_t) settings.data[3] << 24) | ((int32_t) settings.data[2] << 16) | ((int32_t) settings.data[1] << 8) | (int32_t) settings.data[0];
                }
                else
                {
                    value = ((int32_t) settings.data[0] << 24) | ((int32_t) settings.data[1] << 16) | ((int32_t) settings.data[2] << 8) | (int32_t) settings.data[3];
                }

                string_advance(&settings, 4);

                if (strings_are_equal(name, S("Gdk/WindowScalingFactor")))
                {
                    has_gdk_window_scaling_factor = true;
                    gdk_window_scaling_factor = value;
                }
                else if (strings_are_equal(name, S("Xft/DPI")))
                {
                    has_xft_dpi_scaling = true;
                    xft_dpi_scaling = ((value / 1024) + 48) / 96;
                }
            } break;

            case 1: /* XSettingsTypeString */
            {
                String str;

                if (is_little_endian)
                {
                    str.count = ((uint32_t) settings.data[3] << 24) | ((uint32_t) settings.data[2] << 16) | ((uint32_t) settings.data[1] << 8) | (uint32_t) settings.data[0];
                }
                else
                {
                    str.count = ((uint32_t) settings.data[0] << 24) | ((uint32_t) settings.data[1] << 16) | ((uint32_t) settings.data[2] << 8) | (uint32_t) settings.data[3];
                }

                str.data = settings.data + 4;

                string_advance(&settings, 4 + Align(str.count, 4));
            } break;

            case 2: /* XSettingsTypeColor */
            {
                string_advance(&settings, 8);
            } break;
        }
    }

    int32_t ui_scale = 1;

    if (has_gdk_window_scaling_factor)
    {
        ui_scale = gdk_window_scaling_factor;
    }
    else if (has_xft_dpi_scaling)
    {
        ui_scale = xft_dpi_scaling;
    }

    return ui_scale;
}

static int32_t
platform_xlib_get_ui_scale(Display *display)
{
    int32_t ui_scale = 1;

    int screen = DefaultScreen(display);

    char xsettings_screen_str[32];
    snprintf(xsettings_screen_str, sizeof(xsettings_screen_str), "_XSETTINGS_S%d", screen);

    Atom xsettings_screen = XInternAtom(display, xsettings_screen_str, False);
    Atom xsettings_settings = XInternAtom(display, "_XSETTINGS_SETTINGS", False);

    XGrabServer(display);

    Window settings_window = XGetSelectionOwner(display, xsettings_screen);

    if (settings_window != None)
    {
        XSelectInput(display, settings_window, StructureNotifyMask | PropertyChangeMask);

        Atom type;
        int format;
        unsigned long length, remaining;
        unsigned char *settings_buffer = NULL;

        int ret = XGetWindowProperty(display, settings_window, xsettings_settings,
                                     0, 2048, False, AnyPropertyType, &type, &format, &length, &remaining, &settings_buffer);

        if (ret == Success)
        {
            if ((length > 0) && (remaining == 0) && (format == 8))
            {
                String settings_str;
                settings_str.count = length;
                settings_str.data = settings_buffer;
                ui_scale = parse_desktop_settings(settings_str);
            }

            XFree(settings_buffer);
        }
    }

    XUngrabServer(display);

    return ui_scale;
}

#if GRAPHICS_API_OPENGL

typedef GLXContext glXCreateContextAttribsARB(Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);

typedef void glXSwapIntervalEXT(Display *dpy, GLXDrawable drawable, int interval);
typedef int glXSwapIntervalMESA(int interval);

static void
initialize_platform_xlib_opengl(PlatformXlibState *platform_xlib, Display *display, GLXDrawable client_drawable, GLXContext client_context, int32_t window_width, int32_t window_height)
{
    platform_xlib->display         = display;
    platform_xlib->client_drawable = client_drawable;
    platform_xlib->client_context  = client_context;

    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    msg("OpenGL version %d.%d\n", major, minor);

    int buffer_attr[] = {
        GLX_RED_SIZE    ,    8,
        GLX_GREEN_SIZE  ,    8,
        GLX_BLUE_SIZE   ,    8,
        GLX_DEPTH_SIZE  ,   24,
        GLX_DOUBLEBUFFER, True,
        None,
    };

    int fbcount;
    GLXFBConfig *fbconfigs = glXChooseFBConfig(platform_xlib->display, DefaultScreen(platform_xlib->display), buffer_attr, &fbcount);

    if (!fbconfigs || !fbcount)
    {
        // TODO: return failure;
        return;
    }

    int32_t best_config_index = -1;

    // TODO: choose best config
    best_config_index = 0;

    if (best_config_index < 0)
    {
        XFree(fbconfigs);
        // TODO: return failure;
        return;
    }

    GLXFBConfig fbconfig = fbconfigs[best_config_index];
    XFree(fbconfigs);

    XVisualInfo *visual_info = glXGetVisualFromFBConfig(platform_xlib->display, fbconfig);

    Window root_window = RootWindow(platform_xlib->display, visual_info->screen);

    XSetWindowAttributes window_attrs;
    window_attrs.colormap = XCreateColormap(platform_xlib->display, root_window, visual_info->visual, AllocNone);
    window_attrs.event_mask = ButtonPressMask |
                              ButtonReleaseMask |
                              KeyPressMask |
                              KeyReleaseMask |
                              PointerMotionMask |
                              EnterWindowMask |
                              LeaveWindowMask;

    platform_xlib->window = XCreateWindow(platform_xlib->display, root_window, 0, 0, window_width, window_height,
                                          0, visual_info->depth ,InputOutput, visual_info->visual,
                                          CWColormap | CWEventMask, &window_attrs);

    XFree(visual_info);

    if (!platform_xlib->window)
    {
        // TODO: return failure;
        return;
    }

    platform_xlib->wm_protocols = XInternAtom(platform_xlib->display, "WM_PROTOCOLS", False);
    platform_xlib->wm_delete_window = XInternAtom(platform_xlib->display, "WM_DELETE_WINDOW", False);

    XSizeHints sh = {};
    sh.flags = PMinSize | PMaxSize | PWinGravity;
    sh.win_gravity = StaticGravity;
    sh.min_width  = sh.max_width  = window_width;
    sh.min_height = sh.max_height = window_height;

    XSetWMNormalHints(platform_xlib->display, platform_xlib->window, &sh);
    XSetWMProtocols(platform_xlib->display, platform_xlib->window, &platform_xlib->wm_delete_window, 1);

    XStoreName(platform_xlib->display, platform_xlib->window, "OpenXR Viewer");

    bool has_ARB_create_context = false;
    bool has_ARB_create_context_profile = false;
    bool has_EXT_swap_control = false;
    bool has_MESA_swap_control = false;
    bool has_EXT_swap_control_tear = false;

    char *exts = (char *) glXQueryExtensionsString(platform_xlib->display, DefaultScreen(platform_xlib->display));

    char *start = exts;

    while (*start)
    {
        while (*start == ' ')  start += 1;
        char *end = start;
        while (*end && *end != ' ')  end += 1;

        uint64_t length = end - start;

        String name;
        name.count = end - start;
        name.data  = start;

        if (strings_are_equal(name, S("GLX_ARB_create_context")))
        {
            has_ARB_create_context = true;
        }
        else if (strings_are_equal(name, S("GLX_ARB_create_context_profile")))
        {
            has_ARB_create_context_profile = true;
        }
        else if (strings_are_equal(name, S("GLX_EXT_swap_control")))
        {
            has_EXT_swap_control = true;
        }
        else if (strings_are_equal(name, S("GLX_MESA_swap_control")))
        {
            has_MESA_swap_control = true;
        }
        else if (strings_are_equal(name, S("GLX_EXT_swap_control_tear")))
        {
            has_EXT_swap_control_tear = true;
        }

        start = end;
    }

    if (!has_ARB_create_context || !has_ARB_create_context_profile)
    {
        // TODO: return failure;
        return;
    }

    glXCreateContextAttribsARB *glXCreateContextAttribs =
        (glXCreateContextAttribsARB *) glXGetProcAddress((const GLubyte *) "glXCreateContextAttribsARB");

    if (!glXCreateContextAttribs)
    {
        // TODO: return failure;
        return;
    }

    // TODO: use outer context version
    int context_attr[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 2,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    // TODO: share context
    platform_xlib->context = glXCreateContextAttribs(platform_xlib->display, fbconfig, platform_xlib->client_context, true, context_attr);

    if (!platform_xlib->context)
    {
        // TODO: return failure;
        return;
    }

    if (!glXMakeCurrent(platform_xlib->display, platform_xlib->window, platform_xlib->context))
    {
        // TODO: return failure;
        return;
    }

#define load_opengl_function(prototype, name)                          \
    do {                                                               \
        name = (prototype) glXGetProcAddress(#name);                   \
        if (!name)                                                     \
        {                                                              \
            msg("error: could not load opengl function '" #name "'\n");\
            glXMakeCurrent(display, platform_xlib->client_drawable,    \
                           platform_xlib->client_context);             \
            return;                                                    \
        }                                                              \
    } while (0)

#define OPENGL_FUNCTION(type, name) load_opengl_function(type, name)

#include "opengl_functions.h"

#undef load_opengl_function

    glXSwapIntervalEXT *glXSwapInterval_ext = 0;
    glXSwapIntervalMESA *glXSwapInterval_mesa = 0;

    if (has_EXT_swap_control)
    {
        glXSwapInterval_ext = (glXSwapIntervalEXT *) glXGetProcAddress((const GLubyte *) "glXSwapIntervalEXT");
    }
    else if (has_MESA_swap_control)
    {
        glXSwapInterval_mesa = (glXSwapIntervalMESA *) glXGetProcAddress((const GLubyte *) "glXSwapIntervalMESA");
    }

    if (glXSwapInterval_ext)
    {
        glXSwapInterval_ext(platform_xlib->display, platform_xlib->window, 1);
    }
    else if (glXSwapInterval_mesa)
    {
        glXSwapInterval_mesa(1);
    }

    initialize_opengl(&platform_xlib->opengl);

    XMapWindow(platform_xlib->display, platform_xlib->window);

    glXMakeCurrent(platform_xlib->display, platform_xlib->client_drawable, platform_xlib->client_context);
}

static void
deinitialize_platform_xlib_opengl(PlatformXlibState *platform_xlib)
{
    deinitialize_opengl(&platform_xlib->opengl);

    // TODO: the rest
}

#endif

static void
platform_xlib_wait_frame(PlatformXlibState *platform_xlib, PlatformInput *input)
{
    while (XPending(platform_xlib->display))
    {
        XEvent ev;
        XNextEvent(platform_xlib->display, &ev);

        if (XFilterEvent(&ev, platform_xlib->window)) continue;

        switch (ev.type)
        {
            case ClientMessage:
            {
                if (ev.xclient.message_type == platform_xlib->wm_protocols &&
                    (Atom) ev.xclient.data.l[0] == platform_xlib->wm_delete_window)
                {
                    // running = false;
                }
            } break;

            case EnterNotify:
            {
                input->mouse.x = ev.xmotion.x;
                input->mouse.y = ev.xmotion.y;
                input->last_mouse.x = input->mouse.x;
                input->last_mouse.y = input->mouse.y;
            } break;

            case MotionNotify:
            {
                input->mouse.x = ev.xmotion.x;
                input->mouse.y = ev.xmotion.y;
            } break;

            case ButtonPress:
            case ButtonRelease:
            {
                bool is_down = (ev.type == ButtonPress);

                switch (ev.xbutton.button)
                {
                    case Button1:
                    {
                        input->mouse_left.change_count += (input->mouse_left.is_down != is_down) ? 1 : 0;
                        input->mouse_left.is_down = is_down;
                    } break;

                    case Button3:
                    {
                        input->mouse_right.change_count += (input->mouse_right.is_down != is_down) ? 1 : 0;
                        input->mouse_right.is_down = is_down;
                    } break;
                }
            } break;

            case KeyPress:
            case KeyRelease:
            {
                if (ev.type == KeyRelease && XEventsQueued(platform_xlib->display, QueuedAfterReading))
                {
                    XEvent next_event;
                    XPeekEvent(platform_xlib->display, &next_event);

                    if (next_event.type == KeyPress &&
                        next_event.xkey.time == ev.xkey.time &&
                        next_event.xkey.keycode == ev.xkey.keycode)
                    {
                        XNextEvent(platform_xlib->display, &next_event);
                        continue;
                    }
                }

                bool is_down = (ev.type == KeyPress);
                KeySym key = XLookupKeysym((XKeyEvent *) &ev, 0);

                switch (key)
                {
                    case XK_a:
                    {
                        input->left.change_count += (input->left.is_down != is_down) ? 1 : 0;
                        input->left.is_down = is_down;
                    } break;

                    case XK_d:
                    {
                        input->right.change_count += (input->right.is_down != is_down) ? 1 : 0;
                        input->right.is_down = is_down;
                    } break;

                    case XK_e:
                    {
                        input->up.change_count += (input->up.is_down != is_down) ? 1 : 0;
                        input->up.is_down = is_down;
                    } break;

                    case XK_s:
                    {
                        input->back.change_count += (input->back.is_down != is_down) ? 1 : 0;
                        input->back.is_down = is_down;
                    } break;

                    case XK_q:
                    {
                        input->down.change_count += (input->down.is_down != is_down) ? 1 : 0;
                        input->down.is_down = is_down;
                    } break;

                    case XK_w:
                    {
                        input->forward.change_count += (input->forward.is_down != is_down) ? 1 : 0;
                        input->forward.is_down = is_down;
                    } break;
                }
            } break;
        }
    }

    // TODO: delta time
    input->dt = 1.0f / (float) TARGET_FRAME_RATE;
}

#if GRAPHICS_API_OPENGL

static Vertex *
platform_xlib_opengl_begin_drawing(PlatformXlibState *platform_xlib)
{
    glXMakeCurrent(platform_xlib->display, platform_xlib->window, platform_xlib->context);

    return begin_opengl_drawing(&platform_xlib->opengl);
}

static void
platform_xlib_opengl_finish_drawing(PlatformXlibState *platform_xlib, DrawContext *ctx)
{
    finish_opengl_drawing(&platform_xlib->opengl, ctx);

    glXSwapBuffers(platform_xlib->display, platform_xlib->window);

    glXMakeCurrent(platform_xlib->display, platform_xlib->client_drawable, platform_xlib->client_context);
}

#endif
