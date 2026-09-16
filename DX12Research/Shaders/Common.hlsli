#ifndef DX12R_COMMON_HLSLI
#define DX12R_COMMON_HLSLI

cbuffer FrameConstants : register(b0)
{
    float4x4 g_mViewProj;
    float4 g_vColor;
};

struct VsOutput
{
    float4 vPosition : SV_Position;
    float2 vUv : TEXCOORD0;
};

#endif
