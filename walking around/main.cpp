#define WIN32_LEAN_AND_MEAN

#include <DirectXMath.h>
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <numbers>
#include <wrl.h>
#include <wincodec.h>
#include <source_location>


#include "pixel_shader.h"
#include "vertex_shader.h"


using namespace Microsoft::WRL;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

constexpr UINT BMP_PX_SIZE = 4;

constexpr unsigned int const_buffer_size = 256;

struct vs_const_buffer_t {
        DirectX::XMFLOAT4X4 matWorldViewProj;
        DirectX::XMFLOAT4X4 matWorldViewInvT;
        DirectX::XMFLOAT4X4 matView;
        DirectX::XMFLOAT4 colMaterial, colLight, dirLight, padding;
};

void check_output(HRESULT res, std::source_location loc = std::source_location::current()) {
    if (res != S_OK) {
        std::stringstream stream;
        stream << "EXCEPTION in function " << loc.function_name() << " on line " << loc.line();
        throw std::runtime_error(stream.str());
    }
}

class Depth_buffer {
    private:
        ComPtr<ID3D12DescriptorHeap> m_depthBuffHeap;
        ComPtr<ID3D12Resource> m_depthBuffer;

        D3D12_CPU_DESCRIPTOR_HANDLE m_depthStencilView;

        void init_descriptor_heap(ComPtr<ID3D12Device> &device) {
            D3D12_DESCRIPTOR_HEAP_DESC depthBuffHeapDesc = {
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                .NumDescriptors = 1,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                .NodeMask = 0,
            };

            device->CreateDescriptorHeap(&depthBuffHeapDesc, IID_PPV_ARGS(&m_depthBuffHeap));
        }

    public:
        void init(ComPtr<ID3D12Device> &device, UINT width, UINT height) {
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

        D3D12_CPU_DESCRIPTOR_HANDLE &get_view() {
            return m_depthStencilView;
        }
};

class Vertex_buffer {
    private:
        ComPtr<ID3D12Resource> m_vertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
        unsigned int m_vertex_count = 0;

    public:
        template <typename VERTEX_TYPE>
        void init(ComPtr<ID3D12Device> &device, const std::vector<VERTEX_TYPE> &vertex_data) {
            m_vertex_count = vertex_data.size();
            unsigned int data_size = sizeof(VERTEX_TYPE) * m_vertex_count;
            // inits m_vertexBuffer and m_vertexBufferView
            D3D12_HEAP_PROPERTIES heapProps = {
                .Type = D3D12_HEAP_TYPE_UPLOAD,
                .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                .CreationNodeMask = 1,
                .VisibleNodeMask = 1,
            };

            D3D12_RESOURCE_DESC desc = {
                .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                .Alignment = 0,
                .Width = data_size, //  VERTEX_BUFFER_SIZE,
                .Height = 1,
                .DepthOrArraySize = 1,
                .MipLevels = 1,
                .Format = DXGI_FORMAT_UNKNOWN,
                .SampleDesc = {.Count = 1, .Quality = 0},
                .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                .Flags = D3D12_RESOURCE_FLAG_NONE
            };

            device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                            IID_PPV_ARGS(&m_vertexBuffer));

            void *vertex_memory;
            D3D12_RANGE zero_range = {.Begin = 0, .End = 0};
            m_vertexBuffer->Map(0, &zero_range, &vertex_memory);
            std::memcpy(vertex_memory, vertex_data.data(), data_size);
            m_vertexBuffer->Unmap(0, nullptr);

            m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
            m_vertexBufferView.StrideInBytes = sizeof(VERTEX_TYPE);
            m_vertexBufferView.SizeInBytes = data_size;
        }

        D3D12_VERTEX_BUFFER_VIEW &get_view() {
            return m_vertexBufferView;
        }

        unsigned int get_vertex_count() {
            return m_vertex_count;
        }
};

class GPU_waiter {
    private:
        ComPtr<ID3D12Fence> m_fence;
        HANDLE m_fenceEvent;
        UINT64 m_fenceValue = 0;

