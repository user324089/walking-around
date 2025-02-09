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
#include <fstream>
#include <string>
#include <algorithm>
#include <array>
#include <limits>

#include "pixel_shader.h"
#include "vertex_shader.h"
#include <map>
#include <set>


using namespace Microsoft::WRL;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

constexpr UINT BMP_PX_SIZE = 4;

struct vs_const_buffer_t {
        DirectX::XMFLOAT4X4 matWorld[10];
        DirectX::XMFLOAT4X4 matView;
        DirectX::XMFLOAT4X4 matProj;
        DirectX::XMFLOAT4 colLight, dirLight;
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

        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;

    public:
        void init(ComPtr<ID3D12Device> &device, unsigned int width, unsigned int height, BYTE *data,
                  const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
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
            device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator.Get(),
                                      nullptr, IID_PPV_ARGS(&command_list));


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
                                                   .SlicePitch = width * height * BMP_PX_SIZE};
            // ... ID3D12GraphicsCommandList::Reset() - to dlatego lista
            // poleceń i obiekt stanu potoku muszą być wcześniej utworzone

            // command_list->Reset(command_allocator.Get(), nullptr); // TODO is this reset
            // necessary

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
            device->CreateShaderResourceView(texture_resource.Get(), &srv_desc, cpu_handle);

            GPU_waiter gpu_waiter;
            gpu_waiter.init(device);
            gpu_waiter.wait(command_queue);
        }

        void use(ComPtr<ID3D12GraphicsCommandList> &command_list, unsigned int arg_num) {
            command_list->SetGraphicsRootDescriptorTable(arg_num, gpu_handle);
        }
};

class Texture_loader {
    private:
        IWICImagingFactory *m_wic_factory;

        void LoadBitmapFromFile(PCWSTR uri, UINT &width, UINT &height, BYTE **ppBits) {
            ComPtr<IWICBitmapDecoder> pDecoder = nullptr;
            ComPtr<IWICBitmapFrameDecode> pSource = nullptr;
            ComPtr<IWICFormatConverter> pConverter = nullptr;

            m_wic_factory->CreateDecoderFromFilename(
                uri, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, pDecoder.GetAddressOf());


            pDecoder->GetFrame(0, pSource.GetAddressOf());

            m_wic_factory->CreateFormatConverter(pConverter.GetAddressOf());


            pConverter->Initialize(pSource.Get(), GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0f,
                                   WICBitmapPaletteTypeMedianCut);

            pConverter->GetSize(&width, &height);


            *ppBits = new BYTE[4 * width * height];

            pConverter->CopyPixels(nullptr, 4 * width, 4 * width * height, *ppBits);
        }

    public:
        void init() {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                             __uuidof(IWICImagingFactory),
                             reinterpret_cast<LPVOID *>(&m_wic_factory));
        }

        Texture load_texture(ComPtr<ID3D12Device> &device, PCWSTR uri,
                             const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                             const D3D12_GPU_DESCRIPTOR_HANDLE &gpu_handle) {
            UINT m_bmp_width, m_bmp_height;
            BYTE *m_bmp_bits;
            LoadBitmapFromFile(uri, m_bmp_width, m_bmp_height, &m_bmp_bits);

            Texture result;
            result.init(device, m_bmp_width, m_bmp_height, m_bmp_bits, cpu_handle, gpu_handle);
            return result;
        }
};

class Const_buffer {
    private:
        ComPtr<ID3D12Resource> m_constBuffer;
        void *memory;

        unsigned int find_larger_256_divisible(unsigned int size) {
            unsigned int masked_left = size & (~255);
            bool masked_right = size & 255;
            return masked_left + masked_right * 256;
        }

    public:

        void init(ComPtr<ID3D12Device> &device, unsigned int size,
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

        void *data() {
            return memory;
        }
};

class Id_giver {
    private:
        unsigned int given_id = 0;
        std::map<std::string, unsigned int> str_to_id;

    public:
        unsigned int get_id(const std::string &str) {
            auto found = str_to_id.find(str);
            if (found == str_to_id.end()) {
                return (str_to_id[str] = given_id++);
            }
            return found->second;
        }

        constexpr static unsigned int no_id = (std::numeric_limits<unsigned int>::max)();

        void write() {
            std::stringstream s;
            s << "\n------------IDs----------\n";
            for (auto [a, b] : str_to_id) {
                s << a << " " << b << "\n";
            }
            OutputDebugStringA(s.str().c_str());
        }
};

class Object {
    private:
        struct vertex_t {
            public:
                FLOAT position[3];
                FLOAT normal[3];
                FLOAT tex_coord[2];
                UINT mat_index;
        };

