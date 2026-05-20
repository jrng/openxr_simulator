#if defined(_WIN32)
#  include "opengl_windows.h"
#endif

static inline const char *
opengl_format_to_string(int64_t format)
{
    const char *result = "UNKNOWN_OPENGL_FORMAT";

#define NAME(fmt) case fmt: result = #fmt; break

    switch (format)
    {
        NAME(GL_SRGB8_ALPHA8);
        NAME(GL_RGBA8);
        NAME(GL_DEPTH_COMPONENT32F);
        NAME(GL_DEPTH_COMPONENT32);
        NAME(GL_DEPTH_COMPONENT24);
    }

#undef NAME

    return result;
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetOpenGLGraphicsRequirementsKHR_impl(XrInstance instance, XrSystemId system_id, XrGraphicsRequirementsOpenGLKHR *graphics_requirements)
{
    TRACE_ENTER();

    if (!graphics_requirements)
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

    state.instance.graphics_api = GraphicsApiOpenGl;

    graphics_requirements->minApiVersionSupported = XR_MAKE_VERSION(3, 0, 0);
    graphics_requirements->maxApiVersionSupported = XR_MAKE_VERSION(4, 6, 0);

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

#define OPENGL_FUNCTION(type, name) static type name

#include "opengl_functions.h"

static void
check_opengl_error(GLuint id)
{
    GLint length = 0;

    if (glIsShader(id))
    {
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    }
    else if (glIsProgram(id))
    {
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
    }
    else
    {
        return;
    }

    if (length > 0)
    {
        char log[1024];

        if (glIsShader(id))
        {
            glGetShaderInfoLog(id, length, NULL, log);
            msg("shader error: %s\n", log);
        }
        else if (glIsProgram(id))
        {
            glGetProgramInfoLog(id, length, NULL, log);
            msg("program error: %s\n", log);
        }
    }
}

static void
initialize_opengl(GraphicsApiOpenGlState *opengl)
{
    glGenVertexArrays(1, &opengl->vertex_array);
    glBindVertexArray(opengl->vertex_array);

    glGenBuffers(1, &opengl->vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, opengl->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_COUNT * sizeof(Vertex), NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void *) (intptr_t) 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void *) (intptr_t) (2 * sizeof(float)));
    glVertexAttribPointer(2, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (const void *) (intptr_t) (4 * sizeof(float)));

    const GLchar *vertex_shader_source[] = {
        "#version 460\n",
        "layout(location = 0) in vec2 a_position;\n"
        "layout(location = 1) in vec2 a_uv;\n"
        "layout(location = 2) in vec4 a_color;\n"
        "out vec2 v_uv;\n"
        "out vec4 v_color;\n"
        "void main()\n"
        "{\n"
        "    v_uv = a_uv;\n"
        "    v_color = a_color;\n"
        "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "}\n",
    };

    const GLchar *fragment_shader_source[] = {
        "#version 460\n",
        "layout(binding = 0) uniform sampler2D u_texture;\n"
        "in vec2 v_uv;\n"
        "in vec4 v_color;\n"
        "out vec4 output_color;\n"
        "vec3 srgb_to_linear(vec3 color)\n"
        "{\n"
        "    vec3 a = vec3(1.0 / 12.92) * color;\n"
        "    vec3 b = pow(vec3(1.0 / 1.055) * (color + vec3(0.055)), vec3(2.4));\n"
        "    vec3 t = step(vec3(0.04045), color);\n"
        "    return mix(a, b, t);\n"
        "}\n"
        "vec3 linear_to_srgb(vec3 color)\n"
        "{\n"
        "    vec3 a = vec3(12.92) * color;\n"
        "    vec3 b = vec3(1.055) * pow(color, vec3(1.0 / 2.4)) - vec3(0.055);\n"
        "    vec3 t = step(vec3(0.0031308), color);\n"
        "    return mix(a, b, t);\n"
        "}\n"
        "void main()\n"
        "{\n"
        "    vec4 color = v_color;\n"
        "    color.rgb = srgb_to_linear(color.rgb);\n"
        "    vec4 final = color * texture(u_texture, v_uv);\n"
        "    final.rgb = linear_to_srgb(final.rgb);\n"
        "    output_color = final;\n"
        "}\n",
    };

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vertex_shader, ArrayCount(vertex_shader_source), vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    check_opengl_error(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(fragment_shader, ArrayCount(fragment_shader_source), fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    check_opengl_error(fragment_shader);

    opengl->program = glCreateProgram();
    glAttachShader(opengl->program, vertex_shader);
    glAttachShader(opengl->program, fragment_shader);
    glLinkProgram(opengl->program);
    check_opengl_error(opengl->program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
}

static void
deinitialize_opengl(GraphicsApiOpenGlState *opengl)
{
    glDeleteProgram(opengl->program);
    glDeleteVertexArrays(1, &opengl->vertex_array);
    glDeleteBuffers(1, &opengl->vertex_buffer);
}

static Texture
create_opengl_texture(GraphicsApiOpenGlState *opengl, uint32_t width, uint32_t height, void *data)
{
    (void) opengl;

    Texture texture;

    glGenTextures(1, &texture.opengl);

    glBindTexture(GL_TEXTURE_2D, texture.opengl);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // TODO: this was only introduced with opengl 4.2
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

    return texture;
}

static void
destroy_opengl_texture(GraphicsApiOpenGlState *opengl, Texture texture)
{
    (void) opengl;

    glDeleteTextures(1, &texture.opengl);
}

static Vertex *
begin_opengl_drawing(GraphicsApiOpenGlState *opengl)
{
    glBindBuffer(GL_ARRAY_BUFFER, opengl->vertex_buffer);
    return (Vertex *) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
}

static void
finish_opengl_drawing(GraphicsApiOpenGlState *opengl, DrawContext *ctx)
{
    glUnmapBuffer(GL_ARRAY_BUFFER);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(opengl->program);
    glBindVertexArray(opengl->vertex_array);

    glActiveTexture(GL_TEXTURE0);

    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    for (uint32_t i = 0; i < ctx->command_count; i += 1)
    {
        DrawCommand *command = ctx->commands + i;

        glBindTexture(GL_TEXTURE_2D, command->texture.opengl);
        glDrawArrays(GL_TRIANGLES, command->vertex_offset, command->vertex_count);
    }
}