    public:
        void init(ComPtr<ID3D12Device> &device) {
            device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &m_fence);
        }

        void wait(ComPtr<ID3D12CommandQueue> &command_queue) {
            m_fenceValue++;

            command_queue->Signal(m_fence.Get(), m_fenceValue);
            m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);

            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
};

class Const_Heaps {
    private:

        ComPtr<ID3D12DescriptorHeap> m_constBuffHeap;
        unsigned int increment = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = {};

    public:
        void init(ComPtr<ID3D12Device> &device, unsigned int number) {
            D3D12_DESCRIPTOR_HEAP_DESC constBuffHeapDesc = {
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                .NumDescriptors = number,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                .NodeMask = 0};

            device->CreateDescriptorHeap(&constBuffHeapDesc, IID_PPV_ARGS(&m_constBuffHeap));

            increment =
                device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            cpu_handle = m_constBuffHeap->GetCPUDescriptorHandleForHeapStart();
            gpu_handle = m_constBuffHeap->GetGPUDescriptorHandleForHeapStart();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(unsigned int index) {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = cpu_handle;
            handle.ptr += increment * index;
            return handle;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(unsigned int index) {
            D3D12_GPU_DESCRIPTOR_HANDLE handle = gpu_handle;
            handle.ptr += increment * index;
            return handle;
        }

        ID3D12DescriptorHeap *get_heap_ptr() {
            return m_constBuffHeap.Get();
        }
};

class Texture {
    private:
        ComPtr<ID3D12Resource> texture_resource;

    public:
        void init(ComPtr<ID3D12Device> &device, unsigned int width, unsigned int height, BYTE *data,
                  const D3D12_CPU_DESCRIPTOR_HANDLE& cpu_handle) {

            ComPtr<ID3D12CommandQueue> command_queue;
            ComPtr<ID3D12CommandAllocator> command_allocator;
            ComPtr<ID3D12GraphicsCommandList> command_list;

            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

            device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&command_queue));

            device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             IID_PPV_ARGS(&command_allocator));
            device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        command_allocator.Get(), nullptr,
                                        IID_PPV_ARGS(&command_list));
            

            
            // Budowa właściwego zasobu tekstury
            D3D12_HEAP_PROPERTIES tex_heap_prop = {
                .Type = D3D12_HEAP_TYPE_DEFAULT,
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
            check_output(device->CreateCommittedResource(
                &tex_heap_prop, D3D12_HEAP_FLAG_NONE, &tex_resource_desc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture_resource)));

            // Budowa pomocniczego bufora wczytania do GPU
            ComPtr<ID3D12Resource> texture_upload_buffer = nullptr; // TODO change back to local

            // - ustalenie rozmiaru tego pom. bufora
            // i danych o podzasobach tekstury (liczbie linii i ich
            // rozmiarze w bajtach)
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

            // - utworzenie tego pom. bufora
            D3D12_HEAP_PROPERTIES tex_upload_heap_prop = {
                .Type = D3D12_HEAP_TYPE_UPLOAD,
                .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
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

            // - skopiowanie danych tekstury do pom. bufora
            D3D12_SUBRESOURCE_DATA texture_data = {.pData = data,
                                                   .RowPitch = width * BMP_PX_SIZE,
                                                   .SlicePitch =
                                                       width * height * BMP_PX_SIZE};
            // ... ID3D12GraphicsCommandList::Reset() - to dlatego lista
            // poleceń i obiekt stanu potoku muszą być wcześniej utworzone

            command_list->Reset(command_allocator.Get(), nullptr); // TODO is this reset necessary

            UINT8 *map_tex_data = nullptr;
            check_output(
                texture_upload_buffer->Map(0, nullptr, reinterpret_cast<void **>(&map_tex_data)));
            D3D12_MEMCPY_DEST dest_data = {.pData = map_tex_data + layout.Offset,
                                           .RowPitch = layout.Footprint.RowPitch,
                                           .SlicePitch = SIZE_T(layout.Footprint.RowPitch)
                                                         * SIZE_T(num_rows)};
            for (UINT z = 0; z < layout.Footprint.Depth; ++z) {
                auto pDestSlice = static_cast<UINT8 *>(dest_data.pData) + dest_data.SlicePitch * z;
                auto pSrcSlice = static_cast<const UINT8 *>(texture_data.pData)
                                 + texture_data.SlicePitch * LONG_PTR(z);
                for (UINT y = 0; y < num_rows; ++y) {
                    memcpy(pDestSlice + dest_data.RowPitch * y,
                           pSrcSlice + texture_data.RowPitch * LONG_PTR(y),
                           static_cast<SIZE_T>(row_size_in_bytes));
                }
            }
            texture_upload_buffer->Unmap(0, nullptr);

            // -  zlecenie procesorowi GPU skopiowania buf. pom.
            //    do właściwego zasobu tekstury
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

            // - tworzy SRV (widok zasobu shadera) dla tekstury
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
                .Format = tex_resource_desc.Format,
                .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Texture2D = {.MostDetailedMip = 0,
                              .MipLevels = 1,
                              .PlaneSlice = 0,
                              .ResourceMinLODClamp = 0.0f},
            };
            device->CreateShaderResourceView(texture_resource.Get(), &srv_desc,
                                               cpu_handle);

            GPU_waiter gpu_waiter;
            gpu_waiter.init(device);
            gpu_waiter.wait(command_queue);
        }

};

