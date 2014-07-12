/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2007 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

/// @file Widgets.cpp

#include "stdafx.h"

#include "Widgets.h"
#include "SampleBase.h"
#include "LightData.h"

using namespace widgets_detail;

template<UINT vertexCount, UINT indexCount>
static void CreateMesh(ID3D10Device* pd3dDevice, LightMesh &lightMesh,
                       const float (&vertexData)[vertexCount], const UINT (&indexData)[indexCount])
{
    D3D10_BUFFER_DESC vBufferDesc;
    vBufferDesc.Usage            = D3D10_USAGE_DEFAULT;
    vBufferDesc.ByteWidth        = vertexCount*sizeof(float);
    vBufferDesc.BindFlags        = D3D10_BIND_VERTEX_BUFFER;
    vBufferDesc.CPUAccessFlags   = 0;
    vBufferDesc.MiscFlags        = 0;

    D3D10_SUBRESOURCE_DATA vInitData;
    vInitData.pSysMem = vertexData;
    vInitData.SysMemPitch = 0;
    vInitData.SysMemSlicePitch = 0;

    assert(!lightMesh.pVB);
    pd3dDevice->CreateBuffer( &vBufferDesc, &vInitData, &lightMesh.pVB );

    D3D10_BUFFER_DESC iBufferDesc;
    iBufferDesc.Usage            = D3D10_USAGE_DEFAULT;
    iBufferDesc.ByteWidth        = indexCount*sizeof(UINT);
    iBufferDesc.BindFlags        = D3D10_BIND_INDEX_BUFFER;
    iBufferDesc.CPUAccessFlags   = 0;
    iBufferDesc.MiscFlags        = 0;

    lightMesh.indexCount = indexCount;

    D3D10_SUBRESOURCE_DATA iInitData;
    iInitData.pSysMem = indexData;
    iInitData.SysMemPitch = 0;
    iInitData.SysMemSlicePitch = 0;

    assert(!lightMesh.pIB);
    pd3dDevice->CreateBuffer( &iBufferDesc, &iInitData, &lightMesh.pIB );
}

static void ReleaseMesh(LightMesh &lightMesh)
{
    SAFE_RELEASE(lightMesh.pVB);
    SAFE_RELEASE(lightMesh.pIB);
    lightMesh.indexCount = 0;
}

///////////////////////////////////////////////////////////////////////////////

CWidgetBase::CWidgetBase(CSampleBase *app)
  : m_pApp(app)
  , m_id(0)
{
}

CWidgetBase::~CWidgetBase()
{
}

///////////////////////////////////////////////////////////////////////////////

namespace
{
    class MyModelCamera : public CModelViewerCamera
    {
        virtual void Reset()
        {
            // bypass CModelViewerCamera::Reset call
            float rdef = m_fDefaultRadius;
            float rmin = m_fMinRadius;
            float rmax = m_fMaxRadius;
            SetViewParams(&m_vDefaultEye, &m_vDefaultLookAt);
            SetRadius(rdef, rmin, rmax);
        }
    };
}


CBaseCamera* CCameraWidget::GetCamera()
{
    return m_pCamera;
}

void CCameraWidget::CreateFirstPersonCamera()
{
    SAFE_DELETE(m_pCamera);
    m_pCamera = new CFirstPersonCamera();
}

void CCameraWidget::CreateModelViewerCamera()
{
    SAFE_DELETE(m_pCamera);
    m_pCamera = new MyModelCamera();
    static_cast<MyModelCamera*>(m_pCamera)
        ->SetButtonMasks(MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON);
}

CCameraWidget::CCameraWidget(CSampleBase *app) : CWidgetBase(app)
{
    m_pCamera = NULL;
}

CCameraWidget::~CCameraWidget()
{
    SAFE_DELETE(m_pCamera);
}

void CCameraWidget::OnFrameMove(double fTime, float fElapsedTime)
{
    // Update the camera's position based on user input
    m_pCamera->FrameMove(fElapsedTime);
}

LRESULT CCameraWidget::MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam,
                              LPARAM lParam, bool* pbNoFurtherProcessing )
{
    m_pCamera->HandleMessages( hWnd, uMsg, wParam, lParam );
    if( m_pCamera->IsBeingDragged() )
        GetApp()->OnWidgetNotify(this, (WPARAM) NOTIFY_MOVE, 0);

    return 0;
}

