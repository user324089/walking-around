#include "Const_and_texture_heap.hpp"

void Const_and_texture_heap::init(ComPtr<ID3D12Device> &device, unsigned int number) {
    D3D12_DESCRIPTOR_HEAP_DESC constBuffHeapDesc = {.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                                    .NumDescriptors = number,
                                                    .Flags =
                                                        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                                                    .NodeMask = 0};

    device->CreateDescriptorHeap(&constBuffHeapDesc, IID_PPV_ARGS(&m_constBuffHeap));

    increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    cpu_handle = m_constBuffHeap->GetCPUDescriptorHandleForHeapStart();
    gpu_handle = m_constBuffHeap->GetGPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE Const_and_texture_heap::get_cpu_handle(unsigned int index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = cpu_handle;
    handle.ptr += increment * index;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE Const_and_texture_heap::get_gpu_handle(unsigned int index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = gpu_handle;
    handle.ptr += increment * index;
    return handle;
}

ID3D12DescriptorHeap *Const_and_texture_heap::get_heap_ptr() {
    return m_constBuffHeap.Get();
}
