/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2012 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

#include "stdafx.h"
#include <TCHAR.H>
#include <strsafe.h>
#include "fast_particles.h"
#include "basic.hpp"
char  g_device_name[1024]={0};

///////////////////////////////////////////////////////////////////////////////
CSample_NBody::CSample_NBody():
        m_pSim(NULL)
        , m_pEffect(NULL)
        , m_pTechnique(NULL)
        , m_pParticleVB_PosX(NULL)
        , m_pParticleVB_PosY(NULL)
        , m_pParticleVB_PosZ(NULL)
        , m_pParticleVB_Color(NULL)
        , m_pParticleVB_Mass(NULL)
        , m_pParticleIB(NULL)
        , m_pParticleVertexLayout(NULL)
        , m_fpsMax (0.f)
{
    m_pSim = new iNBody();
    m_pSim->SetValidation(GetArgString(L"v")!=NULL);

    m_pCPUParticles.PosX = new float[m_pSim->GetMaxWorkSize()];
    m_pCPUParticles.PosY = new float[m_pSim->GetMaxWorkSize()];
    m_pCPUParticles.PosZ = new float[m_pSim->GetMaxWorkSize()];
    m_pCPUParticles.ColorCode = new float[m_pSim->GetMaxWorkSize()];
    m_pCPUParticles.Mass = new float[m_pSim->GetMaxWorkSize()];
    m_pCPUParticleIndices = new DWORD[m_pSim->GetMaxWorkSize()];

    for(size_t i=0; i < m_pSim->GetMaxWorkSize(); i++)
        m_pCPUParticleIndices[i] = (DWORD)i;
};

CSample_NBody::~CSample_NBody(void)
{
    SAFE_DELETE_ARRAY( m_pCPUParticles.PosX );
    SAFE_DELETE_ARRAY( m_pCPUParticles.PosY );
    SAFE_DELETE_ARRAY( m_pCPUParticles.PosZ );
    SAFE_DELETE_ARRAY( m_pCPUParticles.ColorCode );
    SAFE_DELETE_ARRAY( m_pCPUParticles.Mass);
    SAFE_DELETE_ARRAY( m_pCPUParticleIndices );
    SAFE_DELETE( m_pSim );
};

HRESULT CSample_NBody::LoadEffects(ID3D10Device* pd3dDevice)
{
    HRESULT hr;

    SAFE_RELEASE(m_pEffect);

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;

    // Read the D3DX effect file
    V_RETURN(D3DX10CreateEffectFromFile(FULL_PATH("fast_particles.fx"), NULL, NULL, "fx_4_0", dwShaderFlags, 0,
        pd3dDevice, NULL, NULL, &m_pEffect, NULL, NULL));

    m_pAvailableTechniques[0] = m_pEffect->GetTechniqueByName( "ParticlesRenderRandom" );
    m_pAvailableTechniques[1] = m_pEffect->GetTechniqueByName( "ParticlesRenderSplit" );
    m_pTechnique = m_pAvailableTechniques[m_nVizMode];
    // Obtain variables
    m_pWorldViewVariable     = m_pEffect->GetVariableByName("WorldView")->AsMatrix();
    assert(m_pWorldViewVariable->IsValid());
    m_pProjVariable = m_pEffect->GetVariableByName("Proj")->AsMatrix();
    assert(m_pProjVariable->IsValid());
    m_fMass0Variable = m_pEffect->GetVariableByName("Mass0")->AsScalar();
    assert(m_fMass0Variable->IsValid());
    m_fMass1Variable = m_pEffect->GetVariableByName("Mass1")->AsScalar();
    assert(m_fMass1Variable->IsValid());
    return S_OK;
}