HRESULT CCameraWidget::OnResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain,
                                           const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc )
{
    float fAspectRatio = static_cast<float>(in_pBufferSurfaceDesc->Width)
        / static_cast<float>(in_pBufferSurfaceDesc->Height);

    CameraProjParams cpp;
    cpp.fAspect = fAspectRatio;
    cpp.fFOV = GetFOV();
    cpp.fNearPlane = m_pCamera->GetNearClip();
    cpp.fFarPlane  = m_pCamera->GetFarClip();

    GetApp()->OnWidgetNotify(this, (WPARAM) NOTIFY_RESIZE, (LPARAM) &cpp);

    // Setup the camera's projection parameters
    m_pCamera->SetProjParams( cpp.fFOV, cpp.fAspect, cpp.fNearPlane, cpp.fFarPlane );
    if( CModelViewerCamera *cam = dynamic_cast<CModelViewerCamera*>(GetCamera()) )
    {
        cam->SetWindow(in_pBufferSurfaceDesc->Width, in_pBufferSurfaceDesc->Height);
    }

    return S_OK;
}

void CCameraWidget::OnInitApp()
{
    CreateModelViewerCamera();

    // Initialize the camera
    D3DXVECTOR3 Eye( 0.0f, 0.0f, -10.0f );
    D3DXVECTOR3 At( 0.0f, 0.0f, 0.0f );
    m_pCamera->SetViewParams( &Eye, &At );
}

// work-around for missing methods in CBaseCamera DXUT class
class CBaseCameraExtension : public CBaseCamera
{
public:
    /// methods that should have existed in CBaseCamera
    float GetFOV()         { return m_fFOV; }
    float GetAspectRatio() { return m_fAspect; }
};

float CCameraWidget::GetFOV()
{
    return static_cast<CBaseCameraExtension*>(m_pCamera)->GetFOV();
}

float CCameraWidget::GetAspectRatio()
{
    return static_cast<CBaseCameraExtension*>(m_pCamera)->GetAspectRatio();
}


///////////////////////////////////////////////////////////////////////////////

CLightingWidget::CLightingWidget(CSampleBase *app) : CWidgetBase(app)
  , m_pLightControls(NULL)
  , m_nLightCount(0)
  , m_nNumActiveLights(0)
  , m_nActiveLight(0)
  , m_radius(2)
  , m_fLightScale(NULL)
  , m_lightType(CLightingWidget::LIGHT_DIRECTIONAL)
  , m_bMeshLoaded(false)
{
}

CLightingWidget::~CLightingWidget()
{
    SAFE_DELETE_ARRAY(m_pLightControls);
    delete []m_fLightScale;
}

HRESULT CLightingWidget::OnCreateDevice( ID3D10Device* pd3dDevice,
                                         const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc )
{
    HRESULT hr;

    for( UINT i = 0; i < m_nLightCount; ++i )
        m_pLightControls[i].SetRadius( 100.0f ); // FIXME: object radius


    //
    // Read the D3DX effect file
    //

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

    WCHAR str[MAX_PATH];
    V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, L"light.fx"));
    V_RETURN(D3DX10CreateEffectFromFile(str, NULL, NULL, "fx_4_0", dwShaderFlags, 0,
        pd3dDevice, NULL, NULL, &m_pEffect, NULL, NULL));

    // Obtain technique
    m_pTechnique             = m_pEffect->GetTechniqueByName("Render");

    // Obtain variables
    m_pWorldViewProjVariable = m_pEffect->GetVariableByName("WorldViewProj")->AsMatrix();
    m_pWorldViewVariable     = m_pEffect->GetVariableByName("WorldView")->AsMatrix();
    m_pMaterialDiffuse       = m_pEffect->GetVariableByName("MaterialDiffuse")->AsVector();

    //
    // Create vertex input layout
    //

    const D3D10_INPUT_ELEMENT_DESC layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D10_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0},
    };

    D3D10_PASS_DESC pd;
    V_RETURN(m_pTechnique->GetPassByIndex(0)->GetDesc(&pd));
    V_RETURN(pd3dDevice->CreateInputLayout(layout, sizeof(layout) / sizeof(layout[0]),
        pd.pIAInputSignature, pd.IAInputSignatureSize, &m_pInputLayout));

    return S_OK;
}

void CLightingWidget::OnDestroyDevice()
{
    CDXUTDirectionWidget::StaticOnD3D10DestroyDevice();
    if( m_bMeshLoaded )
    {
        ReleaseMesh(m_lightMesh);
        m_bMeshLoaded = false;
    }
    SAFE_RELEASE(m_pEffect);
    SAFE_RELEASE(m_pInputLayout);
}

