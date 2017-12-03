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
        unsigned const numDevices = vrImpl_->GetNumDevices();
        for (unsigned i = 0; i < numDevices; ++i)
        {
            VRImpl::DeviceData const& dev = vrImpl_->GetDevice(i);
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
    return vrImpl_->IsInitialized();
}

unsigned VR::GetNumDevices() const
{
    return vrImpl_->GetNumDevices();
}

unsigned VR::GetDeviceId(unsigned deviceIndex) const
{
    return vrImpl_->GetDevice(deviceIndex).openVRDeviceIndex_;
}

VRDeviceClass VR::GetDeviceClass(unsigned deviceIndex) const
{
    return VRDeviceClass(vrImpl_->GetDevice(deviceIndex).deviceClass_);
}

VRControllerRole VR::GetControllerRole(unsigned deviceIndex) const
{
    return VRControllerRole(vrImpl_->GetDevice(deviceIndex).controllerRole_);
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
    return vrImpl_->GetDevice(deviceIndex).trackingFromDevice_;
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
                {
                    VRImpl::DeviceData const& deviceData = vrImpl_->GetDevice(vrImpl_->ActivateDevice(vrEvent.trackedDeviceIndex));

                    SendDeviceConnectedEvent(deviceData.openVRDeviceIndex_, VRDeviceClass(deviceData.deviceClass_), VRControllerRole(deviceData.controllerRole_));
                }
                break;

            case vr::VREvent_TrackedDeviceDeactivated:
                {
                    unsigned deviceIndex;
                    /*bool const hasDevice = */vrImpl_->GetIndexForOpenVRDevice(vrEvent.trackedDeviceIndex, deviceIndex);

                    VRImpl::DeviceData const& deviceData = vrImpl_->GetDevice(deviceIndex);

                    SendDeviceDisconnectedEvent(deviceData.openVRDeviceIndex_, VRDeviceClass(deviceData.deviceClass_), VRControllerRole(deviceData.controllerRole_));

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
    for (unsigned implIndex = 0; implIndex < vrImpl_->GetNumDevices(); ++implIndex)
    {
        VRImpl::DeviceData const& deviceData = vrImpl_->GetDevice(implIndex);
        VRDeviceClass const deviceClass = VRDeviceClass(deviceData.deviceClass_);

        if (deviceClass != VRDEVICE_CONTROLLER)
            continue;

        VRControllerRole const oldControllerRole = VRControllerRole(deviceData.controllerRole_);

        vr::TrackedDeviceIndex_t const deviceId = deviceData.openVRDeviceIndex_;
        VRControllerRole const newControllerRole = VRControllerRole(vrImpl_->vrSystem_->GetControllerRoleForTrackedDeviceIndex(deviceId));

        if (newControllerRole != oldControllerRole)
        {
            SendDeviceDisconnectedEvent(deviceId, deviceClass, oldControllerRole);

            vrImpl_->DeactivateDevice(deviceId);

            /*int const implIndex = */vrImpl_->ActivateDevice(deviceId);

            SendDeviceConnectedEvent(deviceId, deviceClass, newControllerRole);
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

        unsigned implIndex;
        if (vrImpl_->GetIndexForOpenVRDevice(nDevice, implIndex))
        {
            VRImpl::DeviceData& deviceData = vrImpl_->AccessDevice(implIndex);

            deviceData.poseValid_ = trackedDevicePose.bPoseIsValid;
            if (deviceData.poseValid_)
            {
                VRImpl::UrhoAffineTransformFromOpenVR(trackedDevicePose.mDeviceToAbsoluteTracking, deviceData.trackingFromDevice_);
            }
        }
    }

    using namespace VRDevicePosesUpdatedForRendering;

    SendEvent(E_VRDEVICEPOSESUPDATEDFORRENDERING, GetEventDataMap());
}

}