HRESULT CSample_NBody::OnCreateDevice( ID3D10Device* pd3dDevice,
                                 const DXGI_SURFACE_DESC *in_pBufferSurfaceDesc )
{
    HRESULT hr;
    V_RETURN(__super::OnCreateDevice(pd3dDevice, in_pBufferSurfaceDesc));

    V_RETURN(LoadEffects(pd3dDevice));

    D3D10_PASS_DESC passDesc;
    const D3D10_INPUT_ELEMENT_DESC particlelayout[] =
    {
        { "POSX", 0,       DXGI_FORMAT_R32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "POSY", 0,       DXGI_FORMAT_R32_FLOAT, 1, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "POSZ", 0,       DXGI_FORMAT_R32_FLOAT, 2, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0,           DXGI_FORMAT_R32_FLOAT,        3, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "MASS", 0,           DXGI_FORMAT_R32_FLOAT,        4, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 }
    };
    V_RETURN(m_pTechnique->GetPassByIndex(0)->GetDesc(&passDesc));
    V_RETURN( pd3dDevice->CreateInputLayout( particlelayout,
                                            sizeof(particlelayout)/sizeof(particlelayout[0]),
                                            passDesc.pIAInputSignature,
                                            passDesc.IAInputSignatureSize, &m_pParticleVertexLayout ) );

    D3D10_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = 0;
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D10_USAGE_DEFAULT;
    bdesc.ByteWidth = (UINT)(m_pSim->GetMaxWorkSize()*sizeof(float));
    V_RETURN( pd3dDevice->CreateBuffer( &bdesc, NULL, &m_pParticleVB_PosX ) );
    V_RETURN( pd3dDevice->CreateBuffer( &bdesc, NULL, &m_pParticleVB_PosY ) );
    V_RETURN( pd3dDevice->CreateBuffer( &bdesc, NULL, &m_pParticleVB_PosZ ) );
    V_RETURN( pd3dDevice->CreateBuffer( &bdesc, NULL, &m_pParticleVB_Color ) );
    V_RETURN( pd3dDevice->CreateBuffer( &bdesc, NULL, &m_pParticleVB_Mass ) );

    bdesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
    bdesc.ByteWidth = (UINT)(m_pSim->GetMaxWorkSize()*sizeof(DWORD));
    V_RETURN( pd3dDevice->CreateBuffer( &bdesc, NULL, &m_pParticleIB ) );
    return S_OK;
}

void CSample_NBody::OnDestroyDevice()
{
    SAFE_RELEASE( m_pParticleVB_PosX );
    SAFE_RELEASE( m_pParticleVB_PosY );
    SAFE_RELEASE( m_pParticleVB_PosZ );
    SAFE_RELEASE( m_pParticleVB_Color );
    SAFE_RELEASE( m_pParticleVB_Mass );

    SAFE_RELEASE( m_pParticleIB );
    SAFE_RELEASE( m_pParticleVertexLayout );
    SAFE_RELEASE( m_pEffect );

    __super::OnDestroyDevice();
}

void CSample_NBody::UpdateControls()
{
}

