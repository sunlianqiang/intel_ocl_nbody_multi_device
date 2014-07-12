/* ************************************************************************* *\
                  INTEL CORPORATION PROPRIETARY INFORMATION
     This software is supplied under the terms of a license agreement or
     nondisclosure agreement with Intel Corporation and may not be copied
     or disclosed except in accordance with the terms of that agreement.
          Copyright (C) 2007 Intel Corporation. All Rights Reserved.
\* ************************************************************************* */

/// @file Widgets.h
///
/// Definition of the CWidgetBase class and its derived classes
///
/// Widgets used to share the common functionality across several samples.
/// Often it used as aggregate of the extension class but can be directly used in the user
/// application class. The superclass is responsible for proper calling of widget's member function.


#pragma once

// forward declaration
class CSampleBase;


/// Base class for all widgets.
class CWidgetBase
{
public:
    CSampleBase* GetApp() const { return m_pApp; }

    UINT GetID() const  { return m_id; }
    void SetID(UINT id) { m_id = id;   }

protected:
    /// Constructor
    CWidgetBase(CSampleBase *app ///< Pointer to the application object.
               );

    /// Pure virtual destructor.
    virtual ~CWidgetBase() = 0;

private:
    CWidgetBase(const CWidgetBase&);
    CWidgetBase& operator = (const CWidgetBase&);

    CSampleBase *m_pApp; ///< Pointer to the application object.
    UINT         m_id;   ///< User-defined widget id. The framework doesn't use this value.
};

///////////////////////////////////////////////////////////////////////////////

// forward declaration
class CBaseCamera;

/// Camera widget class
class CCameraWidget : public CWidgetBase
{
public:
    CBaseCamera* GetCamera();
    void CreateFirstPersonCamera();
    void CreateModelViewerCamera();

    /// Notification codes are passed in wParam when the widget calls OnWidgetNotify
    enum NotifyType
    {
        NOTIFY_MOVE,     ///< camera has been moved
        NOTIFY_RESIZE,   ///< viewport size is changing
    };

    /// Structure used in NOTIFY_RESIZE. Pointer to a structure is passed in lParam.
    struct CameraProjParams
    {
        float fFOV;
        float fAspect;
        float fNearPlane;
        float fFarPlane;
    };

    CCameraWidget(CSampleBase *app); ///< Constructor
    ~CCameraWidget();                ///< Destructor

    /// Called by the application. The meaning is the same as CSampleBase::OnFrameMove
    void OnFrameMove(double fTime, float fElapsedTime);

    /// Called by the application. The meaning is the same as CSampleBase::MsgProc
    LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool *pbNoFurtherProcessing);

    /// Called by the application. The meaning is the same as CSampleBase::OnResizedSwapChain
    HRESULT OnResizedSwapChain(ID3D10Device* pd3dDevice,
        IDXGISwapChain *pSwapChain,
        const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc
        );

    /// Called by the application during its initialization.
    void OnInitApp();

    /// methods that should have existed in CBaseCamera
    float GetFOV();
    float GetAspectRatio();
private:
    CBaseCamera *m_pCamera;
};

///////////////////////////////////////////////////////////////////////////////

// forward declaration
class CDXUTDirectionWidget;

namespace widgets_detail
{
    struct LightMesh
    {
        ID3D10Buffer* pVB;
        ID3D10Buffer* pIB;
        UINT indexCount;
        LightMesh() : pVB(NULL), pIB(NULL), indexCount(0) {}
    };
};

/// Lighting widget
class CLightingWidget : public CWidgetBase
{
public:
    /// Type of light used in the widget
    enum LightType
    {
        LIGHT_DIRECTIONAL,    ///< directional light
        LIGHT_POINT,                ///< point light
    };

