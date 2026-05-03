typedef struct
{
    GLuint vertex_buffer;
    GLuint program;
    GLuint vertex_array;
    GLuint font_texture;
} GraphicsApiOpenGlState;

typedef struct
{
    GLuint textures[SWAPCHAIN_IMAGE_COUNT];
} OpenGlSwapchain;
