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
    int const err = vrImpl_->Initialize();

    if (err == 0)
    {
        for (unsigned i = 0; i < vrImpl_->deviceTrackingData_.Size(); ++i)
        {
            VRImpl::DeviceTrackingData const& dev = vrImpl_->deviceTrackingData_[i];
            URHO3D_LOGINFOF("OpenVR init device %d with class %d and controller role %d", i, dev.deviceClass_, dev.controllerRole_);
        }

        SubscribeToEvent(E_BEGINFRAME, URHO3D_HANDLER(VR, HandleBeginFrame));
        SubscribeToEvent(E_BEGINRENDERING, URHO3D_HANDLER(VR, HandleBeginRendering));
    }
    else
    {
        URHO3D_LOGERRORF("OpenVR VRImpl::Initialize error %d", err);
    }
}

VR::~VR()
{
    delete vrImpl_;
}

bool VR::IsInitialized() const
{
    return vrImpl_->vrSystem_ != 0;
}

unsigned VR::GetNumDevices() const
{
    return vrImpl_->deviceTrackingData_.Size();
}

unsigned VR::GetDeviceId(unsigned deviceIndex) const
{
    return vrImpl_->deviceTrackingData_[deviceIndex].deviceIndex_;
}

VRDeviceClass VR::GetDeviceClass(unsigned deviceIndex) const
{
    return VRDeviceClass(vrImpl_->deviceTrackingData_[deviceIndex].deviceClass_);
}

VRControllerRole VR::GetControllerRole(unsigned deviceIndex) const
{
    return VRControllerRole(vrImpl_->deviceTrackingData_[deviceIndex].controllerRole_);
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
    VRImpl::UrhoProjectionFromOpenVR(vrImpl_->vrSystem_->GetProjectionMatrix(vr::EVREye(eye), nearClip, farClip), projOut);
}

void VR::GetHeadFromEyeTransform(VREye eye, Matrix3x4& headFromEyeOut) const
{
    assert(eye >= VREYE_LEFT && eye < NUM_EYES);
    assert(vrImpl_->vrSystem_);
    VRImpl::UrhoAffineTransformFromOpenVR(vrImpl_->vrSystem_->GetEyeToHeadTransform(vr::EVREye(eye)), headFromEyeOut);
}