void CSample_NBody::OnInitApp(StartupInfo *io_StartupInfo)
{
    __super::OnInitApp(io_StartupInfo);

    // set the window title
    wcscpy_s(io_StartupInfo->WindowTitle, L"NBody simulation");

    // setup the camera
    //D3DXVECTOR3 eye(20000, 30000, 50000);
    D3DXVECTOR3 eye(20000, 30000, 5000);
    D3DXVECTOR3 at(5, 5, 5);
    GetCameraWidget()->CreateModelViewerCamera();
    GetCameraWidget()->GetCamera()->SetViewParams(&eye, &at);

    CBaseCamera* pCamera = GetCameraWidget()->GetCamera();
    static_cast<CModelViewerCamera*>(pCamera)->SetButtonMasks(MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON);

    //setup GUI controls
    CDXUTDialog *pUI = GetSampleUI();
    WCHAR sz[100];
    int iY = 0;

    m_bAnimation_On = true;
    pUI->AddCheckBox(IDC_ANIMATION_CHKBOX, L"Toggle simulation(Space)", -30, iY += 40, 170, 22, m_bAnimation_On, VK_SPACE);

    CDXUTComboBox *pCombo1 = NULL;
    pUI->AddStatic(IDC_CPUGPU_STATIC, L"Device(s):", -30, iY += 30, 125, 22);
    pUI->AddComboBox(IDC_CPUGPU_COMBO, -30, iY += 20, 170, 22, VK_ADD, false, &pCombo1);
    cl_platform_id id = GetIntelOCLPlatform();
    if(NULL==id)
    {
        ::MessageBox(0,L"No Intel OpenCL platform detected!Press any key to exit...\n", L"Initialization failed", MB_ICONERROR);
        exit(0);
    }
    pCombo1->AddItem( L"CPU", NULL);
    if(IsGPUDevicePresented(id))
    {
        pCombo1->AddItem( L"GPU", NULL);
        pCombo1->AddItem( L"CPU+GPU", NULL);
    }
    const TCHAR* str = GetArgString(L"t");

    if(str)//device is specified via command line)
    {
        //convert input parameter into upper case
        TCHAR* strup = _tcsdup(str);
        _tcsupr(strup);
        if (S_OK!=pCombo1->SetSelectedByText(strup))
        {
            WCHAR sz[100];
            swprintf( sz, 100, L"'-t %s' command line option is not valid. use '-t cpu', '-t gpu' or '-t cpu+gpu'", str);
            ::MessageBox(0,sz, L"No specified device found", MB_ICONERROR);
            exit(EXIT_FAILURE);
        }
        else
        {
            cl_device_type type = (0==_tcscmp(strup, L"CPU")) ? CL_DEVICE_TYPE_CPU : (0==_tcscmp(strup, L"GPU") ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_ALL);
            m_pSim->Cleanup();
            m_pSim->Setup(type, 0);
        }
        free(strup);
    }


    pUI->AddStatic( IDC_LOADBALANCING_STATIC, L"", -30, iY += 30, 150, 22 );
    pUI->GetStatic(IDC_LOADBALANCING_STATIC)->SetVisible(false);

    m_bManualLoadBalancing = false;
    pUI->AddCheckBox(IDC_LOADBALANCING_CHKBOX, L"(Manual)Load-Balancing", -30, iY += 20, 170, 22, m_bManualLoadBalancing, VK_BACK);
    pUI->GetCheckBox(IDC_LOADBALANCING_CHKBOX)->SetEnabled(pUI->GetComboBox(IDC_CPUGPU_COMBO)->GetSelectedIndex()==2);
      int nCPUportion = (int)(m_pSim->GetSplitting()*100.0f);
    pUI->AddSlider( IDC_LOADBALANCING_SCALE, -30, iY += 20, 180, 22, 1, 99, nCPUportion);
    pUI->GetSlider(IDC_LOADBALANCING_SCALE)->SetEnabled(false);


    m_nLog2WorkGroupSize = 4;//6;
    swprintf( sz, 100, L"Workgroup size %d", 1 << m_nLog2WorkGroupSize );
    pUI->AddStatic( IDC_WG_SZ_STATIC, sz, -30, iY += 30, 150, 22 );
    pUI->AddSlider( IDC_WG_SZ_SCALE, -30, iY += 20, 180, 22, 0, 9, m_nLog2WorkGroupSize);

    CDXUTComboBox *pCombo = NULL;
    pUI->AddStatic(-1, L"Optimization method:", -30, iY += 30, 125, 22);
    pUI->AddComboBox(IDC_OPTIMIZATION_COMBO, -30, iY += 20, 170, 22, VK_MULTIPLY, false, &pCombo);
    pCombo->AddItem( L"C serial", NULL);
    pCombo->AddItem( L"C parallel", NULL);
    pCombo->AddItem( L"SSE parallel", NULL);
    pCombo->AddItem( L"OpenCL", NULL);
    pUI->GetComboBox(IDC_OPTIMIZATION_COMBO)->SetSelectedByIndex(3);//5;//4;
    m_nOptimization = pUI->GetComboBox(IDC_OPTIMIZATION_COMBO)->GetSelectedIndex();//5;//4;

    CDXUTComboBox *pCombo2 = NULL;
    pUI->AddStatic(IDC_PROVIDER_STATIC, L"OpenCL provider:", -30, iY += 30, 125, 22);
    pUI->AddComboBox(IDC_PROVIDER_COMBO, -30, iY += 20, 170, 22, VK_DIVIDE, false, &pCombo2);
    pCombo2->AddItem( L"Intel", NULL);
    if(NULL!=GetATIOCLPlatform())
        pCombo2->AddItem( L"AMD", NULL);

    pCombo->SetSelectedByIndex(m_nOptimization);

    m_nNBodyCount = 8192;
    swprintf( sz, 100, L"Number of bodies %d", m_nNBodyCount);
    pUI->AddStatic( IDC_NBODY_FRACTION_STATIC, sz, -30, iY += 20, 150, 22 );
    pUI->AddSlider( IDC_NBODY_FRACTION_SCALE, -30, iY += 20, 180, 22, 1, 100, int(100.f*m_nNBodyCount/m_pSim->GetMaxWorkSize()));

    m_nVizMode = 0;//random
    pUI->AddRadioButton( IDC_VIZ_MODE_RADIO_0, IDC_VIZ_MODE_RADIO, L"RANDOM COLORS",  -30, iY += 30, 150, 22, m_nVizMode == 0,VK_F4);
    pUI->AddRadioButton( IDC_VIZ_MODE_RADIO_1, IDC_VIZ_MODE_RADIO, L"SPLIT VIZ",  -30, iY += 30, 150, 22, m_nVizMode == 1,VK_F5);

    pUI->AddButton( IDC_MAXIMUM_FPS, L"Reset Max. FPS",  -30, iY += 20, 180, 22, VK_DELETE);
    pUI->AddButton( IDC_RESET_CAMERA, L"Reset Camera",  -30, iY += 20, 180, 22,VK_HOME);

}

