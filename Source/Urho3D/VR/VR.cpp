#include "../Precompiled.h"

#include "VR.h"

#include "../Core/CoreEvents.h"
#include "../Graphics/Texture2D.h"
#include "../Math/Frustum.h"
#include "../IO/Log.h"

// hack.cb fix OpenVR C++ static lib exports
#define VR_API_EXPORT 1
#include <openvr/openvr.h>

#include "../DebugNew.h"

namespace
{
    const float DEFAULT_EYE_NEAR = 0.1f;
    const float DEFAULT_EYE_FAR = 100.0f;

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

struct VRData
{
    vr::TrackedDevicePose_t trackedDevicePose_[vr::k_unMaxTrackedDeviceCount];

    Urho3D::Matrix3x4 devicePose_[vr::k_unMaxTrackedDeviceCount];
};

VR::VR(Context* context_) :
    Object(context_),
    vrData_(new VRData()),
    vrSystem_(0),
    recommendedRenderTargetWidth_(0),
    recommendedRenderTargetHeight_(0)
{
    Initialize();
}

VR::~VR()
{
    if (vrSystem_)
    {
        vr::VR_Shutdown();
        vrSystem_ = 0;
    }

    delete vrData_;
    vrData_ = 0;
}

void VR::Initialize()
{
    vr::EVRInitError err;
    vrSystem_ = vr::VR_Init(&err, vr::VRApplication_Scene);

    if (err != vr::VRInitError_None)
    {
        URHO3D_LOGERRORF("vr::VR_Init failed with code: %d", err);
        return;
    }

    // Cache recommended render target size
    vrSystem_->GetRecommendedRenderTargetSize(&recommendedRenderTargetWidth_, &recommendedRenderTargetHeight_);

    // Set up projection
    SetEyeNearAndFar(DEFAULT_EYE_NEAR, DEFAULT_EYE_FAR);

    // Set up eye to head transform
    for (int eye = 0; eye < NUM_EYES; ++eye)
    {
        vr::HmdMatrix34_t eyeToHead = vrSystem_->GetEyeToHeadTransform(static_cast<vr::EVREye>(eye));
        OpenVRAffineTransformToMatrix3x4(eyeToHead, eyeData_[eye].eyeToHeadTransform_);
    }
}

const Matrix3x4& VR::GetWorldFromHeadTransform() const
{
    return vrData_->devicePose_[vr::k_unTrackedDeviceIndex_Hmd];
}

void VR::SetEyeNearAndFar(float eyeNear, float eyeFar)
{
    if (!vrSystem_)
        return;

    for (int eye = 0; eye < NUM_EYES; ++eye)
    {
        vr::HmdMatrix44_t proj = vrSystem_->GetProjectionMatrix(static_cast<vr::EVREye>(eye), eyeNear, eyeFar);
        OpenVRProjectionToMatrix4(proj, eyeData_[eye].projection_);
    }
}

void VR::UpdatePosesBeforeRendering()
{
    if (!vrSystem_)
        return;

    vr::VRCompositor()->WaitGetPoses(vrData_->trackedDevicePose_, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
    {
        if (vrData_->trackedDevicePose_[nDevice].bPoseIsValid)
        {
            OpenVRAffineTransformToMatrix3x4(
                vrData_->trackedDevicePose_[nDevice].mDeviceToAbsoluteTracking,
                vrData_->devicePose_[nDevice]);
        }
    }
}

void VR::SubmitEyeTexturesAfterRendering(Texture2D* eyeTextures[NUM_EYES])
{
    if (!vrSystem_)
        return;

    for (int eye = 0; eye < NUM_EYES; ++eye)
    {
        vr::Texture_t vrTex;
        {
            vrTex.handle = eyeTextures[eye]->GetGPUObject();
            vrTex.eType = vr::TextureType_DirectX;
            vrTex.eColorSpace = vr::ColorSpace_Auto;
        }

        vr::EVRCompositorError err = vr::VRCompositor()->Submit(static_cast<vr::EVREye>(eye), &vrTex);

        if (err != vr::VRCompositorError_None)
        {
            URHO3D_LOGERRORF("vr::VRCompositor()->Submit failed with code: %d", err);
        }
    }
}

}