        Texture texture;
        Vertex_buffer vertex_buffer;

        std::map<unsigned int, std::array<float, 3>> id_to_pivot_point;

    public:

        const std::array<float, 3> &get_pivot(unsigned int id) {
            return id_to_pivot_point[id];
        }

        void init(ComPtr<ID3D12Device> &device, Texture_loader &texture_loader,
                  PCWSTR texture_filename, PCWSTR obj_filename,
                  const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                  const D3D12_GPU_DESCRIPTOR_HANDLE &gpu_handle, Id_giver &id_giver) {
            texture = texture_loader.load_texture(device, texture_filename, cpu_handle, gpu_handle);

            std::vector<std::array<float, 3>> vertex_coords;
            std::vector<unsigned int> vertex_groups;
            std::vector<bool> is_pivot;
            std::vector<std::array<float, 3>> normals;
            std::vector<std::array<float, 2>> tex_coords;


            std::string current_object_name;
            std::string current_group_name = "off";

            unsigned int off_gid = Id_giver::no_id;

            std::ifstream obj_file(obj_filename);
            std::string current_line;
            while (std::getline(obj_file, current_line)) {
                std::replace(current_line.begin(), current_line.end(), '/', ' ');
                std::stringstream line_stream(current_line);
                std::string current_token;
                line_stream >> current_token;
                if (current_token == "v") {
                    std::array<float, 3> coords;
                    line_stream >> coords[0] >> coords[1] >> coords[2];
                    vertex_coords.push_back(coords);
                    vertex_groups.push_back(Id_giver::no_id); // unused id
                    is_pivot.push_back(false);
                } else if (current_token == "vt") {
                    std::array<float, 2> coords;
                    line_stream >> coords[0] >> coords[1];
                    tex_coords.push_back(coords);
                } else if (current_token == "vn") {
                    std::array<float, 3> coords;
                    line_stream >> coords[0] >> coords[1] >> coords[2];
                    normals.push_back(coords);
                } else if (current_token == "f") {
                    unsigned int current_gid =
                        id_giver.get_id(current_object_name + "." + current_group_name);


                    for (unsigned int i = 0; i < 3; i++) {
                        vertex_t current_vertex;
                        unsigned int v_index, vt_index, vn_index;
                        line_stream >> v_index >> vt_index >> vn_index;
                        if (vertex_groups[v_index - 1] != Id_giver::no_id
                            && vertex_groups[v_index - 1] != current_gid) {
                            is_pivot[v_index - 1] = true;
                        }
                        if (vertex_groups[v_index - 1] == Id_giver::no_id
                            || current_gid != off_gid) {
                            vertex_groups[v_index - 1] = current_gid;
                        }
                    }
                } else if (current_token == "o") {
                    line_stream >> current_object_name;
                    off_gid = id_giver.get_id(current_object_name + ".off");
                } else if (current_token == "g") {
                    line_stream >> current_group_name;
                }
            }
            obj_file.clear();
            obj_file.seekg(0, std::ios::beg);

            std::vector<vertex_t> vertices;

            while (std::getline(obj_file, current_line)) {

                std::replace(current_line.begin(), current_line.end(), '/', ' ');
                std::stringstream line_stream(current_line);
                std::string current_token;
                line_stream >> current_token;

                if (current_token == "f") {
                    for (unsigned int i = 0; i < 3; i++) {
                        vertex_t current_vertex;
                        unsigned int v_index, vt_index, vn_index;
                        line_stream >> v_index >> vt_index >> vn_index;
                        std::array<float, 3> &coords = vertex_coords[v_index - 1];
                        std::array<float, 3> &normal = normals[vn_index - 1];
                        std::array<float, 2> &tex = tex_coords[vt_index - 1];
                        std::copy(coords.begin(), coords.end(), current_vertex.position);
                        std::copy(normal.begin(), normal.end(), current_vertex.normal);
                        std::copy(tex.begin(), tex.end(), current_vertex.tex_coord);

                        current_vertex.mat_index = vertex_groups[v_index - 1];

                        vertices.push_back(current_vertex);
                    }
                }
            }
            vertex_buffer.init(device, vertices);

            std::stringstream s;
            s << "------------------POINTS\n";
            std::map<unsigned int, unsigned int> id_to_num_pivot_points;
            for (unsigned int i = 0; i < vertex_coords.size(); i++) {

                if (vertex_groups[i] == id_giver.get_id("person.left_leg")) {
                    s << vertex_coords[i][1] << " ";
                    if (!is_pivot[i]) {
                        s << "is not pivot\n";
                    } else {
                        s << "is pivot\n";
                    }
                }
                if (!is_pivot[i]) {
                    continue;
                }
                id_to_num_pivot_points[vertex_groups[i]]++;
                std::array<float, 3> &sum_array = id_to_pivot_point[vertex_groups[i]];
                for (unsigned int j = 0; j < 3; j++) {
                    sum_array[j] += vertex_coords[i][j];
                }
            }
            OutputDebugStringA(s.str().c_str());

            for (const auto &[id, num] : id_to_num_pivot_points) {
                std::array<float, 3> &sum_array = id_to_pivot_point[id];
                for (unsigned int j = 0; j < 3; j++) {
                    sum_array[j] /= num;
                }
            }
        }