void CLightingWidget::OnInitApp()
{
    LightInitParams params;
    params.maxCount   = 3;
    params.bShowScale = TRUE;

    GetApp()->OnWidgetNotify(this, (WPARAM) NOTIFY_INIT, (LPARAM) &params);

    m_nLightCount      = params.maxCount;
    m_nActiveLight     = 0;
    m_nNumActiveLights = 1;

    m_fLightScale = new float[params.maxCount];
    m_pLightControls   = new CDXUTDirectionWidget[m_nLightCount];

    for( UINT i = 0; i < m_nLightCount; ++i )
    {
        m_fLightScale[i] = 1.0f;
        m_pLightControls[i].SetLightDirection( D3DXVECTOR3(
            sinf(D3DX_PI * 2 * i / (float) (m_nLightCount) - D3DX_PI / 6),
            0,
            -cosf(D3DX_PI * 2 * i / (float) (m_nLightCount) - D3DX_PI / 6) ) );
    }

    WCHAR sz[100];

    if( m_nLightCount > 1 || params.bShowScale )
    {
        GetApp()->GetNextYPosHUD(24); // offset from previous block
    }

    // Create light selector slider only if we have more than 1 light
    if( m_nLightCount > 1 )
    {
        swprintf_s( sz, 100, L"# Lights: %d", m_nNumActiveLights);
        GetApp()->GetHUD()->AddStatic(IDC_NUM_LIGHTS_STATIC, sz, 35, GetApp()->GetNextYPosHUD(24), 125, 22);
        GetApp()->GetHUD()->AddSlider(IDC_NUM_LIGHTS, 50, GetApp()->GetNextYPosHUD(24),
            100, 22, 1, m_nLightCount, m_nNumActiveLights);
    }

    if( params.bShowScale )
    {
        swprintf_s(sz, 100, L"Light scale: %0.2f", m_fLightScale[m_nActiveLight]);
        GetApp()->GetHUD()->AddStatic(IDC_LIGHT_SCALE_STATIC, sz, 35, GetApp()->GetNextYPosHUD(24), 125, 22);
        GetApp()->GetHUD()->AddSlider(IDC_LIGHT_SCALE, 50, GetApp()->GetNextYPosHUD(24),
            100, 22, 0, 20, (int) (m_fLightScale[m_nActiveLight] * 10.0f));
    }

    // Create light selection button only if we have more than 1 light
    if( m_nLightCount > 1 )
    {
        GetApp()->GetHUD()->AddButton(IDC_ACTIVE_LIGHT, L"Change active light (K)",
            35, GetApp()->GetNextYPosHUD(24), 125, 22, 'K');
    }
}

LRESULT CLightingWidget::MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam, bool* pbNoFurtherProcessing )
{
    m_pLightControls[m_nActiveLight].HandleMessages( hWnd, uMsg, wParam, lParam );

    if( m_pLightControls[m_nActiveLight].IsBeingDragged() )
        GetApp()->OnWidgetNotify(this, (WPARAM) NOTIFY_MOVE, 0);

    return 0;
}

void CLightingWidget::OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl)
{
    WCHAR sz[100];
    switch( nControlID )
    {
    case IDC_ACTIVE_LIGHT:
        if( !m_pLightControls[m_nActiveLight].IsBeingDragged() )
        {
            m_nActiveLight = (m_nActiveLight + 1) % m_nNumActiveLights;
            GetApp()->GetHUD()->GetSlider(IDC_LIGHT_SCALE)->SetValue(int(m_fLightScale[m_nActiveLight]*10.0f));
            swprintf_s(sz, L"Light scale: %0.2f", m_fLightScale[m_nActiveLight]);
            GetApp()->GetHUD()->GetStatic(IDC_LIGHT_SCALE_STATIC)->SetText(sz);
        }
        break;

    case IDC_NUM_LIGHTS:
        if( !m_pLightControls[m_nActiveLight].IsBeingDragged() )
        {
            swprintf_s(sz, L"# Lights: %d", GetApp()->GetHUD()->GetSlider(IDC_NUM_LIGHTS)->GetValue());

            GetApp()->GetHUD()->GetStatic(IDC_NUM_LIGHTS_STATIC)->SetText( sz );
            m_nNumActiveLights = GetApp()->GetHUD()->GetSlider(IDC_NUM_LIGHTS)->GetValue();
            m_nActiveLight %= m_nNumActiveLights;
        }
        break;

    case IDC_LIGHT_SCALE:
        m_fLightScale[m_nActiveLight] = (float) (GetApp()->GetHUD()->GetSlider(IDC_LIGHT_SCALE)->GetValue() * 0.10f);
        swprintf_s(sz, L"Light scale: %0.2f", m_fLightScale[m_nActiveLight]);
        GetApp()->GetHUD()->GetStatic(IDC_LIGHT_SCALE_STATIC)->SetText(sz);
        break;
    }
}


