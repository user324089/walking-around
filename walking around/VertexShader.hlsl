cbuffer vs_const_buffer_t
{
    float4x4 matWorld[10];
    float4x4 matView;
    float4x4 matProj;
    float4 colLight;
    float4 dirLight;
};

struct vs_output_t
{
    float4 position : SV_POSITION;
    float4 viewer : VIEWER;
    float2 tex : TEXCOORD;
    float3 norm : NORMAL_PS;
};

vs_output_t main(
 		float3 pos : POSITION,
 		float3 norm : NORMAL,
        float2 tex : TEXCOORD,
        uint mat_index : MAT_INDEX)
{
    vs_output_t result;
    float4 normal_vec = mul(mul(float4(norm, 0.0f), matWorld[mat_index]), matView);
    result.viewer = -mul(mul(float4(pos, 1.0f), matWorld[mat_index]), matView);
    result.position = mul(mul(mul(float4(pos, 1.0f), matWorld[mat_index]), matView), matProj);
    result.tex = tex;
    result.norm = normalize(normal_vec);

    return result;
}
