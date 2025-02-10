#pragma once
#include "Windows_includes.hpp"
#include "Object.hpp"
#include "Shader_const_buffer.hpp"


#include <numbers>
#include <cmath>


class Player {
    private:
        float time = 0;
        float x = 0, z = 0;
        constexpr static float y = 5, camera_back_dist = 3;
        constexpr static float viewing_down_angle = -0.6;
        constexpr static float movement_speed = 5;
        float angle = 0;
        float velocity_x_forward = 0, velocity_x_backward = 0, velocity_z_forward = 0,
              velocity_z_backward = 0, angular_velocity_left = 0, angular_velocity_right = 0;

        float hand_pivot_y = 0, leg_pivot_y = 0;

        float limb_angle = 0, limb_time = 0;

        Object person_obj;

        unsigned int off_mat_id = 0, left_leg_mat_id = 0, right_leg_mat_id = 0,
                     left_hand_mat_id = 0, right_hand_mat_id = 0;

        void fill_view_matrix(Shader_const_buffer &buffer);

        DirectX::XMMATRIX rotate_by_y_pivot(float pivot, float rotation);

        void fill_person_matrices(Shader_const_buffer &buffer);

        float limb_angle_function(float current_limb_time);

        float limb_angle_function_inv(float current_limb_angle);

    public:
        void init(ComPtr<ID3D12Device> &device, Texture_loader &texture_loader,
                  const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                  const D3D12_GPU_DESCRIPTOR_HANDLE &gpu_handle, Id_giver &id_giver);

        void key_down(WPARAM key_code);

        void key_up(WPARAM key_code);

        void update(float delta_time);

        void fill_const_buffer(Shader_const_buffer &buffer);

        void draw(ComPtr<ID3D12GraphicsCommandList> &command_list);
};