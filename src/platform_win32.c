static XRAPI_ATTR XrResult XRAPI_CALL
xrConvertWin32PerformanceCounterToTimeKHR_impl(XrInstance instance, const LARGE_INTEGER *performance_counter, XrTime *time)
{
    TRACE_ENTER();

    if (!performance_counter || !time)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    *time = (1000000000 * performance_counter->QuadPart) / freq.QuadPart;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrConvertTimeToWin32PerformanceCounterKHR_impl(XrInstance instance, XrTime time, LARGE_INTEGER *performance_counter)
{
    TRACE_ENTER();

    if (!performance_counter)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_VALIDATION_FAILURE);
    }

    if ((instance != INSTANCE_HANDLE) || !state.instance.active)
    {
        TRACE_LEAVE_RESULT(XR_ERROR_HANDLE_INVALID);
    }

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    performance_counter->QuadPart = (time * freq.QuadPart) / 1000000000;

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

LRESULT CALLBACK
window_callback(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    PlatformWin32State *platform_win32 = NULL;

    if (message == WM_NCCREATE)
    {
        LPCREATESTRUCT create_struct = (LPCREATESTRUCT) l_param;
        platform_win32 = (PlatformWin32State *) create_struct->lpCreateParams;
        SetWindowLongPtr(window_handle, GWLP_USERDATA, (LONG_PTR) platform_win32);
    }
    else
    {
        platform_win32 = (PlatformWin32State *) GetWindowLongPtr(window_handle, GWLP_USERDATA);
    }

    LRESULT result = FALSE;

    switch (message)
    {
        case WM_MOUSEMOVE:
        {
            platform_win32->mouse_x = (int16_t)( l_param        & 0xFFFF);
            platform_win32->mouse_y = (int16_t)((l_param >> 16) & 0xFFFF);
        } break;

        case WM_LBUTTONDOWN:
        {
            platform_win32->mouse_x = (int16_t)( l_param        & 0xFFFF);
            platform_win32->mouse_y = (int16_t)((l_param >> 16) & 0xFFFF);
            platform_win32->mouse_left_down = true;
        } break;

        case WM_LBUTTONUP:
        {
            platform_win32->mouse_x = (int16_t)( l_param        & 0xFFFF);
            platform_win32->mouse_y = (int16_t)((l_param >> 16) & 0xFFFF);
            platform_win32->mouse_left_down = false;
        } break;

        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            bool was_down = (l_param & (1 << 30)) ? true : false;
            bool is_down  = (l_param & (1 << 31)) ? false : true;

            if (is_down != was_down)
            {
                switch (w_param)
                {
                    case 'A':
                    {
                        platform_win32->left_down = is_down;
                    } break;

                    case 'D':
                    {
                        platform_win32->right_down = is_down;
                    } break;

                    case 'E':
                    {
                        platform_win32->up_down = is_down;
                    } break;

                    case 'S':
                    {
                        platform_win32->back_down = is_down;
                    } break;

                    case 'Q':
                    {
                        platform_win32->down_down = is_down;
                    } break;

                    case 'W':
                    {
                        platform_win32->forward_down = is_down;
                    } break;
                }
            }
        } break;

        default:
        {
            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;
    }

    return result;
}

#define WIN32_WINDOW_CLASS_NAME "openxr_simulator_window_class"

static HWND
create_window(int32_t width, int32_t height, DWORD window_style, PlatformWin32State *platform_win32)
{
    RECT window_rect = { 0, 0, width, height };
    AdjustWindowRect(&window_rect, window_style, false);

    WNDCLASS window_class = { sizeof(WNDCLASS) };
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = window_callback;
    window_class.hInstance = 0; // TODO: windows.instance;
    window_class.lpszClassName = WIN32_WINDOW_CLASS_NAME;
    window_class.hCursor = LoadCursor(0, IDC_ARROW);

    if (!RegisterClass(&window_class))
    {
        msg("win32 error: could not register window class\n");
        return NULL;
    }

    HWND window = CreateWindow(WIN32_WINDOW_CLASS_NAME, "OpenXR Viewer", window_style,
                               CW_USEDEFAULT, CW_USEDEFAULT, window_rect.right - window_rect.left,
                               window_rect.bottom - window_rect.top, 0, 0, 0 /* TODO: windows.instance */, platform_win32);

    if (!window)
    {
        msg("win32 error: could not create a window\n");
        return NULL;
    }

    return window;
}

