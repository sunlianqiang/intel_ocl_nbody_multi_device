/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2012 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

/// @file stdafx.h
#define _CRT_SECURE_NO_WARNINGS
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"

#include "SDKmisc.h"


#pragma warning(disable: 4995) // 'name' was marked as #pragma deprecated

#define _SECURE_SCL 0  // disable checked iterators

#include <crtdbg.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <smmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#if defined(__INTEL_COMPILER)
#include <immintrin.h>
#endif
