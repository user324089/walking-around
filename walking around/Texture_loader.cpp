#include "Texture_loader.hpp"

void Texture_loader::LoadBitmapFromFile(PCWSTR uri, UINT &width, UINT &height, BYTE **ppBits) {
    ComPtr<IWICBitmapDecoder> decoder = nullptr;
    ComPtr<IWICBitmapFrameDecode> source = nullptr;
    ComPtr<IWICFormatConverter> converter = nullptr;

    check_output(m_wic_factory->CreateDecoderFromFilename(
        uri, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()));


    check_output(decoder->GetFrame(0, source.GetAddressOf()));

    check_output(m_wic_factory->CreateFormatConverter(converter.GetAddressOf()));


    check_output(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppRGBA,
                                       WICBitmapDitherTypeNone, nullptr, 0.0f,
                                       WICBitmapPaletteTypeMedianCut));

    check_output(converter->GetSize(&width, &height));


    *ppBits = new BYTE[4 * width * height];

    check_output(converter->CopyPixels(nullptr, 4 * width, 4 * width * height, *ppBits));
}

void Texture_loader::init() {
    check_output(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    check_output(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  __uuidof(IWICImagingFactory),
                                  reinterpret_cast<LPVOID *>(&m_wic_factory)));
}

Texture Texture_loader::load_texture(ComPtr<ID3D12Device> &device, PCWSTR uri,
                                     const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                                     const D3D12_GPU_DESCRIPTOR_HANDLE &gpu_handle) {
    UINT m_bmp_width, m_bmp_height;
    BYTE *m_bmp_bits;
    LoadBitmapFromFile(uri, m_bmp_width, m_bmp_height, &m_bmp_bits);

    Texture result;
    result.init(device, m_bmp_width, m_bmp_height, m_bmp_bits, cpu_handle, gpu_handle);
    return result;
}