static void
destroy_window(HWND window)
{
    DestroyWindow(window);

    if (!UnregisterClass(WIN32_WINDOW_CLASS_NAME, 0))
    {
        msg("win32 error: could not unregister window class\n");
    }
}

#undef WIN32_WINDOW_CLASS_NAME

#if GRAPHICS_API_OPENGL

#define WGL_DRAW_TO_WINDOW                  0x2001
#define WGL_SUPPORT_OPENGL                  0x2010
#define WGL_DOUBLE_BUFFER                   0x2011
#define WGL_PIXEL_TYPE                      0x2013
#define WGL_COLOR_BITS                      0x2014
#define WGL_TYPE_RGBA                       0x202B
#define WGL_CONTEXT_MAJOR_VERSION           0x2091
#define WGL_CONTEXT_MINOR_VERSION           0x2092
#define WGL_CONTEXT_PROFILE_MASK            0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT        0x00000001

typedef BOOL wglSwapIntervalEXT(int interval);
typedef const char *wglGetExtensionsStringARB(HDC hdc);
typedef BOOL wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList,
                                     UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
typedef HGLRC wglCreateContextAttribsARB(HDC hDC, HGLRC hshareContext, const int *attribList);

static void
initialize_platform_win32_opengl(PlatformWin32State *platform_win32, HDC client_device_context, HGLRC client_gl_context, int32_t window_width, int32_t window_height)
{
    platform_win32->client_device_context = client_device_context;
    platform_win32->client_gl_context = client_gl_context;

    platform_win32->window_width = window_width;
    platform_win32->window_height = window_height;

    DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    HWND dummy_window = CreateWindow("STATIC", "dummy_window", window_style,
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                     0, 0, 0, 0);

    if (!dummy_window)
    {
        return;
    }

    HDC device_context = GetDC(dummy_window);

    // TODO: is there a way to iterate the pixel formats
    PIXELFORMATDESCRIPTOR pixel_format = { 0 };
    pixel_format.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pixel_format.cColorBits = 24;

    int pixel_format_index = ChoosePixelFormat(device_context, &pixel_format);

    if (!pixel_format_index)
    {
        ReleaseDC(dummy_window, device_context);
        DestroyWindow(dummy_window);
        return;
    }

    DescribePixelFormat(device_context, pixel_format_index, sizeof(PIXELFORMATDESCRIPTOR), &pixel_format);
    SetPixelFormat(device_context, pixel_format_index, &pixel_format);

    HGLRC dummy_context = wglCreateContext(device_context);

    if (!dummy_context)
    {
        ReleaseDC(dummy_window, device_context);
        DestroyWindow(dummy_window);
        return;
    }

    if (!wglMakeCurrent(device_context, dummy_context))
    {
        wglDeleteContext(dummy_context);
        ReleaseDC(dummy_window, device_context);
        DestroyWindow(dummy_window);
        return;
    }

    wglGetExtensionsStringARB *wglGetExtensionsString = (wglGetExtensionsStringARB *) wglGetProcAddress("wglGetExtensionsStringARB");

    if (!wglGetExtensionsString)
    {
        wglMakeCurrent(0, 0);
        wglDeleteContext(dummy_context);
        ReleaseDC(dummy_window, device_context);
        DestroyWindow(dummy_window);
        return;
    }

    bool has_WGL_ARB_pixel_format = false;
    bool has_WGL_ARB_create_context = false;
    bool has_WGL_ARB_create_context_profile = false;
    bool has_WGL_EXT_swap_control = false;

    {
        char *wgl_extensions = (char *) wglGetExtensionsString(device_context);
        char *at = wgl_extensions;

        while (*at)
        {
            while (*at == ' ') at += 1;
            char *start = at;
            while (*at && (*at != ' ')) at += 1;

            String name;
            name.count = at - start;
            name.data  = start;

            if (strings_are_equal(name, S("WGL_ARB_pixel_format")))
            {
                has_WGL_ARB_pixel_format = true;
            }
            else if (strings_are_equal(name, S("WGL_ARB_create_context")))
            {
                has_WGL_ARB_create_context = true;
            }
            else if (strings_are_equal(name, S("WGL_ARB_create_context_profile")))
            {
                has_WGL_ARB_create_context_profile = true;
            }
            else if (strings_are_equal(name, S("WGL_EXT_swap_control")))
            {
                has_WGL_EXT_swap_control = true;
            }
        }
    }

    if (!has_WGL_ARB_pixel_format)
    {
        wglMakeCurrent(0, 0);
        wglDeleteContext(dummy_context);
        ReleaseDC(dummy_window, device_context);
        DestroyWindow(dummy_window);
        return;
    }

    if (!has_WGL_ARB_create_context)
    {
        wglMakeCurrent(0, 0);
        wglDeleteContext(dummy_context);
        ReleaseDC(dummy_window, device_context);
        DestroyWindow(dummy_window);
        return;
    }

    if (!has_WGL_ARB_create_context_profile)
    {
        wglMakeCurrent(0, 0);
        wglDeleteContext(dummy_context);
        ReleaseDC(dummy_window, device_context);
        DestroyWindow(dummy_window);
        return;
    }

    wglSwapIntervalEXT *wglSwapInterval = NULL;

    if (has_WGL_EXT_swap_control)
    {
        wglSwapInterval = (wglSwapIntervalEXT *) wglGetProcAddress("wglSwapIntervalEXT");
    }

    wglChoosePixelFormatARB *wglChoosePixelFormat = (wglChoosePixelFormatARB *) wglGetProcAddress("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB *wglCreateContextAttribs = (wglCreateContextAttribsARB *) wglGetProcAddress("wglCreateContextAttribsARB");

    wglMakeCurrent(0, 0);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummy_window, device_context);
    DestroyWindow(dummy_window);

    platform_win32->window = create_window(platform_win32->window_width, platform_win32->window_height, window_style, platform_win32);

    if (!platform_win32->window)
    {
        return;
    }

    device_context = GetDC(platform_win32->window);

    int attributes[] = {
        WGL_DRAW_TO_WINDOW, GL_TRUE,
        WGL_SUPPORT_OPENGL, GL_TRUE,
        WGL_DOUBLE_BUFFER,  GL_TRUE,
        WGL_PIXEL_TYPE,     WGL_TYPE_RGBA,
        WGL_COLOR_BITS,     24,
        0
    };

    UINT format_count;

    if (!wglChoosePixelFormat(device_context, attributes, 0, 1, &pixel_format_index, &format_count) || (format_count == 0))
    {
        destroy_window(platform_win32->window);
        return;
    }

    DescribePixelFormat(device_context, pixel_format_index, sizeof(PIXELFORMATDESCRIPTOR), &pixel_format);
    SetPixelFormat(device_context, pixel_format_index, &pixel_format);

    int context_attributes[] = {
        WGL_CONTEXT_MAJOR_VERSION, 4,
        WGL_CONTEXT_MINOR_VERSION, 3,
        WGL_CONTEXT_PROFILE_MASK, WGL_CONTEXT_CORE_PROFILE_BIT,
        0
    };

    platform_win32->gl_context = wglCreateContextAttribs(device_context, platform_win32->client_gl_context, context_attributes);

    if (!platform_win32->gl_context)
    {
        msg("opengl error: could not create context\n");
        ReleaseDC(platform_win32->window, device_context);
        destroy_window(platform_win32->window);
        return;
    }

    if (!wglMakeCurrent(device_context, platform_win32->gl_context))
    {
        msg("opengl error: could not make context current\n");
        wglDeleteContext(platform_win32->gl_context);
        ReleaseDC(platform_win32->window, device_context);
        destroy_window(platform_win32->window);
        return;
    }

    if (wglSwapInterval)
    {
        wglSwapInterval(1);
    }

#define load_opengl_function(prototype, name)                          \
    do {                                                               \
        name = (prototype) wglGetProcAddress(#name);                   \
        if (!name)                                                     \
        {                                                              \
            msg("error: could not load opengl function '" #name "'\n");\
            wglMakeCurrent(0, 0);                                      \
            wglDeleteContext(platform_win32->gl_context);              \
            ReleaseDC(platform_win32->window, device_context);         \
            destroy_window(platform_win32->window);                    \
            return;                                                    \
        }                                                              \
    } while (0)

#define OPENGL_FUNCTION(type, name) load_opengl_function(type, name)

#include "opengl_functions.h"

#undef load_opengl_function

    initialize_opengl(&platform_win32->opengl);

    ReleaseDC(platform_win32->window, device_context);
    ShowWindow(platform_win32->window, SW_SHOW);

    wglMakeCurrent(platform_win32->client_device_context, platform_win32->client_gl_context);

    QueryPerformanceCounter(&platform_win32->last_time);
}

#endif

#if GRAPHICS_API_D3D11

static void
initialize_platform_win32_d3d11(PlatformWin32State *platform_win32, ID3D11Device *d3d11_device, int32_t window_width, int32_t window_height)
{
    platform_win32->window_width = window_width;
    platform_win32->window_height = window_height;

    platform_win32->d3d11.device = d3d11_device;

    ID3D11Device_GetImmediateContext(platform_win32->d3d11.device, &platform_win32->d3d11.device_context);

    DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    platform_win32->window = create_window(platform_win32->window_width, platform_win32->window_height, window_style, platform_win32);

    if (!platform_win32->window)
    {
        ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
        return;
    }

    HRESULT result;

    IDXGIDevice *dxgi_device;

    if (FAILED(ID3D11Device_QueryInterface(platform_win32->d3d11.device, &IID_IDXGIDevice, (void **) &dxgi_device)))
    {
        destroy_window(platform_win32->window);
        ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
        return;
    }

    IDXGIAdapter *dxgi_adapter;

    result = IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter);

    IDXGIDevice_Release(dxgi_device);

    if (FAILED(result))
    {
        destroy_window(platform_win32->window);
        ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
        return;
    }

    IDXGIFactory2 *dxgi_factory;

    result = IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, (void **) &dxgi_factory);

    IDXGIAdapter_Release(dxgi_adapter);

    if (FAILED(result))
    {
        destroy_window(platform_win32->window);
        ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 swapchain_description;
    swapchain_description.Width              = 0;
    swapchain_description.Height             = 0;
    swapchain_description.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchain_description.Stereo             = FALSE;
    swapchain_description.SampleDesc.Count   = 1;
    swapchain_description.SampleDesc.Quality = 0;
    swapchain_description.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_description.BufferCount        = 2;
    swapchain_description.Scaling            = DXGI_SCALING_NONE;
    swapchain_description.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_description.AlphaMode          = DXGI_ALPHA_MODE_IGNORE;
    swapchain_description.Flags              = 0;

    result = IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *) platform_win32->d3d11.device, platform_win32->window, &swapchain_description, NULL, NULL, &platform_win32->d3d11.swapchain);

    IDXGIFactory2_Release(dxgi_factory);

    if (FAILED(result))
    {
        msg("d3d11 error: could not create swapchain for window\n");
        destroy_window(platform_win32->window);
        ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
        return;
    }

    ID3D11Texture2D *framebuffer;

    if (FAILED(IDXGISwapChain_GetBuffer(platform_win32->d3d11.swapchain, 0, &IID_ID3D11Texture2D, (void **) &framebuffer)))
    {
        msg("d3d11 error: could not get swapchain framebuffer\n");
        destroy_window(platform_win32->window);
        ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
        return;
    }

    D3D11_TEXTURE2D_DESC framebuffer_description = { 0 };
    ID3D11Texture2D_GetDesc(framebuffer, &framebuffer_description);

    result = ID3D11Device_CreateRenderTargetView(platform_win32->d3d11.device, (ID3D11Resource *) framebuffer, NULL, &platform_win32->d3d11.framebuffer_view);

    ID3D11Texture2D_Release(framebuffer);

    if (FAILED(result))
    {
        msg("d3d11 error: could not create render target view for swapchain framebuffers\n");
        destroy_window(platform_win32->window);
        ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
        return;
    }

    ID3DBlob *vertex_binary;
    ID3DBlob *pixel_binary;

    String shader_source = S(
        "struct VertexInput\n"
        "{\n"
        "    float2 position : POS;\n"
        "    float2 uv       : TEX;\n"
        "    float4 color    : COLOR;\n"
        "};\n"
        "\n"
        "struct VertexOutput\n"
        "{\n"
        "    float4 position : SV_POSITION;\n"
        "    float2 uv       : TEX;\n"
        "    float4 color    : COLOR;\n"
        "};\n"
        "\n"
        "VertexOutput vertex_main(VertexInput input)\n"
        "{\n"
        "    VertexOutput output;\n"
        "\n"
        "    output.position = float4(input.position.x, input.position.y, 0.0f, 1.0f);\n"
        "    output.uv       = input.uv;\n"
        "    output.color    = input.color;\n"
        "\n"
        "    return output;\n"
        "}\n"
        "\n"
        "Texture2D u_texture : register(t0);\n"
        "SamplerState u_sampler : register(s0);\n"
        "\n"
        "float4 pixel_main(VertexOutput input) : SV_TARGET\n"
        "{\n"
        "    return input.color * u_texture.Sample(u_sampler, input.uv);\n"
        "}\n"
    );

    ID3DBlob *errors;

    if (FAILED(D3DCompile(shader_source.data, shader_source.count, NULL, NULL, NULL, "vertex_main", "vs_5_0", 0, 0, &vertex_binary, &errors)))
    {
        String error_string;
        error_string.count = ID3D10Blob_GetBufferSize(errors);
        error_string.data  = ID3D10Blob_GetBufferPointer(errors);
        __debugbreak();
        // TODO:
        return;
    }

    if (FAILED(D3DCompile(shader_source.data, shader_source.count, NULL, NULL, NULL, "pixel_main", "ps_5_0", 0, 0, &pixel_binary, &errors)))
    {
        String error_string;
        error_string.count = ID3D10Blob_GetBufferSize(errors);
        error_string.data  = ID3D10Blob_GetBufferPointer(errors);
        __debugbreak();
        // TODO:
        return;
    }

    if (FAILED(ID3D11Device_CreateVertexShader(platform_win32->d3d11.device, ID3D10Blob_GetBufferPointer(vertex_binary),
                                               ID3D10Blob_GetBufferSize(vertex_binary), NULL, &platform_win32->d3d11.vertex_shader)))
    {
        msg("d3d11 error: could not create vertex shader\n");
        ID3D10Blob_Release(vertex_binary);
        // TODO:
        return;
    }

    result = ID3D11Device_CreatePixelShader(platform_win32->d3d11.device, ID3D10Blob_GetBufferPointer(pixel_binary),
                                            ID3D10Blob_GetBufferSize(pixel_binary), NULL, &platform_win32->d3d11.pixel_shader);

    ID3D10Blob_Release(pixel_binary);

    if (FAILED(result))
    {
        msg("d3d11 error: could not create pixel shader\n");
        ID3D10Blob_Release(vertex_binary);
        // TODO:
        return;
    }

    const D3D11_INPUT_ELEMENT_DESC vertex_layout_description[] = {
        { "POS"  , 0, DXGI_FORMAT_R32G32_FLOAT  , 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEX"  , 0, DXGI_FORMAT_R32G32_FLOAT  , 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    result = ID3D11Device_CreateInputLayout(platform_win32->d3d11.device, vertex_layout_description, ArrayCount(vertex_layout_description),
                                            ID3D10Blob_GetBufferPointer(vertex_binary), ID3D10Blob_GetBufferSize(vertex_binary),
                                            &platform_win32->d3d11.vertex_layout);

    ID3D10Blob_Release(vertex_binary);

    if (FAILED(result))
    {
        // TODO:
        return;
    }

    D3D11_BUFFER_DESC vertex_buffer_description;
    vertex_buffer_description.ByteWidth           = MAX_VERTEX_COUNT * sizeof(Vertex);
    vertex_buffer_description.Usage               = D3D11_USAGE_DYNAMIC;
    vertex_buffer_description.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
    vertex_buffer_description.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    vertex_buffer_description.MiscFlags           = 0;
    vertex_buffer_description.StructureByteStride = sizeof(Vertex);

    if (FAILED(ID3D11Device_CreateBuffer(platform_win32->d3d11.device, &vertex_buffer_description, NULL, &platform_win32->d3d11.vertex_buffer)))
    {
        msg("d3d11 error: could not create vertex buffer\n");
        // TODO: release framebuffer_view
        destroy_window(platform_win32->window);
        ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
        return;
    }

    D3D11_SAMPLER_DESC sampler_description;
    // TODO: choose filtering
    sampler_description.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler_description.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_description.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_description.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_description.MipLODBias     = 0.0f;
    sampler_description.MaxAnisotropy  = 0;
    sampler_description.ComparisonFunc = 0;
    sampler_description.BorderColor[0] = 0.0f;
    sampler_description.BorderColor[1] = 0.0f;
    sampler_description.BorderColor[2] = 0.0f;
    sampler_description.BorderColor[3] = 0.0f;
    sampler_description.MinLOD         = 0.0f;
    sampler_description.MaxLOD         = 0.0f;

    if (FAILED(ID3D11Device_CreateSamplerState(platform_win32->d3d11.device, &sampler_description, &platform_win32->d3d11.sampler_state)))
    {
        msg("d3d11 error: could not create sampler state\n");
        // TODO: release vertex buffer
        // TODO: release framebuffer_view
        destroy_window(platform_win32->window);
        ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
        return;
    }

    D3D11_RASTERIZER_DESC rasterizer_description;
    rasterizer_description.FillMode               = D3D11_FILL_SOLID;
    rasterizer_description.CullMode               = D3D11_CULL_FRONT;
    rasterizer_description.FrontCounterClockwise  = TRUE;
    rasterizer_description.DepthBias              = 0;
    rasterizer_description.DepthBiasClamp         = 0.0f;
    rasterizer_description.SlopeScaledDepthBias   = 0.0f;
    rasterizer_description.DepthClipEnable        = TRUE;
    rasterizer_description.ScissorEnable          = FALSE;
    rasterizer_description.MultisampleEnable      = FALSE;
    rasterizer_description.AntialiasedLineEnable  = FALSE;

    if (FAILED(ID3D11Device_CreateRasterizerState(platform_win32->d3d11.device, &rasterizer_description, &platform_win32->d3d11.rasterizer_state)))
    {
        msg("d3d11 error: could not create rasterizer state\n");
        // TODO:
        return;
    }

    ShowWindow(platform_win32->window, SW_SHOW);

    QueryPerformanceCounter(&platform_win32->last_time);
}

