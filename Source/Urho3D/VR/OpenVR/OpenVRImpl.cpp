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

void VRImpl::DeviceData::SetClassAndControllerRole(vr::ETrackedDeviceClass deviceClass, vr::ETrackedControllerRole controllerRole)
{
    deviceClass_ = deviceClass;
    controllerRole_ = controllerRole;
}

VRImpl::VRImpl()
    : vrSystem_(0),
    numConnectedDevices_(0)
{
    for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
    {
        deviceData_[i].openVRDeviceIndex_ = vr::k_unTrackedDeviceIndexInvalid;
        trackedDeviceConnected_[i] = false;
        connectedDeviceIndexFromTrackedDeviceIndex_[i] = vr::k_unTrackedDeviceIndexInvalid;
    }
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

                unsigned const connectedDeviceIndex = numConnectedDevices_++;
                
                deviceData_[connectedDeviceIndex].openVRDeviceIndex_ = dev;
                deviceData_[connectedDeviceIndex].SetClassAndControllerRole(deviceClass, vrSystem_->GetControllerRoleForTrackedDeviceIndex(dev));

                connectedDeviceIndexFromTrackedDeviceIndex_[dev] = connectedDeviceIndex;
            }
        }
    }

    return err;
}

unsigned VRImpl::ActivateDevice(vr::TrackedDeviceIndex_t deviceIndex)
{
    trackedDeviceConnected_[deviceIndex] = true;

    unsigned const connectedDeviceIndex = numConnectedDevices_++;

    vr::ETrackedDeviceClass const deviceClass = vrSystem_->GetTrackedDeviceClass(deviceIndex);

    bool const isController = deviceClass == vr::TrackedDeviceClass_Controller;
    vr::ETrackedControllerRole const controllerRole = isController ? vrSystem_->GetControllerRoleForTrackedDeviceIndex(deviceIndex) : vr::TrackedControllerRole_Invalid;

    deviceData_[connectedDeviceIndex].openVRDeviceIndex_ = deviceIndex;
    deviceData_[connectedDeviceIndex].SetClassAndControllerRole(deviceClass, controllerRole);

    connectedDeviceIndexFromTrackedDeviceIndex_[deviceIndex] = connectedDeviceIndex;

    return connectedDeviceIndex;
}

void VRImpl::DeactivateDevice(vr::TrackedDeviceIndex_t deviceIndex)
{
    unsigned const connectedDeviceIndex = connectedDeviceIndexFromTrackedDeviceIndex_[deviceIndex];

    if (numConnectedDevices_ > 1 && connectedDeviceIndex < numConnectedDevices_)
    {
        DeviceData const& lastData = deviceData_[numConnectedDevices_ - 1];

        deviceData_[connectedDeviceIndex] = lastData;

        connectedDeviceIndexFromTrackedDeviceIndex_[lastData.openVRDeviceIndex_] = connectedDeviceIndex;
    }

    numConnectedDevices_--;

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