#define MAX_VIEW_DISTANCE 500000

LRESULT CSample_NBody::OnWidgetNotify(CWidgetBase *pWidget, WPARAM wParam, LPARAM lParam)
{
    if( pWidget == GetCameraWidget() && wParam == CCameraWidget::NOTIFY_RESIZE )
    {
        ((CCameraWidget::CameraProjParams*)lParam)->fFarPlane = MAX_VIEW_DISTANCE;
    }

    return 0;
}

void CSample_NBody::OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl)
{
    __super::OnGUIEvent( nEvent, nControlID, pControl );
        CDXUTDialog* const pUI = GetSampleUI();

    WCHAR sz[100];
    switch( nControlID )
    {
        case IDC_ANIMATION_CHKBOX:
            m_bAnimation_On = pUI->GetCheckBox(IDC_ANIMATION_CHKBOX)->GetChecked();
            break;
        case IDC_NBODY_FRACTION_SCALE:
            {
            int nFract  = pUI->GetSlider(IDC_NBODY_FRACTION_SCALE)->GetValue();
            int nNBodyCount = (int)(m_pSim->GetMaxWorkSize() * float(nFract)/100) ;
            m_nNBodyCount = nNBodyCount - nNBodyCount%(1 << m_nLog2WorkGroupSize);
            swprintf( sz, 100, L"Number of bodies %d", m_nNBodyCount);
            pUI->GetStatic(IDC_NBODY_FRACTION_STATIC)->SetText(sz);
            break;
            }

        case IDC_OPTIMIZATION_COMBO:
            m_nOptimization = pUI->GetComboBox(IDC_OPTIMIZATION_COMBO)->GetSelectedIndex();
            break;

        case IDC_PROVIDER_COMBO:
            {
                cl_platform_id id = pUI->GetComboBox(IDC_PROVIDER_COMBO)->GetSelectedIndex() == 0 ? GetIntelOCLPlatform(): GetATIOCLPlatform();
                pUI->GetComboBox(IDC_CPUGPU_COMBO)->RemoveAllItems();
                bool cpu = IsCPUDevicePresented(id);
                bool gpu = IsGPUDevicePresented(id);
                if(cpu)pUI->GetComboBox(IDC_CPUGPU_COMBO)->AddItem( L"CPU", NULL);
                if(gpu)pUI->GetComboBox(IDC_CPUGPU_COMBO)->AddItem( L"GPU", NULL);
                if(cpu&&gpu)pUI->GetComboBox(IDC_CPUGPU_COMBO)->AddItem( L"CPU+GPU", NULL);
            }
        case IDC_CPUGPU_COMBO:
            {
                const LPWSTR pname= pUI->GetComboBox(IDC_CPUGPU_COMBO)->GetSelectedItem()->strText;
                cl_device_type type = (0==_tcscmp(pname, L"CPU")) ? CL_DEVICE_TYPE_CPU : (0==_tcscmp(pname, L"GPU") ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_ALL);

                m_pSim->Cleanup();
                m_pSim->Setup(type, pUI->GetComboBox(IDC_PROVIDER_COMBO)->GetSelectedIndex()>0);

                break;
            }
        case IDC_LOADBALANCING_CHKBOX:
            m_bManualLoadBalancing^=1;
            pUI->GetSlider(IDC_LOADBALANCING_SCALE)->SetEnabled(m_bManualLoadBalancing);
            break;
        case IDC_LOADBALANCING_SCALE:
            assert(pUI->GetComboBox(IDC_CPUGPU_COMBO)->GetSelectedIndex() == 2);
            if(m_bManualLoadBalancing)
            {
                int  nCPUportion = pUI->GetSlider(IDC_LOADBALANCING_SCALE)->GetValue();
                float fCPUportion = float(nCPUportion)/100.f;
                m_pSim->SetSplitting(fCPUportion);
                swprintf( sz, 100, L"%d%% to CPU, %d%% to GPU", nCPUportion, 100-nCPUportion);
                pUI->GetStatic(IDC_LOADBALANCING_STATIC)->SetText(sz);
            }
            break;
        case IDC_WG_SZ_SCALE:
            m_nLog2WorkGroupSize = pUI->GetSlider(IDC_WG_SZ_SCALE)->GetValue();
            swprintf( sz, 100, L"Workgroup size %d", 1 << m_nLog2WorkGroupSize );
            pUI->GetStatic(IDC_WG_SZ_STATIC)->SetText(sz);
            break;
        case IDC_VIZ_MODE_RADIO_0:
        case IDC_VIZ_MODE_RADIO_1:
            m_nVizMode = pUI->GetRadioButton(IDC_VIZ_MODE_RADIO_1)->GetChecked();
            m_pTechnique = m_pAvailableTechniques[m_nVizMode];
            break;
        case IDC_MAXIMUM_FPS:
            m_fpsMax = 0.f;
            break;
        case IDC_RESET_CAMERA:
            GetCameraWidget()->GetCamera()->Reset();
            break;
     }
}

