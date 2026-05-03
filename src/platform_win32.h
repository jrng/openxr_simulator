typedef struct
{
    HWND window;

    int32_t window_width;
    int32_t window_height;

    bool mouse_left_down;

    bool left_down;
    bool right_down;
    bool forward_down;
    bool back_down;
    bool up_down;
    bool down_down;

    int32_t mouse_x, mouse_y;
    int32_t last_mouse_x, last_mouse_y;

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
