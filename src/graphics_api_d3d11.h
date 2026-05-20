typedef struct
{
    ID3D11Device *device;
    ID3D11DeviceContext *device_context;

    IDXGISwapChain1 *swapchain;
    ID3D11RenderTargetView *framebuffer_view;

    ID3D11InputLayout *vertex_layout;
    ID3D11RasterizerState *rasterizer_state;
    ID3D11SamplerState *sampler_state;
    ID3D11BlendState *blend_state;

    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;

    ID3D11Buffer *vertex_buffer;
} GraphicsApiD3D11State;

typedef struct
{
    ID3D11Texture2D *textures[SWAPCHAIN_IMAGE_COUNT];
    ID3D11ShaderResourceView *texture_views[SWAPCHAIN_IMAGE_COUNT];
} D3D11Swapchain;