void CSample_NBody::OnRenderScene(ID3D10Device* pd3dDevice, double fTime, float fElapsedTime)
{
    CDXUTDialog* const pUI = GetSampleUI();
    pUI->GetSlider(IDC_WG_SZ_SCALE)->SetVisible(m_nOptimization>2);//some OCL flavor required
    pUI->GetStatic(IDC_WG_SZ_STATIC)->SetVisible(m_nOptimization>2);//some OCL flavor required
    pUI->GetComboBox(IDC_CPUGPU_COMBO)->SetVisible(m_nOptimization>2 /*OCL selected*/);
    pUI->GetComboBox(IDC_PROVIDER_COMBO)->SetVisible(m_nOptimization>2);//some OCL flavor required
    pUI->GetStatic(IDC_CPUGPU_STATIC)->SetVisible(m_nOptimization>2 /*OCL selected*/);
    pUI->GetStatic(IDC_PROVIDER_STATIC)->SetVisible(m_nOptimization>2);//some OCL flavor required
    pUI->GetCheckBox(IDC_LOADBALANCING_CHKBOX)->SetVisible(m_nOptimization>2);
    pUI->GetSlider(IDC_LOADBALANCING_SCALE)->SetVisible(m_nOptimization>2);


    //always update the text
    WCHAR sz[100];
    int res = pUI->GetComboBox(IDC_CPUGPU_COMBO)->GetSelectedIndex();
    if(res == 2)
    {
            if(!m_bManualLoadBalancing)
            {
                float fCPUportion = m_pSim->GetSplitting();
                int  nCPUportion = (int)(fCPUportion*100.0f);
                swprintf( sz, 100, L"AUTO %d%%to CPU, %d%% to GPU", nCPUportion, 100-nCPUportion);
                pUI->GetStatic(IDC_LOADBALANCING_STATIC)->SetText(sz);
                //pUI->GetSlider(IDC_LOADBALANCING_SCALE)->SetValue(nCPUportion);
            }
    }
    pUI->GetCheckBox(IDC_LOADBALANCING_CHKBOX)->SetEnabled(res==2 );
    pUI->GetSlider(IDC_LOADBALANCING_SCALE)->SetEnabled(res==2 && m_bManualLoadBalancing);
    pUI->GetStatic(IDC_LOADBALANCING_STATIC)->SetVisible(res==2 && m_nOptimization>2);


    if(m_bAnimation_On)
        m_pSim->Execute( m_nNBodyCount,1 << m_nLog2WorkGroupSize,
        m_nOptimization, m_pCPUParticles,m_bManualLoadBalancing, m_nVizMode );

    UINT strides[5];
    UINT offsets[5];
    D3D10_TECHNIQUE_DESC techDesc;
    m_pTechnique->GetDesc( &techDesc );

    // apply the camera
    CModelViewerCamera *pCam = static_cast<CModelViewerCamera*>(GetCameraWidget()->GetCamera());
    D3DXMATRIX tmp;
    D3DXMatrixMultiply(&tmp, pCam->GetWorldMatrix(), pCam->GetViewMatrix());
    m_pWorldViewVariable->SetMatrix(tmp);
    //m_pWorldViewVariable->SetMatrix( (float *) pCam->GetViewMatrix() );
    m_pProjVariable->SetMatrix((float *) pCam->GetProjMatrix());

    //pass particle buffers to GPU
    pd3dDevice->UpdateSubresource( m_pParticleIB, NULL, NULL, m_pCPUParticleIndices, 0, 0 );
    pd3dDevice->UpdateSubresource( m_pParticleVB_PosX, NULL, NULL, m_pCPUParticles.PosX, 0, 0 );
    pd3dDevice->UpdateSubresource( m_pParticleVB_PosY, NULL, NULL, m_pCPUParticles.PosY, 0, 0 );
    pd3dDevice->UpdateSubresource( m_pParticleVB_PosZ, NULL, NULL, m_pCPUParticles.PosZ, 0, 0 );
    pd3dDevice->UpdateSubresource( m_pParticleVB_Color, NULL, NULL, m_pCPUParticles.ColorCode, 0, 0 );
    pd3dDevice->UpdateSubresource( m_pParticleVB_Mass, NULL, NULL, m_pCPUParticles.Mass, 0, 0 );

    pd3dDevice->IASetInputLayout( m_pParticleVertexLayout );
    ID3D10Buffer *pBuffers[5] = { m_pParticleVB_PosX,m_pParticleVB_PosY,m_pParticleVB_PosZ,m_pParticleVB_Color, m_pParticleVB_Mass};
    strides[0] = sizeof(float);
    strides[1] = sizeof(float);
    strides[2] = sizeof(float);
    strides[3] = sizeof(float);
    strides[4] = sizeof(float);
    offsets[0] = 0;
    offsets[1] = 0;
    offsets[2] = 0;
    offsets[3] = 0;
    offsets[4] = 0;
    pd3dDevice->IASetVertexBuffers( 0, 5, pBuffers, strides, offsets );
    pd3dDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_POINTLIST );
    pd3dDevice->IASetIndexBuffer( m_pParticleIB, DXGI_FORMAT_R32_UINT, 0 );

    m_fMass0Variable->SetFloat(m_pSim->GetMinMass());
    m_fMass1Variable->SetFloat(m_pSim->GetMaxMass());

    m_pTechnique->GetDesc( &techDesc );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        m_pTechnique->GetPassByIndex( p )->Apply(0);
        pd3dDevice->DrawIndexed( m_nNBodyCount, 0, 0 );
    }
}

