#include "../Precompiled.h"

#include "VR.h"

#include "../Core/CoreEvents.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/RenderPath.h"
#include "../Graphics/Texture2D.h"
#include "../IO/Log.h"
#include "../Scene/Scene.h"

// hack.cb fix OpenVR C++ static lib exports
#define VR_API_EXPORT 1
#include <openvr/openvr.h>

#include "../DebugNew.h"

namespace
{
    const float DEFAULT_NEAR_CLIP = 0.1f;
    const float DEFAULT_FAR_CLIP = 100.0f;

    const float MIN_RENDER_RESOLUTION_SCALE = 0.25f;
    const float MAX_RENDER_RESOLUTION_SCALE = 8.0f;
    const float DEFAULT_RENDER_RESOLUTION_SCALE = 1.0f;

    Urho3D::Matrix3x4 OpenVRAffineTransformToMatrix3x4(vr::HmdMatrix34_t const& in)
    {
        return Urho3D::Matrix3x4(
             in.m[0][0],  in.m[0][1], -in.m[0][2],  in.m[0][3],
             in.m[1][0],  in.m[1][1], -in.m[1][2],  in.m[1][3],
            -in.m[2][0], -in.m[2][1],  in.m[2][2], -in.m[2][3]
        );
    }

    Urho3D::Matrix4 OpenVRProjectionToMatrix4(vr::HmdMatrix44_t const& in)
    {
#if 0
        float l, r, t, b, zn = 0.1f, zf = 30.f;
        vrSystem_->GetProjectionRaw(static_cast<vr::EVREye>(eye), &l, &r, &t, &b);

        // D3DXMatrixPerspectiveOffCenterLH
        vrData_->eyeProjection_[eye].m00_ = 2.0f / (r - l);
        vrData_->eyeProjection_[eye].m01_ = 0;
        vrData_->eyeProjection_[eye].m02_ = (l + r) / (l - r);
        vrData_->eyeProjection_[eye].m03_ = 0;

        vrData_->eyeProjection_[eye].m10_ = 0;
        vrData_->eyeProjection_[eye].m11_ = 2.0f / (b - t);
        vrData_->eyeProjection_[eye].m12_ = (t + b) / (b - t);
        vrData_->eyeProjection_[eye].m13_ = 0;

        vrData_->eyeProjection_[eye].m20_ = 0;
        vrData_->eyeProjection_[eye].m21_ = 0;
        vrData_->eyeProjection_[eye].m22_ = zf / (zf - zn);
        vrData_->eyeProjection_[eye].m23_ = zn * zf / (zn - zf);

        vrData_->eyeProjection_[eye].m30_ = 0;
        vrData_->eyeProjection_[eye].m31_ = 0;
        vrData_->eyeProjection_[eye].m32_ = 1;
        vrData_->eyeProjection_[eye].m33_ = 0;
#endif
        return Urho3D::Matrix4(
            in.m[0][0], in.m[0][1], -in.m[0][2], in.m[0][3],
            in.m[1][0], in.m[1][1], -in.m[1][2], in.m[1][3],
            in.m[2][0], in.m[2][1], -in.m[2][2], in.m[2][3],
            in.m[3][0], in.m[3][1], -in.m[3][2], in.m[3][3]
        );
    }
}

