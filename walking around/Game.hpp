#pragma once
#include "Windows_includes.hpp"

#include "Depth_buffer.hpp"
#include "Vertex_buffer.hpp"
#include "GPU_waiter.hpp"
#include "Const_and_texture_heap.hpp"
#include "Texture.hpp"
#include "Texture_loader.hpp"
#include "Const_buffer.hpp"
#include "Id_giver.hpp"
#include "Object.hpp"
#include "Player.hpp"


#include "pixel_shader.h"
#include "vertex_shader.h"

#include <chrono>


class Game {
    private:

        UINT width = 0, height = 0;
        HWND hwnd = 0;

        constexpr static UINT FrameCount = 2;

        ComPtr<ID3D12Device> m_device;
        ComPtr<ID3D12CommandQueue> m_commandQueue;
        ComPtr<IDXGISwapChain3> m_swapChain;
        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
        ComPtr<ID3D12CommandAllocator> m_commandAllocator[FrameCount];
        ComPtr<ID3D12GraphicsCommandList> m_commandList[FrameCount];

        Depth_buffer depth_buffer;

        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_pipelineState;

        Const_and_texture_heap const_heaps;

        Const_buffer matrix_buffer;

        GPU_waiter gpu_waiter;

        UINT m_rtvDescriptorSize;
        UINT m_frameIndex = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[FrameCount];

        std::chrono::high_resolution_clock::time_point start_point =
            std::chrono::high_resolution_clock::now();
        std::chrono::high_resolution_clock::time_point prev_time_point =
            std::chrono::high_resolution_clock::now();

        Texture_loader texture_loader;
        Texture smile_texture;

        Object house_object;
        Id_giver object_id_giver;

        Player player;

        double get_delta_time();

        double get_time();

        void recalculate_matrix(double angle);

        void set_root_signature();

        void create_graphics_pipeline_state();

        void init_debug_layer();

        void init_swap_chain();

        void init_command_queue();

    public:
        Game();

        void init(HWND _hwnd);

        void release();

        void resize(UINT _width, UINT _height);

        void update();

        void key_down(WPARAM key_code, LPARAM flags);

        void key_up(WPARAM key_code, LPARAM flags);

        void paint();
};