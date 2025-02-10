#include "Game.hpp"
#include "Utility.hpp"

void Game::init_environment_objects() {

    environment_objects.emplace_back();
    environment_objects.back().init(
        m_device, texture_loader, LR"(resources/house.png)", LR"(resources/house.wobj)",
        const_heaps.get_cpu_handle(heap_ids::house_tex),
        const_heaps.get_gpu_handle(heap_ids::house_tex), object_id_giver);

    obj_id_to_transform[object_id_giver.get_id("house.off")] =
        DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(1.0f, 0.0f, 5.0f));

    environment_objects.emplace_back();
    environment_objects.back().init(
        m_device, texture_loader, LR"(resources/stone.png)", LR"(resources/stone.wobj)",
        const_heaps.get_cpu_handle(heap_ids::stone_tex),
        const_heaps.get_gpu_handle(heap_ids::stone_tex), object_id_giver);

    obj_id_to_transform[object_id_giver.get_id("stone.off")] =
        DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(-2.0f, 0.0f, -3.0f));

    environment_objects.emplace_back();
    environment_objects.back().init(
        m_device, texture_loader, LR"(resources/ground.png)", LR"(resources/ground.wobj)",
        const_heaps.get_cpu_handle(heap_ids::ground_tex),
        const_heaps.get_gpu_handle(heap_ids::ground_tex), object_id_giver);

    obj_id_to_transform[object_id_giver.get_id("ground.off")] =
        DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity());

    environment_objects.emplace_back();
    environment_objects.back().init(
        m_device, texture_loader, LR"(resources/tree.png)", LR"(resources/tree.wobj)",
        const_heaps.get_cpu_handle(heap_ids::tree_tex),
        const_heaps.get_gpu_handle(heap_ids::tree_tex), object_id_giver);

    obj_id_to_transform[object_id_giver.get_id("tree.off")] =
        DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(-4.0f, 0.0f, 3.0f));
}

double Game::get_delta_time() {
    std::chrono::high_resolution_clock::time_point now_point =
        std::chrono::high_resolution_clock::now();
    double microseconds_passed =
        std::chrono::duration_cast<std::chrono::microseconds>(now_point - prev_time_point).count();
    prev_time_point = now_point;
    return microseconds_passed / 1'000'000;
}

double Game::get_time() {
    std::chrono::high_resolution_clock::time_point now_point =
        std::chrono::high_resolution_clock::now();
    double microseconds_passed =
        std::chrono::duration_cast<std::chrono::microseconds>(now_point - start_point).count();
    return microseconds_passed / 1'000'000;
}

void Game::recalculate_matrix(double angle) {

    DirectX::XMMATRIX world, proj;

    world = DirectX::XMMatrixMultiply(
        DirectX::XMMatrixRotationY(2.5f * angle),
        DirectX::XMMatrixRotationX(static_cast<FLOAT>(sin(angle)) / 2.0f));

    proj = DirectX::XMMatrixPerspectiveFovLH(45.0f, width / height, 0.1f, 100.0f);


    world = XMMatrixTranspose(world);
    proj = XMMatrixTranspose(proj);

    DirectX::XMMATRIX alternative = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    alternative = XMMatrixTranspose(alternative);

    Shader_const_buffer buff;

    for (unsigned int i = 0; i < 10; i++) {
        XMStoreFloat4x4(&buff.matWorld[i], alternative);
    }

    //object_id_giver.write();
    for (const auto& [obj_id, transform] : obj_id_to_transform) {

        XMStoreFloat4x4(&buff.matWorld[obj_id], transform);
    }

    player.fill_const_buffer(buff);


    XMStoreFloat4x4(&buff.matProj, proj);

    buff.colLight = {1.0f, 1.0f, 1.0f, 1.0f};
    buff.dirLight = {1, 1, 1, 0.0f};

    memcpy(matrix_buffer.data(), &buff, sizeof(buff));
}

