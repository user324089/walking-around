cbuffer vs_const_buffer_t
{
    float4x4 matWorld[10];
    float4x4 matView;
    float4x4 matProj;
    float4 colMaterial;
    float4 colLight;
    float4 dirLight;
};

struct vs_output_t
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD;
};

vs_output_t main(
 		float3 pos : POSITION,
 		float3 norm : NORMAL,
        float2 tex : TEXCOORD,
        uint mat_index : MAT_INDEX)
{
    vs_output_t result;
    float4 NW = mul(float4(norm, 0.0f), matWorld[mat_index]);
    result.position = mul(mul(mul(float4(pos, 1.0f), matWorld[mat_index]), matView), matProj);
    result.tex = tex;

    return result;
}
