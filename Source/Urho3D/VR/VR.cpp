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

// Match OpenVR C++ static lib exports
#define VR_API_EXPORT 1
#include <openvr/openvr.h>

#include "../DebugNew.h"

namespace
{
    Urho3D::Matrix3x4 OpenVRAffineTransformToMatrix3x4(vr::HmdMatrix34_t const& in)
    {
        // See David Eberly "Conversion of Left-Handed Coordinates to Right - Handed Coordinates"
        return Urho3D::Matrix3x4(
             in.m[0][0],  in.m[0][1], -in.m[0][2],  in.m[0][3],
             in.m[1][0],  in.m[1][1], -in.m[1][2],  in.m[1][3],
            -in.m[2][0], -in.m[2][1],  in.m[2][2], -in.m[2][3]
        );
    }

    Urho3D::Matrix4 OpenVRProjectionToMatrix4(vr::HmdMatrix44_t const& in)
    {
        /*
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
        */
        // Negate Z-column for RH to LH projection
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
    /// DeviceTrackingData
    struct DeviceTrackingData
    {
        unsigned index_;
        vr::ETrackedDeviceClass class_;
        vr::ETrackedControllerRole role_;
        vr::ETrackingResult result_;
    };

    /// DeviceTrackingDataFromIndex
    typedef HashMap<unsigned, DeviceTrackingData> DeviceTrackingDataFromIndex;

public:
    /// Construct.
    VRImpl();

    DeviceTrackingDataFromIndex& AccessDeviceTrackingDataFromIndex() { return trackingResultFromDeviceIndex_; }

private:
    /// OpenVR VRSystem interface
    vr::IVRSystem* vrSystem_;
    /// ETrackingResult from device index
    DeviceTrackingDataFromIndex trackingResultFromDeviceIndex_;
};

VRImpl::VRImpl() :
    vrSystem_(0)
{

}

VR::VR(Context* context_) :
    Object(context_),
    vrImpl_(new VRImpl())
{
    if (InitializeVR() == 0)
    {
        SubscribeToEvent(E_BEGINRENDERING, URHO3D_HANDLER(VR, HandleBeginRendering));
    }
}

VR::~VR()
{
    ShutdownVR();
}

void VR::GetRecommendedRenderTargetSize(unsigned& widthOut, unsigned& heightOut) const
{
    assert(vrImpl_->vrSystem_);
    vrImpl_->vrSystem_->GetRecommendedRenderTargetSize(&widthOut, &heightOut);
}

void VR::GetEyeProjection(VREye eye, float nearClip, float farClip, Matrix4& projOut) const
{
    assert(eye >= VREYE_LEFT && eye < NUM_EYES);
    assert(vrImpl_->vrSystem_);
    projOut = OpenVRProjectionToMatrix4(vrImpl_->vrSystem_->GetProjectionMatrix(vr::EVREye(eye), nearClip, farClip));
}

void VR::GetHeadFromEyeTransform(VREye eye, Matrix3x4& headFromEyeOut) const
{
    assert(eye >= VREYE_LEFT && eye < NUM_EYES);
    assert(vrImpl_->vrSystem_);
    headFromEyeOut = OpenVRAffineTransformToMatrix3x4(vrImpl_->vrSystem_->GetEyeToHeadTransform(vr::EVREye(eye)));
}

const Matrix3x4& VR::GetTrackingFromDeviceTransform(VRDeviceType vrDevice) const
{
    assert(vrDevice > VRDEVICE_INVALID && vrDevice < NUM_VR_DEVICE_TYPES);
    return trackingFromDevice_[vrDevice - 1];
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

void VR::SendDeviceEvent(StringHash eventType, VRDeviceType deviceType, VRTrackingResult trackingResult)
{
    using namespace VRDeviceConnected;

    VariantMap& eventData = GetEventDataMap();

    eventData[P_DEVICETYPE] = deviceType;
    eventData[P_TRACKINGRESULT] = trackingResult;

    SendEvent(eventType, eventData);
}

void VR::SubmitEyeTexture(VREye eye, Texture2D* texture)
{
    assert(eye == VREYE_LEFT || eye == VREYE_RIGHT);

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

    vr::EVRCompositorError err = vr::VRCompositor()->Submit(vr::EVREye(eye), &vrTex);

    if (err != vr::VRCompositorError_None)
    {
        URHO3D_LOGERRORF("vr::VRCompositor()->Submit failed with code: %d", err);
    }
}

void VR::HandleBeginRendering(StringHash eventType, VariantMap& eventData)
{
    vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    vr::VRCompositor()->WaitGetPoses(trackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    for (unsigned nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
    {
        const vr::TrackedDevicePose_t& trackedDevicePose = trackedDevicePoses[nDevice];
        VRDeviceType deviceType = VRDEVICE_INVALID;

        const vr::ETrackedDeviceClass deviceClass = vrImpl_->vrSystem_->GetTrackedDeviceClass(nDevice);
        vr::ETrackedControllerRole controllerRole = vr::TrackedControllerRole_Invalid;
        {
            switch (deviceClass)
            {
            case vr::TrackedDeviceClass_HMD:
                deviceType = VRDEVICE_HMD;
                break;

            case vr::TrackedDeviceClass_Controller:
                controllerRole = vrImpl_->vrSystem_->GetControllerRoleForTrackedDeviceIndex(nDevice);
                switch (controllerRole)
                {
                case vr::TrackedControllerRole_LeftHand:
                    deviceType = VRDEVICE_CONTROLLER_LEFT;
                    break;

                case vr::TrackedControllerRole_RightHand:
                    deviceType = VRDEVICE_CONTROLLER_RIGHT;
                    break;
                }
                break;
            }
        }

        VRImpl::DeviceTrackingDataFromIndex& deviceTrackingDataFromIndex = vrImpl_->AccessDeviceTrackingDataFromIndex();

        VRImpl::DeviceTrackingData trackingData;
        if (deviceTrackingDataFromIndex.TryGetValue(nDevice, trackingData))
        {
            // Device is known
            assert(trackingData.index_ == nDevice && trackingData.class_ == deviceClass/* && trackingData.role_ == controllerRole*/);

            if (!trackedDevicePose.bDeviceIsConnected)
            {
                // OpenVR (sometimes?) reports a different (invalid) controller role on disconnection; log this and use the stored value
                if (trackingData.role_ != controllerRole)
                {
                    assert(trackingData.role_ != vr::TrackedControllerRole_Invalid);

                    URHO3D_LOGINFOF("Disconnecting VR device was previously seen with role %d, now %d; forcing old value", trackingData.role_, controllerRole);
                    deviceType = (trackingData.role_ == vr::TrackedControllerRole_LeftHand) ? VRDEVICE_CONTROLLER_LEFT : VRDEVICE_CONTROLLER_RIGHT;
                }

                URHO3D_LOGINFOF("VR device %d of type %d disconnected", nDevice, deviceType);

                deviceTrackingDataFromIndex.Erase(nDevice);

                // Device disconnected
                using namespace VRDeviceDisconnected;

                SendDeviceEvent(E_VRDEVICEDISCONNECTED, deviceType, VRTRACKING_INVALID);
            }
            else
            {
                // Device connected
                if (trackedDevicePose.eTrackingResult != trackingData.result_)
                {
                    // Tracking changed
                    URHO3D_LOGINFOF("VR device %d tracking changed from %d to %d", nDevice, trackingData.result_, trackedDevicePose.eTrackingResult);

                    deviceTrackingDataFromIndex[nDevice].result_ = trackedDevicePose.eTrackingResult;

                    using namespace VRDeviceTrackingChanged;

                    SendDeviceEvent(E_VRDEVICETRACKINGCHANGED, deviceType, VRTrackingResult(trackedDevicePose.eTrackingResult));
                }
            }
        }
        else
        {
            // Device is not known
            if (trackedDevicePose.bDeviceIsConnected && deviceType != VRDEVICE_INVALID)
            {
                URHO3D_LOGINFOF("VR device %d of type %d connected", nDevice, deviceType);

                // Valid device newly connected
                trackingData.index_ = nDevice;
                trackingData.class_ = deviceClass;
                trackingData.role_ = controllerRole;
                trackingData.result_ = vr::TrackingResult_Uninitialized;

                deviceTrackingDataFromIndex[nDevice] = trackingData;

                using namespace VRDeviceConnected;

                SendDeviceEvent(E_VRDEVICECONNECTED, deviceType, VRTRACKING_INVALID);
            }
        }

        if (trackedDevicePose.bDeviceIsConnected && trackedDevicePose.bPoseIsValid && deviceType != VRDEVICE_INVALID)
        {
            trackingFromDevice_[deviceType - 1] = OpenVRAffineTransformToMatrix3x4(trackedDevicePose.mDeviceToAbsoluteTracking);

            using namespace VRDevicePoseUpdatedForRendering;

            VariantMap& eventData = GetEventDataMap();

            eventData[P_DEVICETYPE] = deviceType;

            SendEvent(E_VRDEVICEPOSEUPDATEDFORRENDERING, eventData);
        }
    }
}

}