class painter {
    private:

        struct vertex_t {
            public:
                FLOAT position[3];
                FLOAT normal[3];
                FLOAT color[4];
                FLOAT tex_coord[2];
        };

        UINT width = 0, height = 0;
        HWND hwnd = 0;

        constexpr static UINT FrameCount = 2;

        ComPtr<ID3D12Device> m_device;
        ComPtr<ID3D12CommandQueue> m_commandQueue;
        ComPtr<IDXGISwapChain3> m_swapChain;
        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
        ComPtr<ID3D12CommandAllocator> m_commandAllocator[FrameCount];
        ComPtr<ID3D12GraphicsCommandList> m_commandList[FrameCount];

        Depth_buffer depth_buffer;

        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_pipelineState;

        Const_Heaps const_heaps;

        Vertex_buffer cube_vertex_buffer;

        ComPtr<ID3D12Resource> m_constBuffer;
        void *m_const_buffer_memory;

        GPU_waiter gpu_waiter;

        UINT m_rtvDescriptorSize;
        UINT m_frameIndex = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[FrameCount];

        std::chrono::high_resolution_clock::time_point start_point =
            std::chrono::high_resolution_clock::now();

        IWICImagingFactory *m_wic_factory;

        //UINT m_bmp_width, m_bmp_height;
        //BYTE *m_bmp_bits;

        //ComPtr<ID3D12Resource> texture_resource;
        Texture smile_texture;

        HRESULT LoadBitmapFromFile(PCWSTR uri, UINT &width, UINT &height, BYTE **ppBits) {
            HRESULT hr;
            ComPtr<IWICBitmapDecoder> pDecoder = nullptr;
            ComPtr<IWICBitmapFrameDecode> pSource = nullptr;
            ComPtr<IWICFormatConverter> pConverter = nullptr;

            hr = m_wic_factory->CreateDecoderFromFilename(
                uri, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, pDecoder.GetAddressOf());

            if (SUCCEEDED(hr)) {
                hr = pDecoder->GetFrame(0, pSource.GetAddressOf());
            }
            if (SUCCEEDED(hr)) {
                hr = m_wic_factory->CreateFormatConverter(pConverter.GetAddressOf());
            }
            if (SUCCEEDED(hr)) {
                hr = pConverter->Initialize(pSource.Get(), GUID_WICPixelFormat32bppRGBA,
                                            WICBitmapDitherTypeNone, nullptr, 0.0f,
                                            WICBitmapPaletteTypeMedianCut);
            }
            if (SUCCEEDED(hr)) {
                hr = pConverter->GetSize(&width, &height);
            }
            if (SUCCEEDED(hr)) {
                *ppBits = new BYTE[4 * width * height];
                hr = pConverter->CopyPixels(nullptr, 4 * width, 4 * width * height, *ppBits);
            }
            return hr;
        }

