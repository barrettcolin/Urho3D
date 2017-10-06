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

    void OpenVRAffineTransformToMatrix3x4(vr::HmdMatrix34_t const& in, Urho3D::Matrix3x4& out)
    {
        out.m00_ = in.m[0][0]; out.m01_ = in.m[0][1]; out.m02_ = -in.m[0][2]; out.m03_ = in.m[0][3];
        out.m10_ = in.m[1][0]; out.m11_ = in.m[1][1]; out.m12_ = -in.m[1][2]; out.m13_ = in.m[1][3];
        out.m20_ = -in.m[2][0]; out.m21_ = -in.m[2][1]; out.m22_ = in.m[2][2]; out.m23_ = -in.m[2][3];
    }

    void OpenVRProjectionToMatrix4(vr::HmdMatrix44_t const& in, Urho3D::Matrix4& out)
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
        out.m00_ = in.m[0][0]; out.m01_ = in.m[0][1]; out.m02_ = -in.m[0][2]; out.m03_ = in.m[0][3];
        out.m10_ = in.m[1][0]; out.m11_ = in.m[1][1]; out.m12_ = -in.m[1][2]; out.m13_ = in.m[1][3];
        out.m20_ = in.m[2][0]; out.m21_ = in.m[2][1]; out.m22_ = -in.m[2][2]; out.m23_ = in.m[2][3];
        out.m30_ = in.m[3][0]; out.m31_ = in.m[3][1]; out.m32_ = -in.m[3][2]; out.m33_ = in.m[3][3];
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
    posesUpdatedThisFrame_(false)
{
    if (Initialize() == 0)
    {
        SubscribeToEvent(E_BEGINFRAME, URHO3D_HANDLER(VR, HandleBeginFrame));
    }
}

VR::~VR()
{
    Shutdown();

    if (vrImpl_->vrSystem_)
    {
        vr::VR_Shutdown();
        vrImpl_->vrSystem_ = 0;
    }

    delete vrImpl_;
    vrImpl_ = 0;
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

int VR::Initialize()
{
    vr::EVRInitError err;
    vrImpl_->vrSystem_ = vr::VR_Init(&err, vr::VRApplication_Scene);

    if (err != vr::VRInitError_None)
    {
        URHO3D_LOGERRORF("vr::VR_Init failed with error %d", err);
    }

    return err;
}

void VR::Shutdown()
{
    URHO3D_LOGINFO("VR shutting down");
        
    UnsubscribeFromEvent(E_BEGINVIEWUPDATE);
    UnsubscribeFromEvent(E_ENDVIEWUPDATE);

    headNode_.Reset();

    for (int eye = 0; eye < 2; ++eye)
    {
        cameraTextures_[eye].Reset();
    }
}

void VR::UpdatePosesThisFrame()
{
    if (posesUpdatedThisFrame_)
        return;

    vr::TrackedDevicePose_t trackedDevicePose[vr::k_unMaxTrackedDeviceCount];
    vr::VRCompositor()->WaitGetPoses(trackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
    {
        if (trackedDevicePose[nDevice].bPoseIsValid)
        {
            switch (nDevice)
            {
            case vr::k_unTrackedDeviceIndex_Hmd:
                if (headNode_)
                {
                    Matrix3x4 vrFromDevice;
                    OpenVRAffineTransformToMatrix3x4(trackedDevicePose[nDevice].mDeviceToAbsoluteTracking, vrFromDevice);

                    headNode_->SetTransform(worldFromVR_ * vrFromDevice);
                }
                break;
            }
        }
    }

    posesUpdatedThisFrame_ = true;
}

void VR::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    posesUpdatedThisFrame_ = false;

    const bool sceneDestroyed = (headNode_ && !scene_);
    if (renderParamsDirty_ || sceneDestroyed)
    {
        if (headNode_)
        {
            Shutdown();
        }

        if (vrImpl_->vrSystem_ && scene_)
        {
            unsigned renderWidth, renderHeight;
            vrImpl_->vrSystem_->GetRecommendedRenderTargetSize(&renderWidth, &renderHeight);

            const int textureWidth = int(renderWidth * renderResolutionScale_);
            const int textureHeight = int(renderHeight * renderResolutionScale_);

            URHO3D_LOGINFOF("VR initializing with %dx%d eye textures", textureWidth, textureHeight);
            headNode_ = new Node(context_);

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
                Node* cameraNode = headNode_->CreateChild(0 == eye ? "LeftEye" : "RightEye");
                {
                    // Set head from eye transform
                    vr::HmdMatrix34_t vrHeadFromEye = vrImpl_->vrSystem_->GetEyeToHeadTransform(vr::EVREye(eye));

                    Matrix3x4 headFromEye;
                    OpenVRAffineTransformToMatrix3x4(vrHeadFromEye, headFromEye);

                    cameraNode->SetTransform(headFromEye);

                    // Create camera component
                    Camera* eyeCamera = cameraNode->CreateComponent<Camera>();
                    {
                        // Set projection
                        vr::HmdMatrix44_t vrProj = vrImpl_->vrSystem_->GetProjectionMatrix(vr::EVREye(eye), nearClip_, farClip_);

                        Matrix4 proj;
                        OpenVRProjectionToMatrix4(vrProj, proj);

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

            SubscribeToEvent(E_BEGINVIEWUPDATE, URHO3D_HANDLER(VR, HandleBeginViewUpdate));
            SubscribeToEvent(E_ENDVIEWRENDER, URHO3D_HANDLER(VR, HandleEndViewRender));
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
            vrTex.handle = texture->GetGPUObject();
            vrTex.eType = vr::TextureType_DirectX;
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
