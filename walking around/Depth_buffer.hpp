#pragma once

#include "Windows_includes.hpp"

class Depth_buffer {
    private:
        ComPtr<ID3D12DescriptorHeap> m_depthBuffHeap;
        ComPtr<ID3D12Resource> m_depthBuffer;

        D3D12_CPU_DESCRIPTOR_HANDLE m_depthStencilView;

        void init_descriptor_heap(ComPtr<ID3D12Device> &device);

    public:
        void init(ComPtr<ID3D12Device> &device, UINT width, UINT height);

        D3D12_CPU_DESCRIPTOR_HANDLE &get_view();
};