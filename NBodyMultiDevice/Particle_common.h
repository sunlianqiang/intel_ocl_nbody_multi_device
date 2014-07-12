/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2008 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */
//#define _CRT_SECURE_NO_WARNINGS
const UINT MAX_PARTICLES = 0xffff;

struct PARTICLES
{
    float*         PosX;
    float*         PosY;
    float*         PosZ;
    float*         ColorCode;
    float*       Mass;
};