void CSample_NBody::OnRenderText(CDXUTTextHelper* pTextHelper, double fTime, float fElapsedTime)
{
    // Call the base class version, this displays the "Show help F1" or "Hide help F1" text messages.
    __super::OnRenderText(pTextHelper, fTime, fElapsedTime);

    WCHAR*  out = new WCHAR[1024];
    float fps = DXUTGetFPS();
    m_fpsMax = max(m_fpsMax,fps);
    pTextHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );

    StringCchPrintf(out, 256, L"Maximum framerate: %.2f fps", m_fpsMax );
    pTextHelper->DrawTextLine( out);

    // Print OCL device
    WCHAR str[256];
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, out, 128, &g_device_name[0], _TRUNCATE);
    if(m_nOptimization>2)
        StringCchPrintf(str, 256, L"OpenCL device: %s", out);
    else
        StringCchPrintf(str, 256, L"Running on the CPU");
    pTextHelper->DrawFormattedTextLine( str );
    delete out;


    // Display help text if it is required by the framework
    if( GetDrawHelp() )
    {
        HRESULT hr;

        UINT nBackBufferHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;

        // Set an insertion point six lines up from the bottom
        pTextHelper->SetInsertionPos(5, nBackBufferHeight - 15 * 7);
        pTextHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f));
        V(pTextHelper->DrawTextLine(L"Controls:"));

        pTextHelper->SetInsertionPos(20, nBackBufferHeight - 15 * 6);
        pTextHelper->DrawTextLine(L"Rotate camera: Left mouse button\n"
                                  L"Zoom camera:   Mouse wheel scroll\n"
                                  L"Reset camera position: Home\n"
                                  L"Toggle simulation: Space\n"
                                  L"Quit: ESC\n");
    }
}