void Game::set_root_signature() {
    D3D12_DESCRIPTOR_RANGE root_signature_ranges[] = {
        {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
         .NumDescriptors = 1,
         .BaseShaderRegister = 0,
         .RegisterSpace = 0,
         .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
        {.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
         .NumDescriptors = 1,
         .BaseShaderRegister = 0,
         .RegisterSpace = 0,
         .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}
    };

    D3D12_ROOT_PARAMETER root_signature_params[] = {
        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
         .DescriptorTable = {1, &root_signature_ranges[0]},
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL  },
        {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
         .DescriptorTable = {1, &root_signature_ranges[1]},
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL}
    };

    D3D12_STATIC_SAMPLER_DESC tex_sampler_desc = {
        .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .MipLODBias = 0,
        .MaxAnisotropy = 0,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
        .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
        .MinLOD = 0.0f,
        .MaxLOD = D3D12_FLOAT32_MAX,
        .ShaderRegister = 0,
        .RegisterSpace = 0,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL};


    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};

    root_signature_desc.NumParameters = _countof(root_signature_params);
    root_signature_desc.pParameters = root_signature_params;
    root_signature_desc.NumStaticSamplers = 1;
    root_signature_desc.pStaticSamplers = &tex_sampler_desc;
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                                | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                                | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                                | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    check_output(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                             &signature, &error));
    check_output(m_device->CreateRootSignature(0, signature->GetBufferPointer(),
                                               signature->GetBufferSize(),
                                               IID_PPV_ARGS(&m_rootSignature)));
}

void Game::create_graphics_pipeline_state() {

    D3D12_INPUT_ELEMENT_DESC input_elements[] = {
        { .SemanticName = "POSITION",
         .SemanticIndex = 0,
         .Format = DXGI_FORMAT_R32G32B32_FLOAT,
         .InputSlot = 0,
         .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
         .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         .InstanceDataStepRate = 0},
        {   .SemanticName = "NORMAL",
         .SemanticIndex = 0,
         .Format = DXGI_FORMAT_R32G32B32_FLOAT,
         .InputSlot = 0,
         .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
         .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         .InstanceDataStepRate = 0},
        { .SemanticName = "TEXCOORD",
         .SemanticIndex = 0,
         .Format = DXGI_FORMAT_R32G32_FLOAT,
         .InputSlot = 0,
         .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
         .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         .InstanceDataStepRate = 0},
        {.SemanticName = "MAT_INDEX",
         .SemanticIndex = 0,
         .Format = DXGI_FORMAT_R32_UINT,
         .InputSlot = 0,
         .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
         .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         .InstanceDataStepRate = 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
        .pRootSignature = m_rootSignature.Get(),
        .VS = {vs_main, sizeof(vs_main)},
        .PS = {ps_main, sizeof(ps_main)},

        .BlendState = {.AlphaToCoverageEnable = FALSE,
               .IndependentBlendEnable = FALSE,
               .RenderTarget = {{.BlendEnable = FALSE,
                                         .LogicOpEnable = FALSE,
                                         .SrcBlend = D3D12_BLEND_ONE,
                                         .DestBlend = D3D12_BLEND_ZERO,
                                         .BlendOp = D3D12_BLEND_OP_ADD,
                                         .SrcBlendAlpha = D3D12_BLEND_ONE,
                                         .DestBlendAlpha = D3D12_BLEND_ZERO,
                                         .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                                         .LogicOp = D3D12_LOGIC_OP_NOOP,
                                         .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL}}},

        .SampleMask = UINT_MAX,
        .RasterizerState = {.FillMode = D3D12_FILL_MODE_SOLID,
               .CullMode = D3D12_CULL_MODE_BACK,
               .FrontCounterClockwise = FALSE,
               .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
               .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
               .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
               .DepthClipEnable = TRUE,
               .MultisampleEnable = FALSE,
               .AntialiasedLineEnable = FALSE,
               .ForcedSampleCount = 0,
               .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF

        },
        .DepthStencilState = {.DepthEnable = TRUE,
               .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
               .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
               .StencilEnable = FALSE,
               .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
               .StencilWriteMask = D3D12_DEFAULT_STENCIL_READ_MASK,
               .FrontFace = {.StencilFailOp = D3D12_STENCIL_OP_KEEP,
                                            .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                                            .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                                            .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS},
               .BackFace = {.StencilFailOp = D3D12_STENCIL_OP_KEEP,
                                           .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                                           .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                                           .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS}},
        .InputLayout = {input_elements, _countof(input_elements)},
        .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats = {DXGI_FORMAT_R8G8B8A8_UNORM},
        .DSVFormat = DXGI_FORMAT_D32_FLOAT,
        .SampleDesc = {.Count = 1, .Quality = 0}
    };

    check_output(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

void Game::init_debug_layer() {
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
}

void Game::init_swap_chain() {
    ComPtr<IDXGIFactory2> factory;
    check_output(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = 0;
    swapChainDesc.Height = 0;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE; // DXGI_ALPHA_MODE_STRAIGHT;
    swapChainDesc.Flags = 0;

    ComPtr<IDXGISwapChain1> swapChain;
    check_output(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &swapChainDesc,
                                                 nullptr, nullptr, &swapChain));

    check_output(swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain)));
}

void Game::init_command_queue() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    check_output(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
}

Game::Game() {}

void Game::init(HWND _hwnd) {
    hwnd = _hwnd;

    D3D12_RECT hwnd_rect;
    GetClientRect(hwnd, &hwnd_rect);
    width = hwnd_rect.right;
    height = hwnd_rect.bottom;

#if defined(_DEBUG)
    init_debug_layer();
#endif

    check_output(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));

    init_command_queue();
    init_swap_chain();
    const_heaps.init(m_device, heap_ids::num);
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));


        m_rtvDescriptorSize =
            m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        // Create a RTV for each frame.
        for (UINT i = 0; i < FrameCount; i++) {
            // Retrieve the swap chain buffer
            m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));

            // Create the render target view
            m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);

            m_rtvHandles[i] = rtvHandle;

            // Offset the handle for the next RTV
            rtvHandle.ptr += m_rtvDescriptorSize;
        }
    }

    for (unsigned int i = 0; i < FrameCount; i++) {
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(&m_commandAllocator[i]));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator[i].Get(),
                                    nullptr, IID_PPV_ARGS(&m_commandList[i]));

        m_commandList[i]->Close();
    }

    gpu_waiter.init(m_device);

    texture_loader.init();
    set_root_signature();
    create_graphics_pipeline_state();

    init_environment_objects();


    player.init(m_device, texture_loader, const_heaps.get_cpu_handle(heap_ids::person_tex),
                const_heaps.get_gpu_handle(heap_ids::person_tex), object_id_giver);
    matrix_buffer.init(m_device, sizeof(Shader_const_buffer),
                       const_heaps.get_cpu_handle(heap_ids::const_buff));
    depth_buffer.init(m_device, width, height);
}

