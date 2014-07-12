/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2007 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */


/// @file ExtensionCamera.cpp
///
/// Implementation of the CExtensionCamera class.

#include "stdafx.h"

#include "ExtensionCamera.h"
#include "Widgets.h"

///////////////////////////////////////////////////////////////////////////////

CExtensionCamera::CExtensionCamera(void)
{
    m_pCameraWidget = new CCameraWidget(this);
}

CExtensionCamera::~CExtensionCamera(void)
{
    SAFE_DELETE(m_pCameraWidget);
}

void CExtensionCamera::OnInitApp(StartupInfo *io_StartupInfo)
{
    CSampleBase::OnInitApp(io_StartupInfo);
    m_pCameraWidget->OnInitApp();
}

HRESULT CExtensionCamera::OnCreateDevice( ID3D10Device* pd3dDevice,
                                          const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc )
{
    HRESULT hr;
    V_RETURN(CSampleBase::OnCreateDevice(pd3dDevice, in_pBufferSurfaceDesc));
    return S_OK;
}

LRESULT CExtensionCamera::MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam, bool* pbNoFurtherProcessing )
{
    LRESULT res;

    res = CSampleBase::MsgProc( hWnd, uMsg, wParam, lParam, pbNoFurtherProcessing );
    if( *pbNoFurtherProcessing || 0 != res )
        return res;

    res = m_pCameraWidget->MsgProc( hWnd, uMsg, wParam, lParam, pbNoFurtherProcessing );
    if( *pbNoFurtherProcessing || 0 != res )
        return res;

    return 0;
}

void CExtensionCamera::OnFrameMove( double fTime, float fElapsedTime )
{
    CSampleBase::OnFrameMove(fTime, fElapsedTime);
    m_pCameraWidget->OnFrameMove(fTime, fElapsedTime);
}

HRESULT CExtensionCamera::OnResizedSwapChain( ID3D10Device* pd3dDevice,
    IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc )
{
    HRESULT hr;
    V_RETURN(CSampleBase::OnResizedSwapChain(pd3dDevice, pSwapChain, in_pBufferSurfaceDesc));
    V_RETURN(m_pCameraWidget->OnResizedSwapChain(pd3dDevice, pSwapChain, in_pBufferSurfaceDesc));
    return S_OK;
}

// end of file
