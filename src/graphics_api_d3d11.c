static inline bool
d3d11_is_depth_format(int64_t format)
{
    switch (format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        {
            return false;
        }

        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_D16_UNORM:
        {
            return true;
        }
    }

    return false;
}

static inline DXGI_FORMAT
d3d11_get_typeless_format(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        {
            return DXGI_FORMAT_R8G8B8A8_TYPELESS;
        }

        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        {
            return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        }
    }

    return format;
}

static inline DXGI_FORMAT
d3d11_get_non_srgb_format(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        {
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        {
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        }
    }

    return format;
}

static XRAPI_ATTR XrResult XRAPI_CALL
xrGetD3D11GraphicsRequirementsKHR_impl(XrInstance instance, XrSystemId system_id, XrGraphicsRequirementsD3D11KHR *graphics_requirements)
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

    state.instance.graphics_api = GraphicsApiD3D11;

    graphics_requirements->adapterLuid.LowPart = 0;
    graphics_requirements->adapterLuid.HighPart = 0;
    graphics_requirements->minFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    IDXGIFactory *dxgi_factory = NULL;

    if (SUCCEEDED(CreateDXGIFactory(&IID_IDXGIFactory, (void **) &dxgi_factory)))
    {
        IDXGIAdapter *dxgi_adapter;

        for (UINT i = 0; IDXGIFactory_EnumAdapters(dxgi_factory, i, &dxgi_adapter) != DXGI_ERROR_NOT_FOUND; i += 1)
        {
            DXGI_ADAPTER_DESC desc = { 0 };
            IDXGIAdapter_GetDesc(dxgi_adapter, &desc);

            char adapter_name[64];

            wcstombs(adapter_name, desc.Description, sizeof(adapter_name));

            msg("adapter = %s\n", adapter_name);

            graphics_requirements->adapterLuid = desc.AdapterLuid;

            break;
        }
    }

    TRACE_LEAVE_RESULT(XR_SUCCESS);
}

static void
initialize_graphics_api_d3d11(ID3D11Device *device)
{
}

static Texture
create_d3d11_texture(GraphicsApiD3D11State *d3d11, uint32_t width, uint32_t height, void *data)
{
    D3D11_TEXTURE2D_DESC texture_description;
    texture_description.Width              = width;
    texture_description.Height             = height;
    texture_description.MipLevels          = 1;
    texture_description.ArraySize          = 1;
    texture_description.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    texture_description.SampleDesc.Count   = 1;
    texture_description.SampleDesc.Quality = 0;
    texture_description.Usage              = D3D11_USAGE_DEFAULT;
    texture_description.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    texture_description.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;
    texture_description.MiscFlags          = 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC texture_view_description;
    texture_view_description.Format                    = texture_description.Format;
    texture_view_description.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    texture_view_description.Texture2D.MostDetailedMip = 0;
    texture_view_description.Texture2D.MipLevels       = 1;

    Texture texture;

    if (FAILED(ID3D11Device_CreateTexture2D(d3d11->device, &texture_description, NULL, &texture.d3d11.texture)))
    {
        msg("error: CreateTexture2D failed\n");
        texture.d3d11.texture = NULL;
        texture.d3d11.texture_view = NULL;
        return texture;
    }

    if (FAILED(ID3D11Device_CreateShaderResourceView(d3d11->device, (ID3D11Resource *) texture.d3d11.texture, &texture_view_description, &texture.d3d11.texture_view)))
    {
        msg("error: CreateShaderResourceView failed\n");
        texture.d3d11.texture = NULL;
        texture.d3d11.texture_view = NULL;
        return texture;
    }

    D3D11_BOX region = { 0, 0, 0, width, height, 1 };

    ID3D11DeviceContext_UpdateSubresource(d3d11->device_context, (ID3D11Resource *) texture.d3d11.texture, 0, &region, data, 4 * width, 0);

    return texture;
}

static void
destroy_d3d11_texture(GraphicsApiD3D11State *d3d11, Texture texture)
{
    (void) d3d11;

    ID3D11ShaderResourceView_Release(texture.d3d11.texture_view);
    ID3D11Texture2D_Release(texture.d3d11.texture);
}
