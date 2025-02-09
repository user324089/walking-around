cbuffer vs_const_buffer_t
{
    float4x4 matWorld[10];
    float4x4 matView;
    float4x4 matProj;
    float4 colLight;
    float4 dirLight;
};

struct ps_input_t
{
    float4 position : SV_POSITION;
    float4 viewer : VIEWER;
    float2 tex : TEXCOORD;
    float3 norm : NORMAL_PS;
};

Texture2D texture_ps;
SamplerState sampler_ps;

float4 main(ps_input_t input) : SV_TARGET
{
    float3 light_dir = normalize(mul(dirLight, matView));
    float amb_light = 0.4;
    float dir_light = max(0, dot(input.norm, light_dir));
    float3 h = normalize(normalize(input.viewer.xyz) + light_dir);
    float spec_light = pow(dot(h, input.norm), 2)/2;
    return (amb_light + dir_light) * texture_ps.Sample(
         sampler_ps, input.tex) + spec_light * colLight;

}