static void
deinitialize_platform_win32_d3d11(PlatformWin32State *platform_win32)
{
    destroy_window(platform_win32->window);
    IDXGISwapChain1_Release(platform_win32->d3d11.swapchain);
    ID3D11DeviceContext_Release(platform_win32->d3d11.device_context);
}

#endif

static void
platform_win32_wait_frame(PlatformWin32State *platform_win32, Session *session)
{
    LARGE_INTEGER time;
    int64_t dt_us;

    for (;;)
    {
        QueryPerformanceCounter(&time);
        dt_us = (1000000 * (time.QuadPart - platform_win32->last_time.QuadPart)) /
                win32_performance_frequency.QuadPart;

        if (dt_us >= (1000000 / TARGET_FRAME_RATE))
        {
            break;
        }
    }

    float dt = (float) (time.QuadPart - platform_win32->last_time.QuadPart) /
               (float) win32_performance_frequency.QuadPart;

    platform_win32->last_time = time;

    MSG message;

    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    int32_t mouse_dx = platform_win32->mouse_x - platform_win32->last_mouse_x;
    int32_t mouse_dy = platform_win32->mouse_y - platform_win32->last_mouse_y;

    platform_win32->last_mouse_x = platform_win32->mouse_x;
    platform_win32->last_mouse_y = platform_win32->mouse_y;

    XrQuaternionf orientation = quaternion_from_orbit_and_pitch(session->head_orbit, session->head_pitch);

    if (platform_win32->mouse_left_down)
    {
        session->head_orbit -= 0.0032f * mouse_dx;
        session->head_pitch -= 0.0032f * mouse_dy;
    }

    XrVector3f direction = { 0.0f, 0.0f, 0.0f };
    XrVector3f forward = quaternion_apply(orientation, (XrVector3f) { 0.0f, 0.0f, -1.0f });
    XrVector3f right   = quaternion_apply(orientation, (XrVector3f) { 1.0f, 0.0f, 0.0f });

    if (platform_win32->left_down)
    {
        direction = vec3_add(direction, vec3_scale(-1.0f, right));
    }

    if (platform_win32->right_down)
    {
        direction = vec3_add(direction, right);
    }

    if (platform_win32->forward_down)
    {
        direction = vec3_add(direction, forward);
    }

    if (platform_win32->back_down)
    {
        direction = vec3_add(direction, vec3_scale(-1.0f, forward));
    }

    if (platform_win32->up_down)
    {
        direction = vec3_add(direction, (XrVector3f) { 0.0f, 1.0f, 0.0f });
    }

    if (platform_win32->down_down)
    {
        direction = vec3_add(direction, (XrVector3f) { 0.0f, -1.0f, 0.0f });
    }

    session->head_position = vec3_add(session->head_position, vec3_scale(dt, direction));
}

