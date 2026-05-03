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

    XStoreName(platform_xlib->display, platform_xlib->window, "OpenXR Viewer");

    platform_xlib->wm_protocols = XInternAtom(platform_xlib->display, "WM_PROTOCOLS", 0);
    platform_xlib->wm_delete_window = XInternAtom(platform_xlib->display, "WM_DELETE_WINDOW", 0);

    XSetWMProtocols(platform_xlib->display, platform_xlib->window, &platform_xlib->wm_delete_window, 1);

    XSizeHints sh = {};
    sh.flags = PMinSize | PMaxSize | PWinGravity;
    sh.win_gravity = StaticGravity;
    sh.min_width  = sh.max_width  = window_width;
    sh.min_height = sh.max_height = window_height;

    XSetWMNormalHints(platform_xlib->display, platform_xlib->window, &sh);

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
platform_xlib_wait_frame(PlatformXlibState *platform_xlib, Session *session)
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
                platform_xlib->mouse_x = ev.xmotion.x;
                platform_xlib->mouse_y = ev.xmotion.y;
                platform_xlib->last_mouse_x = platform_xlib->mouse_x;
                platform_xlib->last_mouse_y = platform_xlib->mouse_y;
            } break;

            case MotionNotify:
            {
                platform_xlib->mouse_x = ev.xmotion.x;
                platform_xlib->mouse_y = ev.xmotion.y;
            } break;

            case ButtonPress:
            case ButtonRelease:
            {
                bool is_down = (ev.type == ButtonPress);

                switch (ev.xbutton.button)
                {
                    case Button1:
                    {
                        platform_xlib->mouse_left_down = is_down;
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
                        platform_xlib->left_down = is_down;
                    } break;

                    case XK_d:
                    {
                        platform_xlib->right_down = is_down;
                    } break;

                    case XK_e:
                    {
                        platform_xlib->up_down = is_down;
                    } break;

                    case XK_s:
                    {
                        platform_xlib->back_down = is_down;
                    } break;

                    case XK_q:
                    {
                        platform_xlib->down_down = is_down;
                    } break;

                    case XK_w:
                    {
                        platform_xlib->forward_down = is_down;
                    } break;
                }
            } break;
        }
    }

    int32_t mouse_dx = platform_xlib->mouse_x - platform_xlib->last_mouse_x;
    int32_t mouse_dy = platform_xlib->mouse_y - platform_xlib->last_mouse_y;

    platform_xlib->last_mouse_x = platform_xlib->mouse_x;
    platform_xlib->last_mouse_y = platform_xlib->mouse_y;

    XrQuaternionf orientation = quaternion_from_orbit_and_pitch(state.session.head_orbit, state.session.head_pitch);

    if (platform_xlib->mouse_left_down)
    {
        session->head_orbit -= 0.0032f * mouse_dx;
        session->head_pitch -= 0.0032f * mouse_dy;
    }

    XrVector3f direction = { 0.0f, 0.0f, 0.0f };
    XrVector3f forward = quaternion_apply(orientation, (XrVector3f) { 0.0f, 0.0f, -1.0f });
    XrVector3f right   = quaternion_apply(orientation, (XrVector3f) { 1.0f, 0.0f, 0.0f });

    if (platform_xlib->left_down)
    {
        direction = vec3_add(direction, vec3_scale(-1.0f, right));
    }

    if (platform_xlib->right_down)
    {
        direction = vec3_add(direction, right);
    }

    if (platform_xlib->forward_down)
    {
        direction = vec3_add(direction, forward);
    }

    if (platform_xlib->back_down)
    {
        direction = vec3_add(direction, vec3_scale(-1.0f, forward));
    }

    if (platform_xlib->up_down)
    {
        direction = vec3_add(direction, (XrVector3f) { 0.0f, 1.0f, 0.0f });
    }

    if (platform_xlib->down_down)
    {
        direction = vec3_add(direction, (XrVector3f) { 0.0f, -1.0f, 0.0f });
    }

    // TODO: delta time
    session->head_position = vec3_add(session->head_position, vec3_scale(1.0f / 60.0f, direction));
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