namespace Urho3D
{

/// %VR implementation. Holds API-specific objects.
class URHO3D_API VRImpl
{
    friend class VR;

public:
    /// Construct.
    VRImpl();

private:
    /// OpenVR VRSystem interface
    vr::IVRSystem* vrSystem_;
};

VRImpl::VRImpl() :
    vrSystem_(0)
{

}

VR::VR(Context* context_) :
    Object(context_),
    vrImpl_(new VRImpl()),
    renderResolutionScale_(DEFAULT_RENDER_RESOLUTION_SCALE),
    nearClip_(DEFAULT_NEAR_CLIP),
    farClip_(DEFAULT_FAR_CLIP),
    renderParamsDirty_(false),
    currentFrame_(0),
    posesUpdatedThisFrame_(false)
{
    if (InitializeVR() == 0)
    {
        SubscribeToEvent(E_BEGINFRAME, URHO3D_HANDLER(VR, HandleBeginFrame));
    }
}

VR::~VR()
{
    ShutdownVR();
}

void VR::SetRenderResolutionScale(float renderResolutionScale)
{
    const float clampedScale = Clamp(renderResolutionScale, MIN_RENDER_RESOLUTION_SCALE, MAX_RENDER_RESOLUTION_SCALE);
    const bool usedClampedScale = (clampedScale != renderResolutionScale);
    if (renderResolutionScale_ != clampedScale)
    {
        renderResolutionScale_ = clampedScale;
        renderParamsDirty_ = true;

        if (usedClampedScale)
        {
            URHO3D_LOGERRORF("VR renderResolutionScale %f was clamped to %f in valid range [%f, %f]",
                renderResolutionScale,
                clampedScale,
                MIN_RENDER_RESOLUTION_SCALE,
                MAX_RENDER_RESOLUTION_SCALE);
        }
    }
}

void VR::SetNearClip(float nearClip)
{
    const float clampedClip = Max(nearClip, M_MIN_NEARCLIP);
    const bool usedClampedClip = nearClip != clampedClip;
    if (nearClip_ != clampedClip)
    {
        nearClip_ = clampedClip;
        renderParamsDirty_ = true;

        if (usedClampedClip)
        {
            URHO3D_LOGERRORF("VR nearClip %f was clamped to %f", nearClip, clampedClip);
        }
    }
}

void VR::SetFarClip(float farClip)
{
    const float clampedClip = Max(farClip, M_MIN_NEARCLIP);
    const bool usedClampedClip = farClip != clampedClip;
    if (farClip_ != clampedClip)
    {
        farClip_ = clampedClip;
        renderParamsDirty_ = true;

        if (usedClampedClip)
        {
            URHO3D_LOGERRORF("VR farClip %f was clamped to %f", farClip, clampedClip);
        }
    }
}

void VR::SetRenderPath(RenderPath* renderPath)
{
    if (renderPath_ != renderPath)
    {
        renderPath_ = renderPath;
        renderParamsDirty_ = true;
    }
}

void VR::SetScene(Scene* scene)
{
    if (scene_ != scene)
    {
        scene_ = scene;
        renderParamsDirty_ = true;
    }
}

void VR::SetWorldFromVRTransform(const Matrix3x4& worldFromVR)
{
    worldFromVR_ = worldFromVR;
}

int VR::InitializeVR()
{
    vr::EVRInitError err;
    vrImpl_->vrSystem_ = vr::VR_Init(&err, vr::VRApplication_Scene);

    if (err != vr::VRInitError_None)
    {
        URHO3D_LOGERRORF("vr::VR_Init failed with error %d", err);
    }

    return err;
}

void VR::ShutdownVR()
{
    if (vrImpl_->vrSystem_)
    {
        vr::VR_Shutdown();
        vrImpl_->vrSystem_ = 0;
    }

    delete vrImpl_;
    vrImpl_ = 0;
}

void VR::CreateHMDNodeAndTextures()
{
    unsigned renderWidth, renderHeight;
    vrImpl_->vrSystem_->GetRecommendedRenderTargetSize(&renderWidth, &renderHeight);

    const int textureWidth = int(renderWidth * renderResolutionScale_);
    const int textureHeight = int(renderHeight * renderResolutionScale_);

    URHO3D_LOGINFOF("VR initializing with %dx%d eye textures", textureWidth, textureHeight);
    HMDNode_ = new Node(context_);

    for (int eye = 0; eye < 2; ++eye)
    {
        // Create texture
        cameraTextures_[eye] = new Texture2D(context_);
        cameraTextures_[eye]->SetSize(textureWidth, textureHeight, Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
        cameraTextures_[eye]->SetFilterMode(FILTER_BILINEAR);

        // Set render surface to always update
        RenderSurface* surface = cameraTextures_[eye]->GetRenderSurface();
        surface->SetUpdateMode(SURFACE_UPDATEALWAYS);

        // Create camera node
        Node* cameraNode = HMDNode_->CreateChild(0 == eye ? "LeftEye" : "RightEye");
        {
            // Set head from eye transform
            vr::HmdMatrix34_t vrHeadFromEye = vrImpl_->vrSystem_->GetEyeToHeadTransform(vr::EVREye(eye));

            Matrix3x4 headFromEye = OpenVRAffineTransformToMatrix3x4(vrHeadFromEye);

            cameraNode->SetTransform(headFromEye);

            // Create camera component
            Camera* eyeCamera = cameraNode->CreateComponent<Camera>();
            {
                // Set projection
                vr::HmdMatrix44_t vrProj = vrImpl_->vrSystem_->GetProjectionMatrix(vr::EVREye(eye), nearClip_, farClip_);

                Matrix4 proj = OpenVRProjectionToMatrix4(vrProj);

                eyeCamera->SetProjection(proj);

                // Set scene camera viewport
                SharedPtr<Viewport> eyeViewport(new Viewport(context_, scene_, eyeCamera));

                // Set viewport render path
                if (renderPath_)
                {
                    eyeViewport->SetRenderPath(renderPath_);
                }

                surface->SetViewport(0, eyeViewport);
            }
        }
    }
}

void VR::DestroyHMDNodeAndTextures()
{
    HMDNode_.Reset();

    for (int eye = 0; eye < 2; ++eye)
    {
        cameraTextures_[eye].Reset();
    }
}

void VR::SubscribeToViewEvents()
{
    SubscribeToEvent(E_BEGINVIEWUPDATE, URHO3D_HANDLER(VR, HandleBeginViewUpdate));
    SubscribeToEvent(E_ENDVIEWRENDER, URHO3D_HANDLER(VR, HandleEndViewRender));
}

void VR::UnsubscribeFromViewEvents()
{
    UnsubscribeFromEvent(E_ENDVIEWUPDATE);
    UnsubscribeFromEvent(E_BEGINVIEWUPDATE);
}

void VR::UpdatePosesThisFrame()
{
    if (posesUpdatedThisFrame_)
        return;

    vr::TrackedDevicePose_t trackedDevicePose[vr::k_unMaxTrackedDeviceCount];
    vr::VRCompositor()->WaitGetPoses(trackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
    {
        if (trackedDevicePose[nDevice].bDeviceIsConnected && trackedDevicePose[nDevice].bPoseIsValid)
        {
            vr::ETrackedDeviceClass deviceClass = vrImpl_->vrSystem_->GetTrackedDeviceClass(nDevice);

            switch (deviceClass)
            {
            case vr::TrackedDeviceClass_HMD:
                {
                    Matrix3x4 VRFromHmd = OpenVRAffineTransformToMatrix3x4(trackedDevicePose[nDevice].mDeviceToAbsoluteTracking);

                    worldFromHMD_ = worldFromVR_ * VRFromHmd;
                    if (HMDNode_)
                    {
                        HMDNode_->SetTransform(worldFromHMD_);
                    }

                    HMDFrame_ = currentFrame_;
                }
                break;

            case vr::TrackedDeviceClass_Controller:
                {
                    Matrix3x4 VRFromController = OpenVRAffineTransformToMatrix3x4(trackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
                
                    switch (vrImpl_->vrSystem_->GetControllerRoleForTrackedDeviceIndex(nDevice))
                    {
                    case vr::TrackedControllerRole_LeftHand:
                        worldFromController_[0] = worldFromVR_ * VRFromController;
                        controllerFrame_[0] = currentFrame_;
                        break;

                    case vr::TrackedControllerRole_RightHand:
                        worldFromController_[1] = worldFromVR_ * VRFromController;
                        controllerFrame_[1] = currentFrame_;
                        break;
                    }
                }
                break;
            }
        }
    }

    posesUpdatedThisFrame_ = true;
}

void VR::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    using namespace BeginFrame;

    currentFrame_ = eventData[P_FRAMENUMBER].GetUInt();
    posesUpdatedThisFrame_ = false;

    const bool HMDCreatedAndSceneDestroyed = (HMDNode_ && !scene_);
    if (renderParamsDirty_ || HMDCreatedAndSceneDestroyed)
    {
        if (HMDNode_)
        {
            UnsubscribeFromViewEvents();

            DestroyHMDNodeAndTextures();
        }

        if (vrImpl_->vrSystem_ && scene_)
        {
            CreateHMDNodeAndTextures();

            SubscribeToViewEvents();
        }

        renderParamsDirty_ = false;
    }
}

void VR::HandleBeginViewUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace BeginViewUpdate;

    Texture2D* texture = static_cast<Texture2D*>(eventData[P_TEXTURE].GetPtr());

    // Might get BeginViewUpdate for some other view, only update poses if texture is one of two we want to update this frame
    if (texture == cameraTextures_[0] || texture == cameraTextures_[1])
    {
        UpdatePosesThisFrame();
    }
}

void VR::HandleEndViewRender(StringHash eventType, VariantMap& eventData)
{
    using namespace EndViewRender;

    Texture2D* texture = static_cast<Texture2D*>(eventData[P_TEXTURE].GetPtr());

    int textureIndex = -1;
    for (int eye = 0; eye < 2; ++eye)
    {
        if (cameraTextures_[eye] == texture)
        {
            textureIndex = eye;
            break;
        }
    }

    if (textureIndex >= 0)
    {
        vr::Texture_t vrTex;
        {
#if defined(URHO3D_D3D11)
            vrTex.handle = texture->GetGPUObject();
            vrTex.eType = vr::TextureType_DirectX;
#elif defined(URHO3D_OPENGL)
            // todo.cb OpenGL textures are upside down
            intptr_t textureName = texture->GetGPUObjectName();
            vrTex.handle = reinterpret_cast<void*&>(textureName);
            vrTex.eType = vr::TextureType_OpenGL;
#else
#error Unsupported VR graphics API!
#endif
            vrTex.eColorSpace = vr::ColorSpace_Auto;
        }

        vr::EVRCompositorError err = vr::VRCompositor()->Submit(vr::EVREye(textureIndex), &vrTex);

        if (err != vr::VRCompositorError_None)
        {
            URHO3D_LOGERRORF("vr::VRCompositor()->Submit failed with code: %d", err);
        }
    }
}

}
