/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or 
     nondisclosure agreement with Intel Corporation and may not be copied 
     or disclosed except in accordance with the terms of that agreement. 
          Copyright (C) 2007-2012 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

/// @file stump.fx
///    Effects file for STUMP.


#define MAX_LIGHTS  3

//-----------------------------------------------------------------------------
// Constant Buffer Variables
//-----------------------------------------------------------------------------

cbuffer cbChangesEveryFrame
{
    matrix WorldView;
    matrix Proj;

    float Radius0 = 1.5;
    float Radius1 = 150.0;
    float Mass0;
    float Mass1;
    float SigmaScatter = 0.7;
    float SigmaAbsorb = 0.2;
    float3 AmbientColor0 = float3(1.f, .75f, 0.f);
    float3 DiffuseColor0 = float3(1.f, .15f, 0.f);
    float3 AmbientColor1 = float3(0.7f, .95f, 1.0f);
    float3 DiffuseColor1 = float3(0, 0.45f, 0.85f);
}

//--------------------------------------------------------------------------------------
// GPU-assisted hard particle rendering
//--------------------------------------------------------------------------------------
struct Particle
{
    float PosX            : POSX;
    float PosY            : POSY;
    float PosZ            : POSZ;
    float  ColorCode      : COLOR;
    float  Mass           : MASS;
};

struct PSParticleIn
{
                    float4 Pos              : SV_POSITION;
    noperspective   float3 viewPos          : VIEWPOS;
    nointerpolation float3 particleOrig     : ORIGIN;
                    float  Radius           : RADIUS;
                    float3 ambColor         : COLOR0;
                    float3 difColor         : COLOR1;
};

Particle VSParticlemain(Particle input)
{
    Particle output = input;
    float3 res = mul( float4( input.PosX,input.PosY, input.PosZ, 1 ), WorldView);
    output.PosX =  res.x;
    output.PosY =  res.y;
    output.PosZ =  res.z;
    return output;
}

cbuffer cbImmutable
{
    float3 g_positions[4] =
    {
        float3( -1,  1, 0 ),
        float3(  1,  1, 0 ),
        float3( -1, -1, 0 ),
        float3(  1, -1, 0 ),
    };
}

/*this trick originated in DX SDK (one of the samples)*/
static const float3 Rainbow[5] = {
    float3(1, 0, 0), // red
    float3(1, 1, 0), // orange
    float3(0, 1, 0), // green
    float3(0, 1, 1), // teal
    float3(0, 0, 1), // blue
};
float3 VisualizeNumber(float n)
{
    return lerp( Rainbow[ floor(n * 4.0f) ], Rainbow[ ceil(n * 4.0f) ], frac(n * 4.0f) );
}
/*this trick originated in DX SDK (one of the samples)\*/


// Geometry shader for creating point sprites
[maxvertexcount(4)]
void GSParticlemain(point Particle input[1], inout TriangleStream<PSParticleIn> SpriteStream)
{
    PSParticleIn output;
    output.particleOrig  = float3(input[0].PosX,input[0].PosY, input[0].PosZ);
    float mass_normalized = saturate(input[0].Mass / (Mass1 - Mass0));
    mass_normalized = mass_normalized * mass_normalized * mass_normalized * mass_normalized;
    output.Radius = lerp(Radius0, Radius1, mass_normalized);
    output.ambColor =  lerp(AmbientColor0, AmbientColor1, mass_normalized);
    output.difColor =  VisualizeNumber(input[0].ColorCode);
       
    // Emit two new triangles
    [unroll] for(int i=0; i<4; i++)
    {
        float3 position = g_positions[i] * output.Radius + output.particleOrig;
        
        output.viewPos = position;                            // view position
        output.Pos = mul( float4(position,1.0), Proj );        // project space position
        SpriteStream.Append(output);
    }
    SpriteStream.RestartStrip();
}

[maxvertexcount(4)]
void GSParticlemainSplit(point Particle input[1], inout TriangleStream<PSParticleIn> SpriteStream)
{
    PSParticleIn output;
    output.particleOrig  = float3(input[0].PosX,input[0].PosY, input[0].PosZ);
    float mass_normalized = saturate(input[0].Mass / (Mass1 - Mass0));
    mass_normalized = mass_normalized * mass_normalized * mass_normalized * mass_normalized;
    output.Radius = lerp(Radius0, Radius1, mass_normalized);
    output.ambColor =  lerp(AmbientColor0, AmbientColor1, mass_normalized);
    output.difColor =  input[0].ColorCode==0 ? float3(1,0,0): float3(0,0,1);
       
    // Emit two new triangles
    [unroll] for(int i=0; i<4; i++)
    {
        float3 position = g_positions[i] * output.Radius + output.particleOrig;
        
        output.viewPos = position;                            // view position
        output.Pos = mul( float4(position,1.0), Proj );        // project space position
        SpriteStream.Append(output);
    }
    SpriteStream.RestartStrip();
}


float my_exp(float f)
{
    float res = 1+f;
    //aproximation for f<1
    return res*res*res;
}

float4 PSParticlemain( PSParticleIn input) : SV_Target
{   
    float path = 1 - length(input.viewPos - input.particleOrig) / input.Radius;
    float opacity = 1 - saturate(my_exp(-path * SigmaAbsorb));

    float res = path * SigmaScatter;
    float3 color  = saturate(input.ambColor * opacity) + res * input.difColor;
    return float4(color,opacity);
    //return float4(color,1);
}
float4 PSParticlemainSplit( PSParticleIn input) : SV_Target
{   
    float3 n = input.particleOrig - input.viewPos;
    float len = length(n);
    if(len> input.Radius)
        discard;
    float path = 1 - len/ input.Radius;
    float opacity = 1 - saturate(my_exp(-path * SigmaAbsorb));

    float res = path * SigmaScatter;
    float3 color  = saturate(input.ambColor* opacity + res * input.difColor);
    return float4(color,opacity);
    //return float4(color,1);
}

RasterizerState RSCullingOff
{
    MultisampleEnable = false;
    CullMode = none;
};

DepthStencilState DSSDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = 0;
    StencilEnable = FALSE;
};

BlendState ParticlesBS
{
    AlphaToCoverageEnable    = FALSE;
    BlendEnable[0]           = TRUE;
    RenderTargetWriteMask[0] = 0xF;//D3D10_COLOR_WRITE_ENABLE_ALL;
    BlendOp                  = ADD;
    SrcBlend                 = ONE;
    DestBlend                = INV_SRC_ALPHA;
    SrcBlendAlpha            = ZERO;
    DestBlendAlpha           = ZERO;
    BlendOpAlpha             = ADD;
};

technique10 ParticlesRenderRandom
{
    pass P0
    {
        SetRasterizerState( RSCullingOff );
        SetBlendState(ParticlesBS , float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
        SetDepthStencilState( DSSDepth, 0 );
        SetVertexShader( CompileShader( vs_4_0, VSParticlemain() ) );
        SetGeometryShader( CompileShader( gs_4_0, GSParticlemain() ) );
        SetPixelShader( CompileShader( ps_4_0, PSParticlemain() ) );
    }
}


technique10 ParticlesRenderSplit
{
    pass P0
    {
        SetRasterizerState( RSCullingOff );
        SetBlendState(ParticlesBS , float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
        SetDepthStencilState( DSSDepth, 0 );
        SetVertexShader( CompileShader( vs_4_0, VSParticlemain() ) );
        SetGeometryShader( CompileShader( gs_4_0, GSParticlemainSplit() ) );
        SetPixelShader( CompileShader( ps_4_0, PSParticlemainSplit() ) );
    }
}