        void draw(ComPtr<ID3D12GraphicsCommandList> &command_list) {
            texture.use(command_list, 1); // 1 is the texture argument number
            command_list->IASetVertexBuffers(0, 1, &vertex_buffer.get_view());
            command_list->DrawInstanced(vertex_buffer.get_vertex_count(), 1, 0, 0);
        }
};

class Player {
    private:
        float time = 0;
        float x = 0, z = 0;
        constexpr static float y = 3, camera_back_dist = 3;
        constexpr static float viewing_down_angle = -0.8;
        float angle = 0;
        float velocity_x_forward = 0, velocity_x_backward = 0, velocity_z_forward = 0,
              velocity_z_backward = 0, angular_velocity_left = 0, angular_velocity_right = 0;

        float hand_pivot_y = 0, leg_pivot_y = 0;

        Object person_obj;

        unsigned int off_mat_id = 0, left_leg_mat_id = 0, right_leg_mat_id = 0,
                     left_hand_mat_id = 0, right_hand_mat_id = 0;

        void fill_view_matrix(vs_const_buffer_t &buffer) {

            DirectX::XMMATRIX view;
            view = DirectX::XMMatrixMultiply(DirectX::XMMatrixTranslation(-x, -y, -z),
                                             DirectX::XMMatrixRotationY(-angle));
            view = DirectX::XMMatrixMultiply(view,
                                             DirectX::XMMatrixTranslation(0, 0, camera_back_dist));
            view = DirectX::XMMatrixMultiply(view, DirectX::XMMatrixRotationX(viewing_down_angle));

            view = XMMatrixTranspose(view);
            XMStoreFloat4x4(&buffer.matView, view);
        }

        DirectX::XMMATRIX rotate_by_y_pivot(float pivot, float rotation) {
            DirectX::XMMATRIX rotation_matrix = DirectX::XMMatrixTranslation(0, -pivot, 0);
            rotation_matrix =
                DirectX::XMMatrixMultiply(rotation_matrix, DirectX::XMMatrixRotationX(rotation));
            rotation_matrix = DirectX::XMMatrixMultiply(rotation_matrix,
                                                        DirectX::XMMatrixTranslation(0, pivot, 0));
            return rotation_matrix;
        }

        void fill_person_matrices(vs_const_buffer_t &buffer) {
            DirectX::XMMATRIX off;

            off = DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationY(angle),
                                            DirectX::XMMatrixTranslation(x, 0, z));

            DirectX::XMMATRIX left_hand_matrix = rotate_by_y_pivot(hand_pivot_y, std::sin(time));
            left_hand_matrix = DirectX::XMMatrixMultiply(left_hand_matrix, off);

            DirectX::XMMATRIX right_hand_matrix = rotate_by_y_pivot(hand_pivot_y, std::sin(time));
            right_hand_matrix = DirectX::XMMatrixMultiply(right_hand_matrix, off);

            DirectX::XMMATRIX left_leg_matrix = rotate_by_y_pivot(leg_pivot_y, std::sin(time));
            left_leg_matrix = DirectX::XMMatrixMultiply(left_leg_matrix, off);

            DirectX::XMMATRIX right_leg_matrix = rotate_by_y_pivot(leg_pivot_y, std::sin(time));
            right_leg_matrix = DirectX::XMMatrixMultiply(right_leg_matrix, off);

            off = XMMatrixTranspose(off);
            left_leg_matrix = XMMatrixTranspose(left_leg_matrix);
            right_leg_matrix = XMMatrixTranspose(right_leg_matrix);
            left_hand_matrix = XMMatrixTranspose(left_hand_matrix);
            right_hand_matrix = XMMatrixTranspose(right_hand_matrix);
            XMStoreFloat4x4(&buffer.matWorld[off_mat_id], off);
            XMStoreFloat4x4(&buffer.matWorld[left_leg_mat_id], left_leg_matrix);
            XMStoreFloat4x4(&buffer.matWorld[right_leg_mat_id], right_leg_matrix);
            XMStoreFloat4x4(&buffer.matWorld[left_hand_mat_id], left_hand_matrix);
            XMStoreFloat4x4(&buffer.matWorld[right_hand_mat_id], right_hand_matrix);
        }

