#include "Const_buffer.hpp"

unsigned int Const_buffer::find_larger_256_divisible(unsigned int size) {
    unsigned int masked_left = size & (~255);
    bool masked_right = size & 255;
    return masked_left + masked_right * 256;
}

void Const_buffer::init(ComPtr<ID3D12Device> &device, unsigned int size,
                               const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle) {

    size = find_larger_256_divisible(size);
    D3D12_HEAP_PROPERTIES heapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD,
                                       .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                       .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                       .CreationNodeMask = 1,
                                       .VisibleNodeMask = 1};

    D3D12_RESOURCE_DESC desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = size, //  size of const buffer, must be divisible by 256
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };


    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                    IID_PPV_ARGS(&m_constBuffer));

    D3D12_CONSTANT_BUFFER_VIEW_DESC const_buffer_desc = {
        .BufferLocation = m_constBuffer->GetGPUVirtualAddress(), .SizeInBytes = size};
    device->CreateConstantBufferView(&const_buffer_desc, cpu_handle);

    D3D12_RANGE zero_range = {.Begin = 0, .End = 0};
    m_constBuffer->Map(0, &zero_range, &memory);
}

void *Const_buffer::data() {
    return memory;
}
