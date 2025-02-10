#pragma once
#include "Windows_includes.hpp"


class Const_buffer {
    private:
        ComPtr<ID3D12Resource> m_constBuffer;
        void *memory;

        unsigned int find_larger_256_divisible(unsigned int size);

    public:

        void init(ComPtr<ID3D12Device> &device, unsigned int size,
                  const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle);

        void *data();
};