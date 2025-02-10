#pragma once
#include "Windows_includes.hpp"
#include "Texture.hpp"
#include "Texture_loader.hpp"
#include "Id_giver.hpp"
#include "Vertex_buffer.hpp"
#include <map>
#include <array>

class Object {
    private:
        struct vertex_t {
            public:
                FLOAT position[3];
                FLOAT normal[3];
                FLOAT tex_coord[2];
                UINT mat_index;
        };

        Texture texture;
        Vertex_buffer vertex_buffer;

        std::map<unsigned int, std::array<float, 3>> id_to_pivot_point;

    public:

        const std::array<float, 3> &get_pivot(unsigned int id);

        void init(ComPtr<ID3D12Device> &device, Texture_loader &texture_loader,
                  PCWSTR texture_filename, PCWSTR obj_filename,
                  const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                  const D3D12_GPU_DESCRIPTOR_HANDLE &gpu_handle, Id_giver &id_giver);

        void draw(ComPtr<ID3D12GraphicsCommandList> &command_list);
};