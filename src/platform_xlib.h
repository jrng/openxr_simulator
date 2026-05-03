typedef struct
{
    Display *display;

    Window window;
    GLXContext context;

    GLXDrawable client_drawable;
    GLXContext client_context;

    Atom wm_protocols;
    Atom wm_delete_window;

    bool mouse_left_down;

    bool left_down;
    bool right_down;
    bool forward_down;
    bool back_down;
    bool up_down;
    bool down_down;

    int32_t mouse_x, mouse_y;
    int32_t last_mouse_x, last_mouse_y;

    union
    {

#if GRAPHICS_API_OPENGL
        GraphicsApiOpenGlState opengl;
#endif

    };
} PlatformXlibState;
