#pragma once
#include "Windows_includes.hpp"


struct Shader_const_buffer {
    public:
        DirectX::XMFLOAT4X4 matWorld[10];
        DirectX::XMFLOAT4X4 matView;
        DirectX::XMFLOAT4X4 matProj;
        DirectX::XMFLOAT4 colLight, dirLight;
};