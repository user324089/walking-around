#pragma once
#include "Windows_includes.hpp"
#include "Texture.hpp"


class Texture_loader {
    private:
        IWICImagingFactory *m_wic_factory;

        void LoadBitmapFromFile(PCWSTR uri, UINT &width, UINT &height, BYTE **ppBits);

    public:
        void init();

        Texture load_texture(ComPtr<ID3D12Device> &device, PCWSTR uri,
                             const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                             const D3D12_GPU_DESCRIPTOR_HANDLE &gpu_handle);
};