#include "../../Precompiled.h"

#include "../VR.h"

#include "../../Core/CoreEvents.h"
#include "../../Graphics/Camera.h"
#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsEvents.h"
#include "../../Graphics/RenderPath.h"
#include "../../Graphics/Texture2D.h"
#include "../../IO/Log.h"
#include "../../Scene/Scene.h"

#include "OpenVRImpl.h"

#include "../../DebugNew.h"

namespace Urho3D
{

VR::VR(Context* context_) :
    Object(context_),
    vrImpl_(new VRImpl())
{
    if (vrImpl_->vrSystem_)
    {
        SubscribeToEvent(E_BEGINRENDERING, URHO3D_HANDLER(VR, HandleBeginRendering));
    }
}

VR::~VR()
{
    delete vrImpl_;
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
    projOut = VRImpl::UrhoProjectionFromOpenVR(vrImpl_->vrSystem_->GetProjectionMatrix(vr::EVREye(eye), nearClip, farClip));
}

void VR::GetHeadFromEyeTransform(VREye eye, Matrix3x4& headFromEyeOut) const
{
    assert(eye >= VREYE_LEFT && eye < NUM_EYES);
    assert(vrImpl_->vrSystem_);
    headFromEyeOut = VRImpl::UrhoAffineTransformFromOpenVR(vrImpl_->vrSystem_->GetEyeToHeadTransform(vr::EVREye(eye)));
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
            trackingFromDevice_[deviceType] = VRImpl::UrhoAffineTransformFromOpenVR(trackedDevicePose.mDeviceToAbsoluteTracking);

            using namespace VRDevicePoseUpdatedForRendering;

            VariantMap& eventData = GetEventDataMap();

            eventData[P_DEVICETYPE] = deviceType;

            SendEvent(E_VRDEVICEPOSEUPDATEDFORRENDERING, eventData);
        }
    }
}

}
