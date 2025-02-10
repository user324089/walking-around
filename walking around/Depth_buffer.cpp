#include "Depth_buffer.hpp"

void Depth_buffer::init_descriptor_heap(ComPtr<ID3D12Device> &device) {
    D3D12_DESCRIPTOR_HEAP_DESC depthBuffHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };

    device->CreateDescriptorHeap(&depthBuffHeapDesc, IID_PPV_ARGS(&m_depthBuffHeap));
}

void Depth_buffer::init(ComPtr<ID3D12Device> &device, UINT width, UINT height) {
    init_descriptor_heap(device);
    D3D12_HEAP_PROPERTIES heapProps = {
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1,

    };

    D3D12_RESOURCE_DESC desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = width,
        .Height = height,
        .DepthOrArraySize = 1,
        .MipLevels = 0,
        .Format = DXGI_FORMAT_D32_FLOAT,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    };

    D3D12_CLEAR_VALUE clear_val = {
        .Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {.Depth = 1.0f, .Stencil = 0}
    };

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                    D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_val,
                                    IID_PPV_ARGS(&m_depthBuffer));


    D3D12_DEPTH_STENCIL_VIEW_DESC view_desc{.Format = DXGI_FORMAT_D32_FLOAT,
                                            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                                            .Flags = D3D12_DSV_FLAG_NONE,
                                            .Texture2D = {}};


    m_depthStencilView = m_depthBuffHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateDepthStencilView(m_depthBuffer.Get(), &view_desc, m_depthStencilView);
}

D3D12_CPU_DESCRIPTOR_HANDLE &Depth_buffer::get_view() {
    return m_depthStencilView;
}