        double get_time() {
            std::chrono::high_resolution_clock::time_point now_point =
                std::chrono::high_resolution_clock::now();
            double microseconds_passed =
                std::chrono::duration_cast<std::chrono::microseconds>(now_point - start_point)
                    .count();
            return microseconds_passed / 1'000'000;
        }

        void recalculate_matrix(double angle) {

            DirectX::XMMATRIX world, view, proj;

            world = DirectX::XMMatrixMultiply(
                DirectX::XMMatrixRotationY(2.5f * angle), // zmienna angle zmienia się
                // o 1 / 64 co ok. 15 ms
                DirectX::XMMatrixRotationX(static_cast<FLOAT>(sin(angle)) / 2.0f));

            view = DirectX::XMMatrixTranslation(0.0f, 0.0f, 4.0f);

            proj = DirectX::XMMatrixPerspectiveFovLH(45.0f, width / height, 1.0f, 100.0f);

            DirectX::XMMATRIX world_view = DirectX::XMMatrixMultiply(world, view);
            DirectX::XMMATRIX world_view_proj = DirectX::XMMatrixMultiply(world_view, proj);

            view = XMMatrixTranspose(view);
            world_view = XMMatrixTranspose(world_view);
            world_view_proj = XMMatrixTranspose(world_view_proj);

            vs_const_buffer_t buff;

            XMStoreFloat4x4(&buff.matWorldViewProj, world_view_proj);
            XMStoreFloat4x4(&buff.matWorldViewInvT, world_view); // DUE TO ORTHOGONAL MATRIX
            XMStoreFloat4x4(&buff.matView, view);

            buff.colLight = {1.0f, 0.5f, 0.5f, 1.0f};
            buff.dirLight = {1.0f, 1.0f, 1.0f, 0.0f};

            memcpy(m_const_buffer_memory, &buff, sizeof(buff));
        }

        void set_root_signature() {
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

            /*
            D3D12_ROOT_PARAMETER root_signature_params[] = {
                {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                 .DescriptorTable = {1, root_signature_ranges},
                 .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX}
            };
            */
            D3D12_ROOT_PARAMETER root_signature_params[] = {
                {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                 .DescriptorTable = {1, &root_signature_ranges[0]},
                 .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX},
                {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                 .DescriptorTable = {1, &root_signature_ranges[1]},
                 .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL }
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
            root_signature_desc.Flags =
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                        &signature, &error);
            m_device->CreateRootSignature(0, signature->GetBufferPointer(),
                                          signature->GetBufferSize(),
                                          IID_PPV_ARGS(&m_rootSignature));
        }

