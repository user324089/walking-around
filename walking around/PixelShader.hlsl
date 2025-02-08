struct ps_input_t
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD;
};

Texture2D texture_ps;
SamplerState sampler_ps;

float4 main(ps_input_t input) : SV_TARGET
{
    return texture_ps.Sample(
         sampler_ps, input.tex);

}
