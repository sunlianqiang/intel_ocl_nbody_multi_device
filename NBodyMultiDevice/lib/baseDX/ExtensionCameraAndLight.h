/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2007 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

/// @file ExtensionCameraAndLight.h
///
/// Definition of the CExtensionCameraAndLight class.

#pragma once

#include "SampleBase.h"


// forward declarations
class CCameraWidget;
class CLightingWidget;


///
/// @class CExtensionCameraAndLight adds extra camera and light functionality to the CSampleBase class
///
class CExtensionCameraAndLight : public CSampleBase
{
    CCameraWidget    *m_pCameraWidget;
    CLightingWidget  *m_pLightingWidget;

    CExtensionCameraAndLight(const CExtensionCameraAndLight&);
    CExtensionCameraAndLight& operator = (const CExtensionCameraAndLight&);

public:
    CExtensionCameraAndLight(void);
    ~CExtensionCameraAndLight(void);

protected:
    CCameraWidget* GetCameraWidget() const { return m_pCameraWidget; }
    CLightingWidget* GetLightingWidget() const { return m_pLightingWidget; }

protected:
    //
    // CSampleBase overrides
    //

    void OnInitApp(StartupInfo *io_StartupInfo);

    HRESULT OnCreateDevice( ID3D10Device* pd3dDevice,
                            const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc );

    void OnDestroyDevice();

    LRESULT MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam,
                     LPARAM lParam, bool* pbNoFurtherProcessing );

    void OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl );

    void OnFrameMove( double fTime, float fElapsedTime );

    HRESULT OnResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain,
                                const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc );
};


// end of file