void Game::release() {}

void Game::resize(UINT _width, UINT _height) {
    width = _width;
    height = _height;
}

void Game::update() {
    float delta_time = get_delta_time();
    player.update(delta_time);
}

void Game::key_down(WPARAM key_code, LPARAM flags) {
    if (!(flags & KF_REPEAT)) {
        player.key_down(key_code);
    }
}

void Game::key_up(WPARAM key_code, LPARAM flags) {
    player.key_up(key_code);
}

void Game::paint() {


    double time = get_time();

    recalculate_matrix(time);

    HRESULT hr;

    check_output(m_commandAllocator[m_frameIndex]->Reset());
    check_output(m_commandList[m_frameIndex]->Reset(m_commandAllocator[m_frameIndex].Get(),
                                                    m_pipelineState.Get()));

    m_commandList[m_frameIndex]->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap *pHeaps = const_heaps.get_heap_ptr();
    m_commandList[m_frameIndex]->SetDescriptorHeaps(1, &pHeaps);


    m_commandList[m_frameIndex]->SetGraphicsRootDescriptorTable(0, const_heaps.get_gpu_handle(0));


    D3D12_VIEWPORT viewport = {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = static_cast<float>(width),
        .Height = static_cast<float>(height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,

    };
    D3D12_RECT scissor_rect = {
        .left = 0, .right = static_cast<LONG>(width), .bottom = static_cast<LONG>(height)};
    m_commandList[m_frameIndex]->RSSetViewports(1, &viewport);
    m_commandList[m_frameIndex]->RSSetScissorRects(1, &scissor_rect);

    D3D12_RESOURCE_BARRIER barrier;
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_commandList[m_frameIndex]->ResourceBarrier(1, &barrier);

    m_commandList[m_frameIndex]->OMSetRenderTargets(1, &m_rtvHandles[m_frameIndex], FALSE,
                                                    &depth_buffer.get_view());


    constexpr static FLOAT yellow[4] = {1, 0, 1, 1};
    m_commandList[m_frameIndex]->ClearRenderTargetView(m_rtvHandles[m_frameIndex], yellow, 0,
                                                       nullptr);


    m_commandList[m_frameIndex]->ClearDepthStencilView(
        depth_buffer.get_view(), D3D12_CLEAR_FLAG_STENCIL | D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0,
        nullptr);

    m_commandList[m_frameIndex]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& object : environment_objects) {
        object.draw(m_commandList[m_frameIndex]);
    }
    
    player.draw(m_commandList[m_frameIndex]);

    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_commandList[m_frameIndex]->ResourceBarrier(1, &barrier);

    check_output(m_commandList[m_frameIndex]->Close());

    ID3D12CommandList *ppCommandLists[] = {m_commandList[m_frameIndex].Get()};
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    check_output(m_swapChain->Present(1, 0));

    gpu_waiter.wait(m_commandQueue);

    m_frameIndex ^= 1;
}
