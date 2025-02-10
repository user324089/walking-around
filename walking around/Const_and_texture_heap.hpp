#pragma once
#include "Windows_includes.hpp"

class Const_and_texture_heap {
    private:

        ComPtr<ID3D12DescriptorHeap> m_constBuffHeap;
        unsigned int increment = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = {};

    public:
        void init(ComPtr<ID3D12Device> &device, unsigned int number);

        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(unsigned int index);

        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(unsigned int index);

        ID3D12DescriptorHeap *get_heap_ptr();
};