    public:
        void init(ComPtr<ID3D12Device> &device, Texture_loader &texture_loader,
                  const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                  const D3D12_GPU_DESCRIPTOR_HANDLE &gpu_handle, Id_giver &id_giver) {
            person_obj.init(device, texture_loader, LR"(resources/person.png)",
                            LR"(resources/person.wobj)", cpu_handle, gpu_handle, id_giver);

            off_mat_id = id_giver.get_id("person.off");
            left_leg_mat_id = id_giver.get_id("person.left_leg");
            right_leg_mat_id = id_giver.get_id("person.right_leg");
            left_hand_mat_id = id_giver.get_id("person.left_hand");
            right_hand_mat_id = id_giver.get_id("person.right_hand");

            hand_pivot_y = person_obj.get_pivot(right_hand_mat_id)[1];
            leg_pivot_y = person_obj.get_pivot(right_leg_mat_id)[1];

            std::stringstream s;
            s << "--------------PIVOTS\nleft leg pivot: " << hand_pivot_y << " " << leg_pivot_y
              << "\n";
            OutputDebugStringA(s.str().c_str());
        }

        void key_down(WPARAM key_code) {
            switch (key_code) {
                case 'W':
                    velocity_z_forward = 1;
                    return;
                case 'S':
                    velocity_z_backward = 1;
                    return;
                case 'A':
                    velocity_x_backward = 1;
                    return;
                case 'D':
                    velocity_x_forward = 1;
                    return;
                case VK_LEFT:
                    angular_velocity_left = 1;
                    return;
                case VK_RIGHT:
                    angular_velocity_right = 1;
                    return;
            }
        }

        void key_up(WPARAM key_code) {
            switch (key_code) {
                case 'W':
                    velocity_z_forward = 0;
                    return;
                case 'S':
                    velocity_z_backward = 0;
                    return;
                case 'A':
                    velocity_x_backward = 0;
                    return;
                case 'D':
                    velocity_x_forward = 0;
                    return;
                case VK_LEFT:
                    angular_velocity_left = 0;
                    return;
                case VK_RIGHT:
                    angular_velocity_right = 0;
                    return;
            }
        }

        void update(float delta_time) {

            time += delta_time;

            float velocity_z = velocity_z_forward - velocity_z_backward;
            float velocity_x = velocity_x_forward - velocity_x_backward;
            float angular_velocity = angular_velocity_right - angular_velocity_left;

            std::stringstream s;
            s << "IN UPDATE: " << delta_time << " " << angle << "\n";
            OutputDebugStringA(s.str().c_str());

            float sin_angle = std::sin(angle), cos_angle = std::cos(angle);
            float forward_x = sin_angle, forward_z = cos_angle;
            float left_x = cos_angle, left_z = -sin_angle;

            x += (left_x * velocity_x + forward_x * velocity_z) * delta_time;
            z += (forward_z * velocity_z + left_z * velocity_x) * delta_time;

            angle += angular_velocity * delta_time;
            if (angle > 2 * std::numbers::pi_v<float>) {
                angle -= 2 * std::numbers::pi_v<float>;
            } else if (angle < -2 * std::numbers::pi_v<float>) {
                angle += 2 * std::numbers::pi_v<float>;
            }
        }

        void fill_const_buffer(vs_const_buffer_t &buffer) {
            fill_view_matrix(buffer);
            fill_person_matrices(buffer);
        }

        void draw(ComPtr<ID3D12GraphicsCommandList> &command_list) {
            person_obj.draw(command_list);
        }
};

class painter {
    private:

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

        // Vertex_buffer cube_vertex_buffer;

        // ComPtr<ID3D12Resource> m_constBuffer;
        // void *m_const_buffer_memory;
        Const_buffer matrix_buffer;

        GPU_waiter gpu_waiter;

        UINT m_rtvDescriptorSize;
        UINT m_frameIndex = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[FrameCount];

        std::chrono::high_resolution_clock::time_point start_point =
            std::chrono::high_resolution_clock::now();
        std::chrono::high_resolution_clock::time_point prev_time_point =
            std::chrono::high_resolution_clock::now();

        Texture_loader texture_loader;
        Texture smile_texture;