#if GRAPHICS_API_D3D11

static Vertex *
platform_win32_d3d11_begin_drawing(PlatformWin32State *platform_win32)
{
    D3D11_MAPPED_SUBRESOURCE mapping;

    ID3D11DeviceContext_Map(platform_win32->d3d11.device_context, (ID3D11Resource *) platform_win32->d3d11.vertex_buffer,
                            0, D3D11_MAP_WRITE_DISCARD, 0, &mapping);

    return (Vertex *) mapping.pData;
}

static void
platform_win32_d3d11_finish_drawing(PlatformWin32State *platform_win32, DrawContext *ctx)
{
    ID3D11DeviceContext_Unmap(platform_win32->d3d11.device_context, (ID3D11Resource *) platform_win32->d3d11.vertex_buffer, 0);

    float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    ID3D11DeviceContext_ClearRenderTargetView(platform_win32->d3d11.device_context, platform_win32->d3d11.framebuffer_view, color);

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width    = (float) platform_win32->window_width;
    viewport.Height   = (float) platform_win32->window_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 0.0f;

    ID3D11DeviceContext_RSSetViewports(platform_win32->d3d11.device_context, 1, &viewport);
    ID3D11DeviceContext_OMSetRenderTargets(platform_win32->d3d11.device_context, 1, &platform_win32->d3d11.framebuffer_view, NULL);

    ID3D11DeviceContext_IASetPrimitiveTopology(platform_win32->d3d11.device_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_IASetInputLayout(platform_win32->d3d11.device_context, platform_win32->d3d11.vertex_layout);

    ID3D11DeviceContext_VSSetShader(platform_win32->d3d11.device_context, platform_win32->d3d11.vertex_shader, NULL, 0);
    ID3D11DeviceContext_PSSetShader(platform_win32->d3d11.device_context, platform_win32->d3d11.pixel_shader, NULL, 0);

    // TODO: restore the state after we are done
    ID3D11DeviceContext_RSSetState(platform_win32->d3d11.device_context, platform_win32->d3d11.rasterizer_state);

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;

    ID3D11DeviceContext_IASetVertexBuffers(platform_win32->d3d11.device_context, 0, 1, &platform_win32->d3d11.vertex_buffer, &stride, &offset);
    ID3D11DeviceContext_PSSetSamplers(platform_win32->d3d11.device_context, 0, 1, &platform_win32->d3d11.sampler_state);

    for (uint32_t i = 0; i < ctx->command_count; i += 1)
    {
        DrawCommand *command = ctx->commands + i;

        ID3D11DeviceContext_PSSetShaderResources(platform_win32->d3d11.device_context, 0, 1, &command->texture.d3d11.texture_view);
        ID3D11DeviceContext_Draw(platform_win32->d3d11.device_context, command->vertex_count, command->vertex_offset);
    }

    ID3D11ShaderResourceView *null_view = NULL;
    ID3D11DeviceContext_PSSetShaderResources(platform_win32->d3d11.device_context, 0, 1, &null_view);

    IDXGISwapChain1_Present(platform_win32->d3d11.swapchain, 1, 0);
}

#endif

#if GRAPHICS_API_OPENGL

static Vertex *
platform_win32_opengl_begin_drawing(PlatformWin32State *platform_win32)
{
    platform_win32->device_context = GetDC(platform_win32->window);

    wglMakeCurrent(platform_win32->device_context, platform_win32->gl_context);

    return begin_opengl_drawing(&platform_win32->opengl);
}

static void
platform_win32_opengl_finish_drawing(PlatformWin32State *platform_win32, DrawContext *ctx)
{
    finish_opengl_drawing(&platform_win32->opengl, ctx);

    SwapBuffers(platform_win32->device_context);

    ReleaseDC(platform_win32->window, platform_win32->device_context);
    platform_win32->device_context = NULL;

    wglMakeCurrent(platform_win32->client_device_context, platform_win32->client_gl_context);
}

#endif
