/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2007 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

/// @file ExtensionCameraAndLight.cpp
///
/// Implementation of the CExtensionCameraAndLight class.

#include "stdafx.h"

#include "ExtensionCameraAndLight.h"
#include "Widgets.h"

///////////////////////////////////////////////////////////////////////////////

CExtensionCameraAndLight::CExtensionCameraAndLight(void)
{
    m_pCameraWidget = new CCameraWidget(this);
    m_pLightingWidget = new CLightingWidget(this);
}

CExtensionCameraAndLight::~CExtensionCameraAndLight(void)
{
    SAFE_DELETE(m_pLightingWidget);
    SAFE_DELETE(m_pCameraWidget);
}

void CExtensionCameraAndLight::OnInitApp(StartupInfo *io_StartupInfo)
{
    CSampleBase::OnInitApp(io_StartupInfo);
    m_pCameraWidget->OnInitApp();
    m_pLightingWidget->OnInitApp();
}

HRESULT CExtensionCameraAndLight::OnCreateDevice( ID3D10Device* pd3dDevice,
                                                  const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc )
{
    HRESULT hr;
    V_RETURN(CSampleBase::OnCreateDevice(pd3dDevice, in_pBufferSurfaceDesc));
    V_RETURN(m_pLightingWidget->OnCreateDevice(pd3dDevice, in_pBufferSurfaceDesc));
    return S_OK;
}

void CExtensionCameraAndLight::OnDestroyDevice()
{
    m_pLightingWidget->OnDestroyDevice();
    CSampleBase::OnDestroyDevice();
}

LRESULT CExtensionCameraAndLight::MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam,
                                           LPARAM lParam, bool* pbNoFurtherProcessing )
{
    LRESULT res;

    res = CSampleBase::MsgProc( hWnd, uMsg, wParam, lParam, pbNoFurtherProcessing );
    if( *pbNoFurtherProcessing || 0 != res )
        return res;

    res = m_pCameraWidget->MsgProc( hWnd, uMsg, wParam, lParam, pbNoFurtherProcessing );
    m_pLightingWidget->UpdateViewMatrix(m_pCameraWidget->GetCamera()->GetViewMatrix());
    if( *pbNoFurtherProcessing || 0 != res )
        return res;

    res = m_pLightingWidget->MsgProc( hWnd, uMsg, wParam, lParam, pbNoFurtherProcessing );
    if( *pbNoFurtherProcessing || 0 != res )
        return res;


    return 0;
}

void CExtensionCameraAndLight::OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl )
{
    CSampleBase::OnGUIEvent(nEvent, nControlID, pControl);
    m_pLightingWidget->OnGUIEvent(nEvent, nControlID, pControl);
}

void CExtensionCameraAndLight::OnFrameMove( double fTime, float fElapsedTime )
{
    CSampleBase::OnFrameMove(fTime, fElapsedTime);
    m_pCameraWidget->OnFrameMove(fTime, fElapsedTime);
}

HRESULT CExtensionCameraAndLight::OnResizedSwapChain( ID3D10Device* pd3dDevice,
    IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc )
{
    HRESULT hr;
    V_RETURN(CSampleBase::OnResizedSwapChain(pd3dDevice, pSwapChain, in_pBufferSurfaceDesc));
    V_RETURN(m_pCameraWidget->OnResizedSwapChain(pd3dDevice, pSwapChain, in_pBufferSurfaceDesc));
    V_RETURN(m_pLightingWidget->OnResizedSwapChain(pd3dDevice, pSwapChain, in_pBufferSurfaceDesc));
    return S_OK;
}

// end of file
