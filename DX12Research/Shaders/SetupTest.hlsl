#include "Common.hlsli"

VsOutput VSMain(uint uVertexId : SV_VertexID)
{
    VsOutput output;
    float2 vUv = float2((uVertexId << 1) & 2, uVertexId & 2);
    output.vPosition = float4(vUv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.vUv = vUv;
    return output;
}

float4 PSMain(VsOutput input) : SV_Target
{
    return g_vColor;
}