const Matrix3x4& VR::GetTrackingFromDeviceTransform(unsigned deviceIndex) const
{
    return vrImpl_->deviceTrackingData_[deviceIndex].trackingFromDevice_;
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
#error OpenGL support incomplete!
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

void VR::HandleBeginFrame(StringHash eventType, VariantMap& /*eventData*/)
{
    // Process VR events
    {
        vr::VREvent_t vrEvent;
        while (vrImpl_->vrSystem_->PollNextEvent(&vrEvent, sizeof(vrEvent)))
        {
            switch (vrEvent.eventType)
            {
            case vr::VREvent_TrackedDeviceActivated:
                URHO3D_LOGINFOF("VREvent_TrackedDeviceActivated with device %d", vrEvent.trackedDeviceIndex);
                {
                    int const implIndex = vrImpl_->ActivateDevice(vrEvent.trackedDeviceIndex);

                    vr::TrackedDeviceIndex_t const deviceId = vrImpl_->deviceTrackingData_[implIndex].deviceIndex_;
                    VRDeviceClass const deviceClass = VRDeviceClass(vrImpl_->deviceTrackingData_[implIndex].deviceClass_);
                    VRControllerRole const controllerRole = VRControllerRole(vrImpl_->deviceTrackingData_[implIndex].controllerRole_);

                    SendDeviceConnectedEvent(E_VRDEVICECONNECTED, deviceId, deviceClass, controllerRole);
                }
                break;

            case vr::VREvent_TrackedDeviceDeactivated:
                URHO3D_LOGINFOF("VREvent_TrackedDeviceDeactivated with device %d", vrEvent.trackedDeviceIndex);
                {
                    int const implIndex = vrImpl_->implIndexFromTrackedDeviceIndex_[vrEvent.trackedDeviceIndex];

                    vr::TrackedDeviceIndex_t const deviceId = vrImpl_->deviceTrackingData_[implIndex].deviceIndex_;
                    VRDeviceClass const deviceClass = VRDeviceClass(vrImpl_->deviceTrackingData_[implIndex].deviceClass_);
                    VRControllerRole const controllerRole = VRControllerRole(vrImpl_->deviceTrackingData_[implIndex].controllerRole_);

                    SendDeviceConnectedEvent(E_VRDEVICEDISCONNECTED, deviceId, deviceClass, controllerRole);

                    vrImpl_->DeactivateDevice(vrEvent.trackedDeviceIndex);
                }
                break;

            case vr::VREvent_TrackedDeviceUpdated:
                URHO3D_LOGINFOF("VREvent_TrackedDeviceUpdated with device %d", vrEvent.trackedDeviceIndex);
                break;
            }
        }
    }

    // Update controller roles
    for (unsigned i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
        if (!vrImpl_->implIndexFromTrackedDeviceIndex_.Contains(i))
            continue;

        int const implIndex = vrImpl_->implIndexFromTrackedDeviceIndex_[i];
        const vr::ETrackedControllerRole oldControllerRole = vrImpl_->deviceTrackingData_[implIndex].controllerRole_;
        const vr::ETrackedControllerRole newControllerRole = vrImpl_->vrSystem_->GetControllerRoleForTrackedDeviceIndex(i);
        if (newControllerRole != oldControllerRole)
        {
            URHO3D_LOGINFOF("Deactivating controller %d with role %d", i, oldControllerRole);
            {
                vr::TrackedDeviceIndex_t const deviceId = vrImpl_->deviceTrackingData_[implIndex].deviceIndex_;
                VRDeviceClass const deviceClass = VRDeviceClass(vrImpl_->deviceTrackingData_[implIndex].deviceClass_);
                VRControllerRole const controllerRole = VRControllerRole(vrImpl_->deviceTrackingData_[implIndex].controllerRole_);

                SendDeviceConnectedEvent(E_VRDEVICEDISCONNECTED, deviceId, deviceClass, controllerRole);

                vrImpl_->DeactivateDevice(i);
            }

            URHO3D_LOGINFOF("Activating controller %d with role %d", i, newControllerRole);
            {
                int const implIndex = vrImpl_->ActivateDevice(i);

                vr::TrackedDeviceIndex_t const deviceId = vrImpl_->deviceTrackingData_[implIndex].deviceIndex_;
                VRDeviceClass const deviceClass = VRDeviceClass(vrImpl_->deviceTrackingData_[implIndex].deviceClass_);
                VRControllerRole const controllerRole = VRControllerRole(vrImpl_->deviceTrackingData_[implIndex].controllerRole_);

                SendDeviceConnectedEvent(E_VRDEVICECONNECTED, deviceId, deviceClass, controllerRole);
            }
        }
    }
}

void VR::HandleBeginRendering(StringHash eventType, VariantMap& eventData)
{
    vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    vr::VRCompositor()->WaitGetPoses(trackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);

    for (unsigned nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
    {
        const vr::TrackedDevicePose_t& trackedDevicePose = trackedDevicePoses[nDevice];

        int const implIndex = vrImpl_->implIndexFromTrackedDeviceIndex_[nDevice];
        VRImpl::DeviceTrackingData& deviceData = vrImpl_->deviceTrackingData_[implIndex];
        deviceData.poseValid_ = trackedDevicePose.bPoseIsValid;
        if (deviceData.poseValid_)
        {
            VRImpl::UrhoAffineTransformFromOpenVR(trackedDevicePose.mDeviceToAbsoluteTracking, deviceData.trackingFromDevice_);
        }
    }

    using namespace VRDevicePosesUpdatedForRendering;

    SendEvent(E_VRDEVICEPOSESUPDATEDFORRENDERING, GetEventDataMap());
}

}
