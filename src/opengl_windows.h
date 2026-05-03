#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_DEPTH_COMPONENT32              0x81A7
#define GL_ARRAY_BUFFER                   0x8892
#define GL_WRITE_ONLY                     0x88B9
#define GL_TEXTURE0                       0x84C0
#define GL_STREAM_DRAW                    0x88E0
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_SRGB8_ALPHA8                   0x8C43
#define GL_DEPTH_COMPONENT32F             0x8CAC

typedef char GLchar;
typedef signed long long int GLsizeiptr;
typedef signed long long int GLintptr;

typedef void (*PFNGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (*PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (*PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint *buffers);
typedef void (*PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (*PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void *(*PFNGLMAPBUFFERPROC) (GLenum target, GLenum access);
typedef GLboolean (*PFNGLUNMAPBUFFERPROC) (GLenum target);
typedef void (*PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (*PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (*PFNGLCREATEPROGRAMPROC) (void);
typedef void (*PFNGLDELETEPROGRAMPROC) (GLuint program);
typedef GLuint (*PFNGLCREATESHADERPROC) (GLenum type);
typedef void (*PFNGLDELETESHADERPROC) (GLuint shader);
typedef void (*PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (*PFNGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef GLint (*PFNGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (*PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (*PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (*PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (*PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLboolean (*PFNGLISPROGRAMPROC) (GLuint program);
typedef GLboolean (*PFNGLISSHADERPROC) (GLuint shader);
typedef void (*PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (*PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (*PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (*PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (*PFNGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint *arrays);
typedef void (*PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef void (*PFNGLTEXSTORAGE2DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (*PFNGLBINDVERTEXBUFFERPROC) (GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
