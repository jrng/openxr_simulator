typedef struct
{
    Display *display;

    Window window;
    GLXContext context;

    GLXDrawable client_drawable;
    GLXContext client_context;

    Atom wm_protocols;
    Atom wm_delete_window;

    union
    {

#if GRAPHICS_API_OPENGL
        GraphicsApiOpenGlState opengl;
#endif

    };
} PlatformXlibState;