        Object house_object;
        Id_giver object_id_giver;

        Player player;

        double get_delta_time() {
            std::chrono::high_resolution_clock::time_point now_point =
                std::chrono::high_resolution_clock::now();
            double microseconds_passed =
                std::chrono::duration_cast<std::chrono::microseconds>(now_point - prev_time_point)
                    .count();
            prev_time_point = now_point;
            return microseconds_passed / 1'000'000;
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

            DirectX::XMMATRIX world, proj;

            world = DirectX::XMMatrixMultiply(
                DirectX::XMMatrixRotationY(2.5f * angle),
                DirectX::XMMatrixRotationX(static_cast<FLOAT>(sin(angle)) / 2.0f));

            // view = DirectX::XMMatrixTranslation(0.0f, 0.0f, 4.0f);

            proj = DirectX::XMMatrixPerspectiveFovLH(45.0f, width / height, 0.1f, 100.0f);


            world = XMMatrixTranspose(world);
            proj = XMMatrixTranspose(proj);

            DirectX::XMMATRIX alternative = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
            alternative = XMMatrixTranspose(alternative);

            vs_const_buffer_t buff;

            // XMStoreFloat4x4(&buff.matWorld[0], world);
            for (unsigned int i = 0; i < 10; i++) {
                XMStoreFloat4x4(&buff.matWorld[i], alternative);
            }

            player.fill_const_buffer(buff);

            // XMStoreFloat4x4(&buff.matWorld[object_id_giver.get_id("person.right_leg")], world);


            XMStoreFloat4x4(&buff.matProj, proj);

            buff.colLight = {1.0f, 1.0f, 1.0f, 1.0f};
            buff.dirLight = {1, 1, 1, 0.0f};

            memcpy(matrix_buffer.data(), &buff, sizeof(buff));
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

        void load_assets() {
            set_root_signature();
            create_graphics_pipeline_state();
            // cube_vertex_buffer.init(m_device, gen_data());
            house_object.init(m_device, texture_loader, LR"(resources/house.png)",
                              LR"(resources/house.wobj)", const_heaps.get_cpu_handle(1),
                              const_heaps.get_gpu_handle(1), object_id_giver);
            player.init(m_device, texture_loader, const_heaps.get_cpu_handle(2),
                        const_heaps.get_gpu_handle(2), object_id_giver);
            object_id_giver.write();
            matrix_buffer.init(m_device, sizeof(vs_const_buffer_t), const_heaps.get_cpu_handle(0));
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
            const_heaps.init(m_device, 3);
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

            texture_loader.init();
            // init_textures();
            load_assets();
        }

        void release() {}

        void resize(UINT _width, UINT _height) {
            width = _width;
            height = _height;
        }

        void update() {
            float delta_time = get_delta_time();
            player.update(delta_time);
        }

        void key_down(WPARAM key_code, LPARAM flags) {
            if (!(flags & KF_REPEAT)) {
                player.key_down(key_code);
            }
        }

        void key_up(WPARAM key_code, LPARAM flags) {
            player.key_up(key_code);
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

            /*
            m_commandList[m_frameIndex]->SetGraphicsRootDescriptorTable(
                1, const_heaps.get_gpu_handle(2));
                */


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


            constexpr static FLOAT yellow[4] = {1, 0, 1, 1};
            m_commandList[m_frameIndex]->ClearRenderTargetView(m_rtvHandles[m_frameIndex], yellow,
                                                               0, nullptr);


            m_commandList[m_frameIndex]->ClearDepthStencilView(
                depth_buffer.get_view(), D3D12_CLEAR_FLAG_STENCIL | D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0,
                0, nullptr);

            m_commandList[m_frameIndex]->IASetPrimitiveTopology(
                D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


            /*
            m_commandList[m_frameIndex]->IASetVertexBuffers(0, 1,
            &cube_vertex_buffer.get_view());
            m_commandList[m_frameIndex]->DrawInstanced(cube_vertex_buffer.get_vertex_count(),
            1, 0, 0);
                                                       */
            house_object.draw(m_commandList[m_frameIndex]);
            player.draw(m_commandList[m_frameIndex]);

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
            msg.message = WM_USER;
            DispatchMessage(&msg);
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
        case WM_KEYDOWN:
            pnt.key_down(wParam, lParam);
            return 0;
        case WM_KEYUP:
            pnt.key_up(wParam, lParam);
            return 0;

        case WM_PAINT:

            pnt.paint();

            ValidateRect(hwnd, nullptr);
            return 0;

        case WM_USER:
            pnt.update();
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}