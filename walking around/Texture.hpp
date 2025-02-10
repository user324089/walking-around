#pragma once
#include "Windows_includes.hpp"
#include "Utility.hpp"


constexpr UINT BMP_PX_SIZE = 4;

class Texture {
    private:
        ComPtr<ID3D12Resource> texture_resource;

        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;


    public:
        void init(ComPtr<ID3D12Device> &device, unsigned int width, unsigned int height, BYTE *data,
                  const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                  const D3D12_GPU_DESCRIPTOR_HANDLE &_gpu_handle);

        void use(ComPtr<ID3D12GraphicsCommandList> &command_list, unsigned int arg_num);
};