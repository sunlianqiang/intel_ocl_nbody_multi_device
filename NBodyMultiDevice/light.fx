/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or 
     nondisclosure agreement with Intel Corporation and may not be copied 
     or disclosed except in accordance with the terms of that agreement. 
          Copyright (C) 2007 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

/// @file light.fx


cbuffer cbChangesEveryFrame
{
    matrix WorldView;
    matrix WorldViewProj;
    float4 MaterialDiffuse;    
};


struct VS_INPUT
{
    float3 pos    : POSITION;
    float3 norm   : NORMAL;    
};

struct PS_INPUT
{
    float4 hpos   : SV_POSITION;  // vertex position in homogenius space
    float4 color  : COLOR;
};




//-----------------------------------------------------------------------------
// Vertex Shader
//-----------------------------------------------------------------------------

PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output;

    // Transform the position from object space to homogeneous projection space
    output.hpos = mul( float4(input.pos, 1), WorldViewProj );

    float3 vnorm = normalize(mul(input.norm, (float3x3) WorldView));

    output.color.rgb = MaterialDiffuse * (-vnorm.z);
    output.color.a   = 1.0f;
    
    return output;
}



//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

float4 PS( PS_INPUT input) : SV_Target
{
    return input.color;
}



//--------------------------------------------------------------------------------------
// Technique
//--------------------------------------------------------------------------------------

RasterizerState TurnMultisampleOn
{
    MultisampleEnable = true;
    FillMode = SOLID;
};

DepthStencilState DepthTestOn
{
    DepthEnable = true;
};

technique10 Render
{
    pass P0
    {
        SetRasterizerState( TurnMultisampleOn );
        SetDepthStencilState( DepthTestOn, 0 );

        SetVertexShader( CompileShader( vs_4_0, VS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS() ) );
    }
}

