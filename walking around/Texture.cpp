#include "Texture.hpp"

#include "GPU_waiter.hpp"

void Texture::init(ComPtr<ID3D12Device> &device, unsigned int width, unsigned int height,
                          BYTE *data, const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                          const D3D12_GPU_DESCRIPTOR_HANDLE &_gpu_handle) {

    gpu_handle = _gpu_handle;

    ComPtr<ID3D12CommandQueue> command_queue;
    ComPtr<ID3D12CommandAllocator> command_allocator;
    ComPtr<ID3D12GraphicsCommandList> command_list;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&command_queue));

    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                   IID_PPV_ARGS(&command_allocator));
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator.Get(), nullptr,
                              IID_PPV_ARGS(&command_list));


    // Creating texture resource
    D3D12_HEAP_PROPERTIES tex_heap_prop = {.Type = D3D12_HEAP_TYPE_DEFAULT,
                                           .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                           .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                           .CreationNodeMask = 1,
                                           .VisibleNodeMask = 1};
    D3D12_RESOURCE_DESC tex_resource_desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = width,
        .Height = height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    check_output(device->CreateCommittedResource(&tex_heap_prop, D3D12_HEAP_FLAG_NONE,
                                                 &tex_resource_desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                 nullptr, IID_PPV_ARGS(&texture_resource)));

    // Creating auxiliary buffer to upload to GPU
    ComPtr<ID3D12Resource> texture_upload_buffer = nullptr;

    // finding the auxiliary buffer needed size
    UINT64 required_size = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    UINT num_rows;
    UINT64 row_size_in_bytes;
    D3D12_RESOURCE_DESC resource_desc = texture_resource.Get()->GetDesc();
    ComPtr<ID3D12Device> helper_device = nullptr;
    check_output(texture_resource.Get()->GetDevice(
        __uuidof(helper_device), reinterpret_cast<void **>(helper_device.GetAddressOf())));
    helper_device->GetCopyableFootprints(&resource_desc, 0, 1, 0, &layout, &num_rows,
                                         &row_size_in_bytes, &required_size);

    D3D12_HEAP_PROPERTIES tex_upload_heap_prop = {.Type = D3D12_HEAP_TYPE_UPLOAD,
                                                  .CPUPageProperty =
                                                      D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                  .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                                  .CreationNodeMask = 1,
                                                  .VisibleNodeMask = 1};
    D3D12_RESOURCE_DESC tex_upload_resource_desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = required_size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    check_output(device->CreateCommittedResource(
        &tex_upload_heap_prop, D3D12_HEAP_FLAG_NONE, &tex_upload_resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&texture_upload_buffer)));

    // copying texture data to buffer
    D3D12_SUBRESOURCE_DATA texture_data = {
        .pData = data, .RowPitch = width * BMP_PX_SIZE, .SlicePitch = width * height * BMP_PX_SIZE};

    UINT8 *map_tex_data = nullptr;
    check_output(texture_upload_buffer->Map(0, nullptr, reinterpret_cast<void **>(&map_tex_data)));
    D3D12_MEMCPY_DEST dest_data = {.pData = map_tex_data + layout.Offset,
                                   .RowPitch = layout.Footprint.RowPitch,
                                   .SlicePitch =
                                       SIZE_T(layout.Footprint.RowPitch) * SIZE_T(num_rows)};
    for (UINT z = 0; z < layout.Footprint.Depth; ++z) {
        auto pDestSlice = static_cast<UINT8 *>(dest_data.pData) + dest_data.SlicePitch * z;
        auto pSrcSlice =
            static_cast<const UINT8 *>(texture_data.pData) + texture_data.SlicePitch * LONG_PTR(z);
        for (UINT y = 0; y < num_rows; ++y) {
            memcpy(pDestSlice + dest_data.RowPitch * y,
                   pSrcSlice + texture_data.RowPitch * LONG_PTR(y),
                   static_cast<SIZE_T>(row_size_in_bytes));
        }
    }
    texture_upload_buffer->Unmap(0, nullptr);

    // copying from auxiliary buffer to texture
    D3D12_TEXTURE_COPY_LOCATION Dst = {.pResource = texture_resource.Get(),
                                       .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                                       .SubresourceIndex = 0};
    D3D12_TEXTURE_COPY_LOCATION Src = {.pResource = texture_upload_buffer.Get(),
                                       .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                                       .PlacedFootprint = layout};
    command_list->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);

    D3D12_RESOURCE_BARRIER tex_upload_resource_barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {.pResource = texture_resource.Get(),
                       .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                       .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
                       .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE},
    };
    command_list->ResourceBarrier(1, &tex_upload_resource_barrier);
    check_output(command_list->Close());

    ID3D12CommandList *cmd_list = command_list.Get();
    command_queue->ExecuteCommandLists(1, &cmd_list);

    // creating texture view
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
        .Format = tex_resource_desc.Format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = {.MostDetailedMip = 0,
                      .MipLevels = 1,
                      .PlaneSlice = 0,
                      .ResourceMinLODClamp = 0.0f},
    };
    device->CreateShaderResourceView(texture_resource.Get(), &srv_desc, cpu_handle);

    GPU_waiter gpu_waiter;
    gpu_waiter.init(device);
    gpu_waiter.wait(command_queue);
}

void Texture::use(ComPtr<ID3D12GraphicsCommandList> &command_list, unsigned int arg_num) {
    command_list->SetGraphicsRootDescriptorTable(arg_num, gpu_handle);
}
