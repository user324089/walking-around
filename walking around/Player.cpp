#include "Player.hpp"

void Player::fill_view_matrix(Shader_const_buffer &buffer) {

    DirectX::XMMATRIX view;
    view = DirectX::XMMatrixMultiply(DirectX::XMMatrixTranslation(-x, -y, -z),
                                     DirectX::XMMatrixRotationY(-angle));
    view = DirectX::XMMatrixMultiply(view, DirectX::XMMatrixTranslation(0, 0, camera_back_dist));
    view = DirectX::XMMatrixMultiply(view, DirectX::XMMatrixRotationX(viewing_down_angle));

    view = XMMatrixTranspose(view);
    XMStoreFloat4x4(&buffer.matView, view);
}

DirectX::XMMATRIX Player::rotate_by_y_pivot(float pivot, float rotation) {
    DirectX::XMMATRIX rotation_matrix = DirectX::XMMatrixTranslation(0, -pivot, 0);
    rotation_matrix =
        DirectX::XMMatrixMultiply(rotation_matrix, DirectX::XMMatrixRotationX(rotation));
    rotation_matrix =
        DirectX::XMMatrixMultiply(rotation_matrix, DirectX::XMMatrixTranslation(0, pivot, 0));
    return rotation_matrix;
}

void Player::fill_person_matrices(Shader_const_buffer &buffer) {
    DirectX::XMMATRIX off;

    off = DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationY(angle),
                                    DirectX::XMMatrixTranslation(x, 0, z));

    DirectX::XMMATRIX left_hand_matrix = rotate_by_y_pivot(hand_pivot_y, limb_angle);
    left_hand_matrix = DirectX::XMMatrixMultiply(left_hand_matrix, off);

    DirectX::XMMATRIX right_hand_matrix = rotate_by_y_pivot(hand_pivot_y, -limb_angle);
    right_hand_matrix = DirectX::XMMatrixMultiply(right_hand_matrix, off);

    DirectX::XMMATRIX left_leg_matrix = rotate_by_y_pivot(leg_pivot_y, -limb_angle);
    left_leg_matrix = DirectX::XMMatrixMultiply(left_leg_matrix, off);

    DirectX::XMMATRIX right_leg_matrix = rotate_by_y_pivot(leg_pivot_y, limb_angle);
    right_leg_matrix = DirectX::XMMatrixMultiply(right_leg_matrix, off);

    off = XMMatrixTranspose(off);
    left_leg_matrix = XMMatrixTranspose(left_leg_matrix);
    right_leg_matrix = XMMatrixTranspose(right_leg_matrix);
    left_hand_matrix = XMMatrixTranspose(left_hand_matrix);
    right_hand_matrix = XMMatrixTranspose(right_hand_matrix);
    XMStoreFloat4x4(&buffer.matWorld[off_mat_id], off);
    XMStoreFloat4x4(&buffer.matWorld[left_leg_mat_id], left_leg_matrix);
    XMStoreFloat4x4(&buffer.matWorld[right_leg_mat_id], right_leg_matrix);
    XMStoreFloat4x4(&buffer.matWorld[left_hand_mat_id], left_hand_matrix);
    XMStoreFloat4x4(&buffer.matWorld[right_hand_mat_id], right_hand_matrix);
}

float Player::limb_angle_function(float current_limb_time) {
    float discarded_val;
    float mod_time = 2 * std::modf((current_limb_time + 1.5f) / 2, &discarded_val);
    return 2 * std::abs(mod_time - 1) - 1;
}

float Player::limb_angle_function_inv(float current_limb_angle) {
    if (current_limb_angle >= 0) {
        return 1 - current_limb_angle / 2;
    }
    return 2 + current_limb_angle / 2;
}

void Player::init(ComPtr<ID3D12Device> &device, Texture_loader &texture_loader,
                         const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                         const D3D12_GPU_DESCRIPTOR_HANDLE &gpu_handle, Id_giver &id_giver) {
    person_obj.init(device, texture_loader, LR"(resources/person.png)", LR"(resources/person.wobj)",
                    cpu_handle, gpu_handle, id_giver);

    off_mat_id = id_giver.get_id("person.off");
    left_leg_mat_id = id_giver.get_id("person.left_leg");
    right_leg_mat_id = id_giver.get_id("person.right_leg");
    left_hand_mat_id = id_giver.get_id("person.left_hand");
    right_hand_mat_id = id_giver.get_id("person.right_hand");

    hand_pivot_y = person_obj.get_pivot(right_hand_mat_id)[1];
    leg_pivot_y = person_obj.get_pivot(right_leg_mat_id)[1];
}

void Player::key_down(WPARAM key_code) {
    switch (key_code) {
        case 'W':
            velocity_z_forward = 1;
            return;
        case 'S':
            velocity_z_backward = 1;
            return;
        case 'A':
            angular_velocity_left = 1;
            return;
        case 'D':
            angular_velocity_right = 1;
            return;
        case VK_LEFT:
            velocity_x_backward = 1;
            return;
        case VK_RIGHT:
            velocity_x_forward = 1;
            return;
    }
}

void Player::key_up(WPARAM key_code) {
    switch (key_code) {
        case 'W':
            velocity_z_forward = 0;
            return;
        case 'S':
            velocity_z_backward = 0;
            return;
        case 'A':
            angular_velocity_left = 0;
            return;
        case 'D':
            angular_velocity_right = 0;
            return;
        case VK_LEFT:
            velocity_x_backward = 0;
            return;
        case VK_RIGHT:
            velocity_x_forward = 0;
            return;
    }
}

void Player::update(float delta_time) {

    time += delta_time;

    float velocity_z = velocity_z_forward - velocity_z_backward;
    float velocity_x = 0; // velocity_x_forward - velocity_x_backward; uncomment to move sideways
    float angular_velocity = angular_velocity_right - angular_velocity_left;


    float sin_angle = std::sin(angle), cos_angle = std::cos(angle);
    float forward_x = sin_angle, forward_z = cos_angle;
    float left_x = cos_angle, left_z = -sin_angle;

    x += (left_x * velocity_x + forward_x * velocity_z) * delta_time;
    z += (forward_z * velocity_z + left_z * velocity_x) * delta_time;

    angle += angular_velocity * delta_time;
    if (angle > 2 * std::numbers::pi_v<float>) {
        angle -= 2 * std::numbers::pi_v<float>;
    } else if (angle < -2 * std::numbers::pi_v<float>) {
        angle += 2 * std::numbers::pi_v<float>;
    }


    if (velocity_z != 0) {
        limb_time += delta_time;
        limb_angle = limb_angle_function(limb_time);
    } else {
        if (limb_angle > 0) {
            limb_angle -= 2 * delta_time;
            limb_angle = (std::max)(limb_angle, 0.0f);
        } else {
            limb_angle += 2 * delta_time;
            limb_angle = (std::min)(limb_angle, 0.0f);
        }
        limb_time = limb_angle_function_inv(limb_angle);
    }
}

void Player::fill_const_buffer(Shader_const_buffer &buffer) {
    fill_view_matrix(buffer);
    fill_person_matrices(buffer);
}

void Player::draw(ComPtr<ID3D12GraphicsCommandList> &command_list) {
    person_obj.draw(command_list);
}
