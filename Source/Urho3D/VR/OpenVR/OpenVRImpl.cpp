#include "../../Precompiled.h"

#include "OpenVRImpl.h"

#include "../../IO/Log.h"

#include "../../DebugNew.h"

namespace Urho3D
{

VRImpl::DeviceData::DeviceData(vr::TrackedDeviceIndex_t deviceIndex,
    vr::ETrackedDeviceClass deviceClass, 
    vr::ETrackedControllerRole controllerRole)
    : openVRDeviceIndex_(deviceIndex),
    deviceClass_(deviceClass),
    controllerRole_(controllerRole),
    poseValid_(false)
{
    memset(&controllerState_, 0, sizeof controllerState_);
}

VRImpl::VRImpl()
    : vrSystem_(0)
{
    for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
        trackedDeviceConnected_[i] = false;
}

VRImpl::~VRImpl()
{
    if (vrSystem_)
    {
        vr::VR_Shutdown();
        vrSystem_ = 0;
    }
}

int VRImpl::Initialize()
{
    vr::EVRInitError err;
    vrSystem_ = vr::VR_Init(&err, vr::VRApplication_Scene);

    if (err == vr::VRInitError_None)
    {
        // VR system created: init devices
        for (vr::TrackedDeviceIndex_t dev = 0; dev < vr::k_unMaxTrackedDeviceCount; ++dev)
        {
            vr::ETrackedDeviceClass const deviceClass = vrSystem_->GetTrackedDeviceClass(dev);
            bool const isHMDOrController = (deviceClass == vr::TrackedDeviceClass_HMD || deviceClass == vr::TrackedDeviceClass_Controller);

            if (isHMDOrController && vrSystem_->IsTrackedDeviceConnected(dev))
            {
                trackedDeviceConnected_[dev] = true;

                int const implIndex = deviceData_.Size();
                vr::ETrackedControllerRole const controllerRole = vrSystem_->GetControllerRoleForTrackedDeviceIndex(dev);

                deviceData_.Push(DeviceData(dev, deviceClass, controllerRole));

                implIndexFromTrackedDeviceIndex_[dev] = implIndex;
            }
        }
    }

    return err;
}

unsigned VRImpl::ActivateDevice(vr::TrackedDeviceIndex_t deviceIndex)
{
    trackedDeviceConnected_[deviceIndex] = true;

    unsigned const implIndex = deviceData_.Size();

    vr::ETrackedControllerRole const controllerRole = vrSystem_->GetControllerRoleForTrackedDeviceIndex(deviceIndex);
    deviceData_.Push(DeviceData(deviceIndex, vrSystem_->GetTrackedDeviceClass(deviceIndex), controllerRole));

    implIndexFromTrackedDeviceIndex_[deviceIndex] = implIndex;

    return implIndex;
}

void VRImpl::DeactivateDevice(vr::TrackedDeviceIndex_t deviceIndex)
{
    unsigned const implIndex = implIndexFromTrackedDeviceIndex_[deviceIndex];

    deviceData_.EraseSwap(implIndex);

    if (implIndex < deviceData_.Size())
    {
        // Swapped a DeviceData so update so update the map to the new index
        implIndexFromTrackedDeviceIndex_[deviceData_[implIndex].openVRDeviceIndex_] = implIndex;
    }

    trackedDeviceConnected_[deviceIndex] = false;
}

// Urho scene transforms are X-right/Y-up/Z-forward, OpenVR poses are X-right/Y-up/Z-backward
//
// ovrFromUrho is:  1  0  0  0
//                  0  1  0  0
//                  0  0 -1  0
//                  0  0  0  1
//
// scene transforms have Urho input, Urho output, so OVR poses require change of basis:
// urhoOut = (urhoFromOVR * OVRpose * OVRFromUrho) * urhoIn
// See also: David Eberly "Conversion of Left-Handed Coordinates to Right - Handed Coordinates"
//
// projections have Urho input, clip output (same clip as OVR), so OVR projections require reflection:
// clipOut = (OVRprojection * ovrFromUrho) * urhoIn
void VRImpl::UrhoAffineTransformFromOpenVR(vr::HmdMatrix34_t const& in, Matrix3x4& out)
{
    out = Urho3D::Matrix3x4(
        in.m[0][0], in.m[0][1], -in.m[0][2], in.m[0][3],
        in.m[1][0], in.m[1][1], -in.m[1][2], in.m[1][3],
        -in.m[2][0], -in.m[2][1], in.m[2][2], -in.m[2][3]
    );
}

void VRImpl::UrhoProjectionFromOpenVR(vr::HmdMatrix44_t const& in, Matrix4& out)
{
    out = Urho3D::Matrix4(
        in.m[0][0], in.m[0][1], -in.m[0][2], in.m[0][3],
        in.m[1][0], in.m[1][1], -in.m[1][2], in.m[1][3],
        in.m[2][0], in.m[2][1], -in.m[2][2], in.m[2][3],
        in.m[3][0], in.m[3][1], -in.m[3][2], in.m[3][3]
    );
}

}