    /// Notification codes are passed in wParam when the widget calls OnWidgetNotify
    enum NotifyType
    {
        NOTIFY_INIT,      ///< widget is initializing
        NOTIFY_MOVE,      ///< light has been dragged
    };
    /// Structure used in NOTIFY_INIT. Pointer to a structure is passed in lParam.
    struct LightInitParams
    {
        UINT  maxCount;   ///< Maximum count of light sources.
        BOOL  bShowScale;
    };

    /// An application have to pass an actual view matrix if the camera moves
    void UpdateViewMatrix(const D3DXMATRIX* pmView);

    /// Render the light arrow so the user can visually see the light direction
    void RenderLightArrows(ID3D10Device* pd3dDevice, ///< Pointer to the ID3D10Device Interface device used for rendering.
        const D3DXMATRIX* pmView, ///< Pointer to the view matrix
        const D3DXMATRIX* pmProj
        );

    void ApplyLights(
        ID3D10EffectVectorVariable* in_pLightDir,
        ID3D10EffectVectorVariable* in_pDiffuse = NULL,
        ID3D10EffectScalarVariable* in_pnNumLights = NULL);

    CDXUTDirectionWidget* GetWidgetControl(UINT num) const;

    void SetRadius(float radius);
    float GetRadius() const;
    UINT GetActiveLightCount() const { return m_nNumActiveLights; }
    void SetActiveLightCount(UINT count);

    void  SetLightScale(UINT idx, float scl);
    float GetLightScale(UINT idx) const;

    void SetLightType(LightType type);
    LightType GetLightType() const { return m_lightType; }

    CLightingWidget(CSampleBase *app);  ///< Constructor
    ~CLightingWidget();                 ///< Destructor

    /// Called by the application. The meaning is the same as CSampleBase::OnCreateDevice
    HRESULT OnCreateDevice(ID3D10Device* pd3dDevice, ///< Pointer to the newly created ID3D10Device Interface device.
        const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc ///< Pointer to the back buffer surface description.
        );

    /// Called by the application. The meaning is the same as CSampleBase::OnDestroyDevice
    void OnDestroyDevice();

    void OnInitApp();

    /// Called by the application. The meaning is the same as CSampleBase::MsgProc
    LRESULT MsgProc(HWND hWnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam,
        bool* pbNoFurtherProcessing
        );

    /// Called by the application. The meaning is the same as CSampleBase::OnGUIEvent
    void OnGUIEvent(UINT nEvent,
        int nControlID,
        CDXUTControl* pControl
        );

    /// Called by the application. The meaning is the same as CSampleBase::OnResizedSwapChain
    HRESULT OnResizedSwapChain(ID3D10Device* pd3dDevice,
        IDXGISwapChain *pSwapChain,
        const DXGI_SURFACE_DESC* in_pBufferSurfaceDesc
        );
private:
    widgets_detail::LightMesh    m_lightMesh;
    bool                         m_bMeshLoaded;
    CDXUTDirectionWidget*        m_pLightControls;
    UINT                         m_nLightCount;  ///< total light count

    float*                       m_fLightScale;
    UINT                         m_nNumActiveLights;
    UINT                         m_nActiveLight;           ///< index of the active light

    ///< Interface to manage the set of state objects, resources and shaders for the effect
    ID3D10Effect*                m_pEffect;
    ///< The techniques and their collection of passes used for light arrow drawing
    ID3D10EffectTechnique*       m_pTechnique;
    ID3D10InputLayout*           m_pInputLayout;

    ID3D10EffectMatrixVariable*  m_pWorldViewProjVariable;
    ID3D10EffectMatrixVariable*  m_pWorldViewVariable;
    ID3D10EffectVectorVariable*  m_pMaterialDiffuse;

    float m_radius;

    LightType m_lightType;

    static const int IDC_NUM_LIGHTS          = 10;
    static const int IDC_NUM_LIGHTS_STATIC   = 11;
    static const int IDC_ACTIVE_LIGHT        = 12;
    static const int IDC_LIGHT_SCALE         = 13;
    static const int IDC_LIGHT_SCALE_STATIC  = 14;
};

// end of file