        void create_graphics_pipeline_state() {

            D3D12_INPUT_ELEMENT_DESC input_elements[] = {
                {.SemanticName = "POSITION",
                 .SemanticIndex = 0,
                 .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                 .InputSlot = 0,
                 .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                 .InstanceDataStepRate = 0},
                {  .SemanticName = "NORMAL",
                 .SemanticIndex = 0,
                 .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                 .InputSlot = 0,
                 .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                 .InstanceDataStepRate = 0},
                {   .SemanticName = "COLOR",
                 .SemanticIndex = 0,
                 .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                 .InputSlot = 0,
                 .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                 .InstanceDataStepRate = 0},
                {.SemanticName = "TEXCOORD",
                 .SemanticIndex = 0,
                 .Format = DXGI_FORMAT_R32G32_FLOAT,
                 .InputSlot = 0,
                 .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                 .InstanceDataStepRate = 0}
            };

            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
                .pRootSignature = m_rootSignature.Get(),
                .VS = {vs_main, sizeof(vs_main)}, // bytecode vs w tablicy vs_main
                .PS = {ps_main, sizeof(ps_main)}, // bytecode ps w tablicy ps_main

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
                                                 .RenderTargetWriteMask =
                                                     D3D12_COLOR_WRITE_ENABLE_ALL}}},

                .SampleMask = UINT_MAX, // same 1 w rozwinięciu binarnym
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
                .InputLayout =
                    {
                       input_elements,          // tablica opisująca układ danych wierzchołka
                        _countof(input_elements) // liczba elementów powyższej tablicy
                    },
                .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
                .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
                .NumRenderTargets = 1, // Potok zapisuje tylko w jednym celu na raz
                .RTVFormats = {DXGI_FORMAT_R8G8B8A8_UNORM},
                .DSVFormat = DXGI_FORMAT_D32_FLOAT,
                .SampleDesc = {.Count = 1, .Quality = 0}
            };

            m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
        }

        std::vector<vertex_t> gen_data() {
            std::vector<vertex_t> cube_data = {
                {{-1, -1, -1}, {0, 0, -1}, {1, 0, 0, 1}, {0, 0}}, // back
                { {-1, 1, -1}, {0, 0, -1}, {1, 0, 0, 1}, {0, 1}},
                { {1, -1, -1}, {0, 0, -1}, {1, 0, 0, 1}, {1, 0}},
                { {1, -1, -1}, {0, 0, -1}, {1, 0, 0, 1}, {1, 0}},
                { {-1, 1, -1}, {0, 0, -1}, {1, 0, 0, 1}, {0, 1}},
                {  {1, 1, -1}, {0, 0, -1}, {1, 0, 0, 1}, {1, 1}},


                { {-1, -1, 1},  {0, 0, 1}, {1, 0, 0, 1}, {0, 0}}, // front
                {  {1, -1, 1},  {0, 0, 1}, {1, 0, 0, 1}, {1, 0}},
                {  {-1, 1, 1},  {0, 0, 1}, {1, 0, 0, 1}, {0, 1}},
                {  {-1, 1, 1},  {0, 0, 1}, {1, 0, 0, 1}, {0, 1}},
                {  {1, -1, 1},  {0, 0, 1}, {1, 0, 0, 1}, {1, 0}},
                {   {1, 1, 1},  {0, 0, 1}, {1, 0, 0, 1}, {1, 1}},


                { {1, -1, -1},  {1, 0, 0}, {1, 0, 0, 1}, {0, 0}}, // right
                {  {1, 1, -1},  {1, 0, 0}, {1, 0, 0, 1}, {1, 0}},
                {  {1, -1, 1},  {1, 0, 0}, {1, 0, 0, 1}, {0, 1}},
                {  {1, -1, 1},  {1, 0, 0}, {1, 0, 0, 1}, {0, 1}},
                {  {1, 1, -1},  {1, 0, 0}, {1, 0, 0, 1}, {1, 0}},
                {   {1, 1, 1},  {1, 0, 0}, {1, 0, 0, 1}, {1, 1}},


                {{-1, -1, -1}, {-1, 0, 0}, {1, 0, 0, 1}, {0, 0}}, // left
                { {-1, -1, 1}, {-1, 0, 0}, {1, 0, 0, 1}, {0, 1}},
                { {-1, 1, -1}, {-1, 0, 0}, {1, 0, 0, 1}, {1, 0}},
                { {-1, 1, -1}, {-1, 0, 0}, {1, 0, 0, 1}, {1, 0}},
                { {-1, -1, 1}, {-1, 0, 0}, {1, 0, 0, 1}, {0, 1}},
                {  {-1, 1, 1}, {-1, 0, 0}, {1, 0, 0, 1}, {1, 1}},


                { {-1, 1, -1},  {0, 1, 0}, {1, 0, 0, 1}, {0, 0}}, // up
                {  {-1, 1, 1},  {0, 1, 0}, {1, 0, 0, 1}, {0, 1}},
                {  {1, 1, -1},  {0, 1, 0}, {1, 0, 0, 1}, {1, 0}},
                {  {1, 1, -1},  {0, 1, 0}, {1, 0, 0, 1}, {1, 0}},
                {  {-1, 1, 1},  {0, 1, 0}, {1, 0, 0, 1}, {0, 1}},
                {   {1, 1, 1},  {0, 1, 0}, {1, 0, 0, 1}, {1, 1}},


                {{-1, -1, -1}, {0, -1, 0}, {1, 0, 0, 1}, {0, 0}}, // down
                { {1, -1, -1}, {0, -1, 0}, {1, 0, 0, 1}, {1, 0}},
                { {-1, -1, 1}, {0, -1, 0}, {1, 0, 0, 1}, {0, 1}},
                { {-1, -1, 1}, {0, -1, 0}, {1, 0, 0, 1}, {0, 1}},
                { {1, -1, -1}, {0, -1, 0}, {1, 0, 0, 1}, {1, 0}},
                {  {1, -1, 1}, {0, -1, 0}, {1, 0, 0, 1}, {1, 1}},
            };
            return cube_data;
        }

        std::vector<vertex_t> generate_data() {

            std::vector<vertex_t> data = gen_data();

            // m_numDataVertices = data.size();
            return data;
        }

        void init_const_buffer() {
            D3D12_HEAP_PROPERTIES heapProps = {.Type = D3D12_HEAP_TYPE_UPLOAD,
                                               .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                               .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                               .CreationNodeMask = 1,
                                               .VisibleNodeMask = 1};

            D3D12_RESOURCE_DESC desc = {
                .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                .Alignment = 0,
                .Width = const_buffer_size, //  size of const buffer, must be divisible by 256
                .Height = 1,
                .DepthOrArraySize = 1,
                .MipLevels = 1,
                .Format = DXGI_FORMAT_UNKNOWN,
                .SampleDesc = {.Count = 1, .Quality = 0},
                .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                .Flags = D3D12_RESOURCE_FLAG_NONE,
            };

            m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(&m_constBuffer));

            D3D12_CONSTANT_BUFFER_VIEW_DESC const_buffer_desc = {
                .BufferLocation = m_constBuffer->GetGPUVirtualAddress(),
                .SizeInBytes = const_buffer_size};
            m_device->CreateConstantBufferView(&const_buffer_desc, const_heaps.get_cpu_handle(0));

            D3D12_RANGE zero_range = {.Begin = 0, .End = 0};
            m_constBuffer->Map(0, &zero_range, &m_const_buffer_memory);
        }

        void load_assets() {
            set_root_signature();
            create_graphics_pipeline_state();
            cube_vertex_buffer.init(m_device, generate_data());
            init_const_buffer();
            depth_buffer.init(m_device, width, height);
        }

        void init_debug_layer() {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
                debugController->EnableDebugLayer();
            }
        }

        void init_swap_chain() {
            ComPtr<IDXGIFactory2> factory;
            CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory));

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
            factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &swapChainDesc, nullptr,
                                            nullptr, &swapChain);

            swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain));
        }

        void init_command_queue() {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

            m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
        }

        void init_textures() { // assumes command list and pipeline state object are created

            OutputDebugStringA("\nBEGIN INIT TEXTURES--------\n");
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                             __uuidof(IWICImagingFactory),
                             reinterpret_cast<LPVOID *>(&m_wic_factory));

            UINT m_bmp_width, m_bmp_height;
            BYTE *m_bmp_bits;
            LoadBitmapFromFile(LR"(C:\Users\user157\Downloads\smile.png)", m_bmp_width,
                               m_bmp_height, &m_bmp_bits);

            smile_texture.init(m_device, m_bmp_width, m_bmp_height, m_bmp_bits,
                               const_heaps.get_cpu_handle(1));

        }
        
    public:
        painter() {}

        void init(HWND _hwnd) {
            hwnd = _hwnd;

            D3D12_RECT hwnd_rect;
            GetClientRect(hwnd, &hwnd_rect);
            width = hwnd_rect.right;
            height = hwnd_rect.bottom;

#if defined(_DEBUG)
            init_debug_layer();
#endif

            D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));

            init_command_queue();
            init_swap_chain();
            const_heaps.init(m_device, 2);
            // init_const_buffer_heap();
            {
                D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
                rtvHeapDesc.NumDescriptors = FrameCount;
                rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));


                m_rtvDescriptorSize =
                    m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
                    m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

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
                m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            m_commandAllocator[i].Get(), nullptr,
                                            IID_PPV_ARGS(&m_commandList[i]));

                m_commandList[i]->Close();
            }

            gpu_waiter.init(m_device);

            load_assets();
            init_textures();
        }

        void release() {}

        void resize(UINT _width, UINT _height) {
            width = _width;
            height = _height;
        }

        void paint() {


            double time = get_time();

            recalculate_matrix(time);

            HRESULT hr;

            m_commandAllocator[m_frameIndex]->Reset();
            m_commandList[m_frameIndex]->Reset(m_commandAllocator[m_frameIndex].Get(),
                                               m_pipelineState.Get());

            m_commandList[m_frameIndex]->SetGraphicsRootSignature(m_rootSignature.Get());

            ID3D12DescriptorHeap *pHeaps = const_heaps.get_heap_ptr();
            m_commandList[m_frameIndex]->SetDescriptorHeaps(1, &pHeaps);


            m_commandList[m_frameIndex]->SetGraphicsRootDescriptorTable(
                0, const_heaps.get_gpu_handle(0));
            m_commandList[m_frameIndex]->SetGraphicsRootDescriptorTable(
                1, const_heaps.get_gpu_handle(1));


            D3D12_VIEWPORT viewport = {
                .TopLeftX = 0.0f,
                .TopLeftY = 0.0f,
                .Width = static_cast<float>(
                    width), // aktualna szerokość obszaru roboczego okna (celu rend.)
                .Height = static_cast<float>(
                    height), // aktualna wysokość obszaru roboczego okna (celu rend.)
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


            constexpr static FLOAT blue[4] = {0, 0, 1, 1};
            constexpr static FLOAT yellow[4] = {1, 0, 1, 1};
            const FLOAT *color;
            if (static_cast<int>(time) % 2 == 0) {
                color = blue;
            } else {
                color = yellow;
            }
            m_commandList[m_frameIndex]->ClearRenderTargetView(m_rtvHandles[m_frameIndex], color, 0,
                                                               nullptr);


            m_commandList[m_frameIndex]->ClearDepthStencilView(
                depth_buffer.get_view(), D3D12_CLEAR_FLAG_STENCIL | D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0,
                0, nullptr);

            m_commandList[m_frameIndex]->IASetPrimitiveTopology(
                D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_commandList[m_frameIndex]->IASetVertexBuffers(0, 1, &cube_vertex_buffer.get_view());
            m_commandList[m_frameIndex]->DrawInstanced(cube_vertex_buffer.get_vertex_count(), 1, 0,
                                                       0);

            barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            m_commandList[m_frameIndex]->ResourceBarrier(1, &barrier);

            hr = m_commandList[m_frameIndex]->Close();
            if (hr != S_OK) {
                throw 1;
            }

            ID3D12CommandList *ppCommandLists[] = {m_commandList[m_frameIndex].Get()};
            m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

            hr = m_swapChain->Present(1, 0);

            if (hr != S_OK) {
                throw 1;
            }

            gpu_waiter.wait(m_commandQueue);

            m_frameIndex ^= 1;
        }
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {

    constexpr static TCHAR class_name[] = TEXT("my class");

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassEx(&wc)) {
        return 1;
    }

    HWND hwnd = CreateWindowEx(0,                                // window extra style
                               class_name,                       // window class
                               TEXT("Learn to Program Windows"), // window title
                               WS_OVERLAPPEDWINDOW,              // window style
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               CW_USEDEFAULT, // size and position
                               nullptr,       // parent window
                               nullptr,       // menu
                               hInstance,     // instance
                               nullptr        // application data
    );

    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};

    do {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message != WM_QUIT) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            InvalidateRect(hwnd, nullptr, false);
        }
    } while (msg.message != WM_QUIT);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static painter pnt;

    switch (uMsg) {
        case WM_CREATE:
            pnt.init(hwnd);

            break;
        case WM_SIZE:

            pnt.resize(LOWORD(lParam), HIWORD(lParam));

            break;
        case WM_DESTROY:

            pnt.release();

            PostQuitMessage(0);
            return 0;
        case WM_PAINT:

            pnt.paint();

            ValidateRect(hwnd, nullptr);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}