HRESULT CLightingWidget::OnResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain,
                                             const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc )
{
    // a little hack to access the protected member of CDXUTDirectionWidget
    class MyAccessor : public CDXUTDirectionWidget
    {
    public:
        void SetWindow(UINT w, UINT h) { m_ArcBall.SetWindow(w, h); }
    };

    for( UINT i = 0; i < m_nLightCount; ++i )
    {
        static_cast<MyAccessor&>(m_pLightControls[i]).SetWindow(
            in_pBufferSurfaceDesc->Width, in_pBufferSurfaceDesc->Height);
    }

    return S_OK;
}


void CLightingWidget::UpdateViewMatrix(const D3DXMATRIX* pmView)
{
    // helper class to access the protected member of CDXUTDirectionWidget
    class MyAccessor : public CDXUTDirectionWidget
    {
    public:
        void SetViewMatrix(const D3DXMATRIX *pmView) { m_mView = *pmView; }
    };

    for( UINT i = 0; i < m_nLightCount; ++i )
    {
        static_cast<MyAccessor&>(m_pLightControls[i]).SetViewMatrix(pmView);
    }
}

void CLightingWidget::RenderLightArrows(ID3D10Device* pd3dDevice, const D3DXMATRIX* pmView, const D3DXMATRIX* pmProj)
{
    HRESULT hr;

    if( !m_bMeshLoaded )
    {
        // load mesh
        switch( m_lightType )
        {
        case LIGHT_DIRECTIONAL:
            CreateMesh(pd3dDevice, m_lightMesh, g_arrowVertex, g_arrowIndex);
            break;
        case LIGHT_POINT:
            CreateMesh(pd3dDevice, m_lightMesh, g_bulbVertex, g_bulbIndex);
            break;
        default:
            assert(false);
        }
        m_bMeshLoaded = true;
    }


    for( UINT i = 0; i < m_nNumActiveLights; ++i )
    {
        // Render the light spheres so the user can visually see the light dir
        D3DXMATRIX mRotate;
        D3DXMATRIX mScale;
        D3DXMATRIX mTrans;

        // Rotate arrow model to point towards origin
        D3DXVECTOR3 vAt = D3DXVECTOR3(0,0,0);
        D3DXVECTOR3 vUp = D3DXVECTOR3(0,1,0);
        D3DXVECTOR3 vCurrentDir = m_pLightControls[i].GetLightDirection();

        if( LIGHT_DIRECTIONAL == m_lightType )
        {
            D3DXMATRIX mRotateA, mRotateB;
            D3DXMatrixRotationX(&mRotateB, D3DX_PI);
            D3DXMatrixLookAtLH(&mRotateA, &vCurrentDir, &vAt, &vUp);
            D3DXMatrixInverse(&mRotateA, NULL, &mRotateA);
            mRotate = mRotateB * mRotateA;
        }
        else if( LIGHT_POINT == m_lightType )
        {
            vUp = D3DXVECTOR3(0, -1, 0);
            D3DXVECTOR3 v0;
            D3DXVec3Cross(&v0, &vCurrentDir, &vUp);
            D3DXMatrixRotationAxis(&mRotate, &v0, acos(D3DXVec3Dot(&vCurrentDir, &vUp)));
            D3DXMatrixInverse(&mRotate, NULL, &mRotate);
        }

        D3DXVECTOR3 vL = vCurrentDir * m_radius;
        D3DXMatrixTranslation(&mTrans, vL.x, vL.y, vL.z);
        D3DXMatrixScaling(&mScale, m_radius * 0.1f, m_radius * 0.1f, m_radius * 0.1f);

        D3DXMATRIX mWorldView = (mRotate * mScale * mTrans) * (*pmView);
        D3DXMATRIX mWorldViewProj = mWorldView * (*pmProj);

        m_pWorldViewVariable->SetMatrix((float *) &mWorldView);
        m_pWorldViewProjVariable->SetMatrix((float *) &mWorldViewProj);

        D3DXCOLOR color = (i == m_nActiveLight) ? D3DXVECTOR4(1,1,0,1) : D3DXVECTOR4(1,1,1,1);
        m_pMaterialDiffuse->SetFloatVector((float *) &color);


        //
        // Render
        //

        UINT strides[1] = {sizeof(float) * 6};
        UINT offsets[1] = {0};

        pd3dDevice->IASetInputLayout(m_pInputLayout);
        pd3dDevice->IASetVertexBuffers(0, 1, &m_lightMesh.pVB, strides, offsets);
        pd3dDevice->IASetIndexBuffer(m_lightMesh.pIB, DXGI_FORMAT_R32_UINT, 0);
        pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        D3D10_TECHNIQUE_DESC techDesc;
        m_pTechnique->GetDesc( &techDesc );
        for( UINT p = 0; p < techDesc.Passes; ++p )
        {
            V(m_pTechnique->GetPassByIndex(p)->Apply(0));
            pd3dDevice->DrawIndexed(m_lightMesh.indexCount, 0, 0);
        }
    }
}