void CSample_NBody::OnClearRenderTargets(ID3D10Device* pd3dDevice, double fTime, float fElapsedTime)
{
    __super::OnClearRenderTargets(pd3dDevice, fTime, fElapsedTime);
    ID3D10RenderTargetView *pRTV = DXUTGetD3D10RenderTargetView();
    pd3dDevice->ClearRenderTargetView(pRTV, D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f));
}

/*HRESULT CSample_NBody::OnResizedSwapChain(ID3D10Device* pd3dDevice,
                                           IDXGISwapChain *pSwapChain,
                                           const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc)
{
    return __super::OnResizedSwapChain( pd3dDevice, pSwapChain, in_pBufferSurfaceDesc );
}

void CSample_NBody::OnFrameMove(double fTime, float fElapsedTime)
{
    __super::OnFrameMove(fTime, fElapsedTime);
}

LRESULT CSample_NBody::MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool *pbNoFurtherProcessing)
{
    LRESULT res = __super::MsgProc(hWnd, uMsg, wParam, lParam, pbNoFurtherProcessing);
    if( *pbNoFurtherProcessing || 0 != res )
        return res;

    return 0;
}

void CSample_NBody::KeyboardProc(UINT nChar, bool bKeyDown, bool bAltDown)
{
}*/

// end of file
