#include "Object.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>

const std::array<float, 3> &Object::get_pivot(unsigned int id) {
    return id_to_pivot_point[id];
}

void Object::init(ComPtr<ID3D12Device> &device, Texture_loader &texture_loader,
                         PCWSTR texture_filename, PCWSTR obj_filename,
                         const D3D12_CPU_DESCRIPTOR_HANDLE &cpu_handle,
                         const D3D12_GPU_DESCRIPTOR_HANDLE &gpu_handle, Id_giver &id_giver) {
    texture = texture_loader.load_texture(device, texture_filename, cpu_handle, gpu_handle);

    std::vector<std::array<float, 3>> vertex_coords;
    std::vector<unsigned int> vertex_groups;
    std::vector<bool> is_pivot;
    std::vector<std::array<float, 3>> normals;
    std::vector<std::array<float, 2>> tex_coords;


    std::string current_object_name;
    std::string current_group_name = "off";

    unsigned int off_gid = Id_giver::no_id;

    std::ifstream obj_file(obj_filename);
    std::string current_line;
    while (std::getline(obj_file, current_line)) {
        std::replace(current_line.begin(), current_line.end(), '/', ' ');
        std::stringstream line_stream(current_line);
        std::string current_token;
        line_stream >> current_token;
        if (current_token == "v") {
            std::array<float, 3> coords;
            line_stream >> coords[0] >> coords[1] >> coords[2];
            vertex_coords.push_back(coords);
            vertex_groups.push_back(Id_giver::no_id); // unused id
            is_pivot.push_back(false);
        } else if (current_token == "vt") {
            std::array<float, 2> coords;
            line_stream >> coords[0] >> coords[1];
            tex_coords.push_back(coords);
        } else if (current_token == "vn") {
            std::array<float, 3> coords;
            line_stream >> coords[0] >> coords[1] >> coords[2];
            normals.push_back(coords);
        } else if (current_token == "f") {
            unsigned int current_gid =
                id_giver.get_id(current_object_name + "." + current_group_name);


            for (unsigned int i = 0; i < 3; i++) {
                vertex_t current_vertex;
                unsigned int v_index, vt_index, vn_index;
                line_stream >> v_index >> vt_index >> vn_index;
                if (vertex_groups[v_index - 1] != Id_giver::no_id
                    && vertex_groups[v_index - 1] != current_gid) {
                    is_pivot[v_index - 1] = true;
                }
                if (vertex_groups[v_index - 1] == Id_giver::no_id || current_gid != off_gid) {
                    vertex_groups[v_index - 1] = current_gid;
                }
            }
        } else if (current_token == "o") {
            line_stream >> current_object_name;
            off_gid = id_giver.get_id(current_object_name + ".off");
        } else if (current_token == "g") {
            line_stream >> current_group_name;
        }
    }
    obj_file.clear();
    obj_file.seekg(0, std::ios::beg);

    std::vector<vertex_t> vertices;

    while (std::getline(obj_file, current_line)) {

        std::replace(current_line.begin(), current_line.end(), '/', ' ');
        std::stringstream line_stream(current_line);
        std::string current_token;
        line_stream >> current_token;

        if (current_token == "f") {
            for (unsigned int i = 0; i < 3; i++) {
                vertex_t current_vertex;
                unsigned int v_index, vt_index, vn_index;
                line_stream >> v_index >> vt_index >> vn_index;
                std::array<float, 3> &coords = vertex_coords[v_index - 1];
                std::array<float, 3> &normal = normals[vn_index - 1];
                std::array<float, 2> &tex = tex_coords[vt_index - 1];
                std::copy(coords.begin(), coords.end(), current_vertex.position);
                std::copy(normal.begin(), normal.end(), current_vertex.normal);
                std::copy(tex.begin(), tex.end(), current_vertex.tex_coord);

                current_vertex.mat_index = vertex_groups[v_index - 1];

                vertices.push_back(current_vertex);
            }
        }
    }
    vertex_buffer.init(device, vertices);

    std::map<unsigned int, unsigned int> id_to_num_pivot_points;
    for (unsigned int i = 0; i < vertex_coords.size(); i++) {

        if (!is_pivot[i]) {
            continue;
        }
        id_to_num_pivot_points[vertex_groups[i]]++;
        std::array<float, 3> &sum_array = id_to_pivot_point[vertex_groups[i]];
        for (unsigned int j = 0; j < 3; j++) {
            sum_array[j] += vertex_coords[i][j];
        }
    }

    for (const auto &[id, num] : id_to_num_pivot_points) {
        std::array<float, 3> &sum_array = id_to_pivot_point[id];
        for (unsigned int j = 0; j < 3; j++) {
            sum_array[j] /= num;
        }
    }
}

void Object::draw(ComPtr<ID3D12GraphicsCommandList> &command_list) {
    texture.use(command_list, 1); // 1 is the texture argument number
    command_list->IASetVertexBuffers(0, 1, &vertex_buffer.get_view());
    command_list->DrawInstanced(vertex_buffer.get_vertex_count(), 1, 0, 0);
}
