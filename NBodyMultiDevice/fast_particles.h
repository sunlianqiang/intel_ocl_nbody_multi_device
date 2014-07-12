/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2008-2012 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

#pragma once

#include "iNBody.h"
#include "ExtensionCamera.h"
#include "Widgets.h"
//#include "SampleBase.h"

// forward declarations
class CSample_NBody : public CExtensionCamera
{
    void UpdateControls();

public:
    CSample_NBody();
    ~CSample_NBody();

protected:
    ID3D10Effect*            m_pEffect;
    ID3D10EffectTechnique*   m_pTechnique;
    ID3D10EffectTechnique*   m_pAvailableTechniques[2];

    ID3D10EffectMatrixVariable* m_pWorldViewVariable;
    ID3D10EffectMatrixVariable* m_pProjVariable;

    ID3D10Buffer*           m_pParticleVB_PosX;
    ID3D10Buffer*           m_pParticleVB_PosY;
    ID3D10Buffer*           m_pParticleVB_PosZ;
    ID3D10Buffer*           m_pParticleVB_Color;
    ID3D10Buffer*           m_pParticleVB_Mass;

    ID3D10Buffer*           m_pParticleIB;
    ID3D10InputLayout*      m_pParticleVertexLayout;

    ID3D10EffectScalarVariable* m_fMass0Variable;
    ID3D10EffectScalarVariable* m_fMass1Variable;

    int   m_nNBodyCount;
    int   m_nOptimization;
    bool  m_bAnimation_On;
    bool  m_bManualLoadBalancing;
    int   m_nLog2WorkGroupSize;
    int      m_nVizMode;

    float m_fpsMax;

    UINT m_width, m_height;

    enum
    {
        IDC_ANIMATION_CHKBOX = 101,
        IDC_OPTIMIZATION_COMBO,
        IDC_NBODY_FRACTION_STATIC,
        IDC_NBODY_FRACTION_SCALE,
        IDC_WG_SZ_STATIC,
        IDC_WG_SZ_SCALE,
        IDC_CPUGPU_COMBO,
        IDC_CPUGPU_STATIC,
        IDC_PROVIDER_COMBO,
        IDC_PROVIDER_STATIC,
        IDC_LOADBALANCING_CHKBOX,
        IDC_LOADBALANCING_STATIC,
        IDC_LOADBALANCING_SCALE,
        IDC_RESET_CAMERA,
        IDC_MAXIMUM_FPS,
        IDC_VIZ_MODE_RADIO,
        IDC_VIZ_MODE_RADIO_0,
        IDC_VIZ_MODE_RADIO_1
    };

    iNBody* m_pSim;
    PARTICLES m_pCPUParticles;
    DWORD* m_pCPUParticleIndices;

    HRESULT LoadEffects(ID3D10Device* pd3dDevice);

    HRESULT OnCreateDevice(ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC *in_pBufferSurfaceDesc);
    void OnDestroyDevice();
    void OnInitApp(StartupInfo *io_StartupInfo);
    void OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl);
    void OnRenderScene(ID3D10Device* pd3dDevice, double fTime, float fElapsedTime);
    void OnRenderText(CDXUTTextHelper* pTextHelper, double fTime, float fElapsedTime);
    LRESULT OnWidgetNotify(CWidgetBase *pWidget, WPARAM wParam, LPARAM lParam);

    void OnClearRenderTargets(ID3D10Device* pd3dDevice, double fTime, float fElapsedTime);
    //HRESULT OnResizedSwapChain(ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc);
    //void OnFrameMove(double fTime, float fElapsedTime);
    //LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool *pbNoFurtherProcessing);
    //void KeyboardProc(UINT nChar, bool bKeyDown, bool bAltDown);
};

///////////////////////////////////////////////////////////////////////////////
// end of file
