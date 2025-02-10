#include "GPU_waiter.hpp"

void GPU_waiter::init(ComPtr<ID3D12Device> &device) {
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &m_fence);
}

void GPU_waiter::wait(ComPtr<ID3D12CommandQueue> &command_queue) {
    m_fenceValue++;

    command_queue->Signal(m_fence.Get(), m_fenceValue);
    m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);

    WaitForSingleObject(m_fenceEvent, INFINITE);
}
