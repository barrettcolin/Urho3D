#include "../../Precompiled.h"

#include "OpenVRImpl.h"

#include "../../IO/Log.h"

#include "../../DebugNew.h"

namespace Urho3D
{

VRImpl::DeviceTrackingData::DeviceTrackingData(vr::TrackedDeviceIndex_t deviceIndex, 
    vr::ETrackedDeviceClass deviceClass, 
    vr::ETrackedControllerRole controllerRole)
    : deviceIndex_(deviceIndex),
    deviceClass_(deviceClass),
    controllerRole_(controllerRole),
    poseValid_(false)
{
    memset(&controllerState_, 0, sizeof controllerState_);
}

VRImpl::VRImpl()
    : vrSystem_(0)
{
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
                int const implIndex = deviceTrackingData_.Size();
                vr::ETrackedControllerRole const controllerRole = vrSystem_->GetControllerRoleForTrackedDeviceIndex(dev);

                deviceTrackingData_.Push(DeviceTrackingData(dev, deviceClass, controllerRole));

                implIndexFromTrackedDeviceIndex_[dev] = implIndex;
            }
        }
    }

    return err;
}

int VRImpl::ActivateDevice(vr::TrackedDeviceIndex_t deviceIndex)
{
    int const implIndex = deviceTrackingData_.Size();
    URHO3D_LOGINFOF("ActivateDevice deviceIndex %d with implIndex %d", deviceIndex, implIndex);

    vr::ETrackedControllerRole const controllerRole = vrSystem_->GetControllerRoleForTrackedDeviceIndex(deviceIndex);
    deviceTrackingData_.Push(DeviceTrackingData(deviceIndex, vrSystem_->GetTrackedDeviceClass(deviceIndex), controllerRole));

    implIndexFromTrackedDeviceIndex_[deviceIndex] = implIndex;

    return implIndex;
}

void VRImpl::DeactivateDevice(vr::TrackedDeviceIndex_t deviceIndex)
{
    int const implIndex = implIndexFromTrackedDeviceIndex_[deviceIndex];
    URHO3D_LOGINFOF("DeactivateDevice deviceIndex %d with implIndex %d", deviceIndex, implIndex);

    bool const isLast = (implIndex == deviceTrackingData_.Size() - 1);
    deviceTrackingData_.EraseSwap(implIndex);

    implIndexFromTrackedDeviceIndex_.Erase(deviceIndex);

    if (!isLast)
    {
        // Moved a devicedata so need to update the implindex map
        vr::TrackedDeviceIndex_t const newDeviceAtImplIndex = deviceTrackingData_[implIndex].deviceIndex_;
        implIndexFromTrackedDeviceIndex_[newDeviceAtImplIndex] = implIndex;
    }
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