void CLightingWidget::ApplyLights( ID3D10EffectVectorVariable* in_pLightDir,
                                   ID3D10EffectVectorVariable* in_pDiffuse,
                                   ID3D10EffectScalarVariable* in_pnNumLights )
{
    HRESULT hr;

    D3DXVECTOR4* vLightDir = new D3DXVECTOR4[m_nLightCount];
    D3DXVECTOR4* vLightDiffuse  = new D3DXVECTOR4[m_nLightCount];

    for( UINT i = 0; i < m_nNumActiveLights; ++i )
    {
        reinterpret_cast<D3DXVECTOR3&>(vLightDir[i]) = m_pLightControls[i].GetLightDirection();
        vLightDir[i].w = 1;
        vLightDiffuse[i] = m_fLightScale[i] * D3DXVECTOR4(1,1,1,1);
    }

    V(in_pLightDir->SetRawValue(vLightDir, 0, sizeof(D3DXVECTOR4) * m_nLightCount));

    if( in_pDiffuse )
    {
        V(in_pDiffuse->SetFloatVectorArray(vLightDiffuse[0], 0, m_nLightCount));
    }

    if( in_pnNumLights )
    {
        V(in_pnNumLights->SetInt(m_nNumActiveLights));
    }

    delete [] vLightDir;
    delete [] vLightDiffuse;
}

CDXUTDirectionWidget* CLightingWidget::GetWidgetControl(UINT num) const
{
    return num < m_nLightCount ? m_pLightControls + num : NULL;
}

void CLightingWidget::SetRadius(FLOAT radius)
{
    m_radius = radius;
}

FLOAT CLightingWidget::GetRadius() const
{
    return m_radius;
}

void CLightingWidget::SetLightScale(UINT idx, float scl)
{
    if( scl != m_fLightScale[idx] )
    {
        m_fLightScale[idx] = scl;
        if( idx == m_nActiveLight )
        {
            WCHAR sz[100];
            GetApp()->GetHUD()->GetSlider(IDC_LIGHT_SCALE)->SetValue(int(m_fLightScale[m_nActiveLight]*10.0f));
            swprintf_s(sz, 100, L"Light scale: %0.2f", m_fLightScale[m_nActiveLight]);
            GetApp()->GetHUD()->GetStatic(IDC_LIGHT_SCALE_STATIC)->SetText(sz);
        }
    }
}

float CLightingWidget::GetLightScale(UINT idx) const
{
    return m_fLightScale[idx];
}

void CLightingWidget::SetActiveLightCount(UINT count)
{
    if( m_nNumActiveLights != count && count <= m_nLightCount )
    {
        WCHAR sz[100];
        swprintf_s(sz, L"# Lights: %d", count);
        GetApp()->GetHUD()->GetStatic(IDC_NUM_LIGHTS_STATIC)->SetText(sz);
        GetApp()->GetHUD()->GetSlider(IDC_NUM_LIGHTS)->SetValue(count);
        m_nNumActiveLights = count;
        m_nActiveLight %= m_nNumActiveLights;
    }
}

void CLightingWidget::SetLightType(LightType type)
{
    m_lightType = type;
    if( m_bMeshLoaded )
    {
        ReleaseMesh(m_lightMesh);
        m_bMeshLoaded = false;
    }
}

// end of file
