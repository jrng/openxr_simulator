typedef struct
{
    HWND window;

    int32_t window_width;
    int32_t window_height;

    PlatformInput input;

    LARGE_INTEGER last_time;

#if GRAPHICS_API_OPENGL
    HDC device_context;
    HGLRC gl_context;

    HDC client_device_context;
    HGLRC client_gl_context;
#endif

    union
    {

#if GRAPHICS_API_D3D11
        GraphicsApiD3D11State d3d11;
#endif

#if GRAPHICS_API_OPENGL
        GraphicsApiOpenGlState opengl;
#endif

    };
} PlatformWin32State;
