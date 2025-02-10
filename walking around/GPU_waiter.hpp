#pragma once
#include "Windows_includes.hpp"


class GPU_waiter {
    private:
        ComPtr<ID3D12Fence> m_fence;
        HANDLE m_fenceEvent;
        UINT64 m_fenceValue = 0;

    public:
        void init(ComPtr<ID3D12Device> &device);

        void wait(ComPtr<ID3D12CommandQueue> &command_queue);
};