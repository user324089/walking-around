struct ps_input_t
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD;
    float3 norm : NORMAL_PS;
};

Texture2D texture_ps;
SamplerState sampler_ps;

float4 main(ps_input_t input) : SV_TARGET
{
    float light = max(0, dot(input.norm, float3(0,0,-1))) + 0.5;
    //return float4(input.norm, 1.0f);
    return light * texture_ps.Sample(
         sampler_ps, input.tex);

}
