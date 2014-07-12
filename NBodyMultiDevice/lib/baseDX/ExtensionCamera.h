/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2007 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

/// @file ExtensionCamera.h
///
/// Definition of the CExtensionCamera class.

#pragma once

#include "SampleBase.h" // defines the CSampleBase class


// forward declarations
class CCameraWidget;

///
/// @class CExtensionCamera adds extra camera functionality to the CSampleBase class
///
class CExtensionCamera : public CSampleBase
{
    CCameraWidget    *m_pCameraWidget;

    /// private constructor prevents object copying
    CExtensionCamera(const CExtensionCamera&);
    CExtensionCamera& operator = (const CExtensionCamera&);

public:
    CExtensionCamera(void);
    ~CExtensionCamera(void);

protected:
    CCameraWidget* GetCameraWidget() const { return m_pCameraWidget; }

protected:
    //
    // CSampleBase overrides
    //

    void OnInitApp(StartupInfo *io_StartupInfo);

    HRESULT OnCreateDevice( ID3D10Device* pd3dDevice,
                            const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc );

    LRESULT MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam,
                     LPARAM lParam, bool* pbNoFurtherProcessing );

    void OnFrameMove( double fTime, float fElapsedTime );

    HRESULT OnResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain,
                                const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc );
};


// end of file
