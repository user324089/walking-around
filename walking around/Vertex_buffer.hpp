#pragma once
#include "Windows_includes.hpp"

#include "Utility.hpp"
#include <vector>

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

            check_output(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                         IID_PPV_ARGS(&m_vertexBuffer)));

            void *vertex_memory;
            D3D12_RANGE zero_range = {.Begin = 0, .End = 0};
            check_output(m_vertexBuffer->Map(0, &zero_range, &vertex_memory));
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