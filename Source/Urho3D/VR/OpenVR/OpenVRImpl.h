#pragma once

#include "../../Container/HashMap.h"
#include "../../Math/Matrix3x4.h"
#include "../../Math/Matrix4.h"

// Match OpenVR C++ static lib exports
#define VR_API_EXPORT 1
#include <openvr/openvr.h>

namespace Urho3D
{

template<class K, class V> class HashMap;

/// %VR implementation. Holds API-specific objects.
class URHO3D_API VRImpl
{
public:
    /// DeviceData
    struct DeviceData
    {
        vr::TrackedDeviceIndex_t openVRDeviceIndex_;
        vr::ETrackedDeviceClass deviceClass_;
        vr::ETrackedControllerRole controllerRole_;
        vr::VRControllerState001_t controllerState_;
        Matrix3x4 trackingFromDevice_;
        bool poseValid_;

        /// Construct
        DeviceData(vr::TrackedDeviceIndex_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid, 
            vr::ETrackedDeviceClass deviceClass = vr::TrackedDeviceClass_Invalid, 
            vr::ETrackedControllerRole controllerRole = vr::TrackedControllerRole_Invalid);
    };

public:
    /// Construct.
    VRImpl();
    /// Destruct.
    ~VRImpl();
    /// Initialize
    int Initialize();
    /// Is OpenVR initialized?
    inline bool IsInitialized() const;
    /// Activate device at OpenVR index, returning newly allocated implementation index
    unsigned ActivateDevice(vr::TrackedDeviceIndex_t deviceIndex);
    /// Deactivate device at OpenVR index
    void DeactivateDevice(vr::TrackedDeviceIndex_t deviceIndex);
    /// Get number of devices
    inline unsigned GetNumDevices() const;
    /// Get device at index
    inline DeviceData const& GetDevice(unsigned index) const;
    /// Access device at index
    inline DeviceData& AccessDevice(unsigned index);
    /// Get impl index for device at OpenVR index (returns false if the OpenVR device isn't connected)
    inline bool GetIndexForOpenVRDevice(vr::TrackedDeviceIndex_t deviceIndex, unsigned& implIndexOut) const;

public:
    /// Convert OpenVR pose transform to Urho
    static void UrhoAffineTransformFromOpenVR(vr::HmdMatrix34_t const& in, Matrix3x4& out);
    /// Convert OpenVR eye projection to Urho
    static void UrhoProjectionFromOpenVR(vr::HmdMatrix44_t const& in, Matrix4& out);

public:
    /// OpenVR VRSystem interface
    vr::IVRSystem* vrSystem_;

private:

    Vector<DeviceData> deviceData_;

    bool trackedDeviceConnected_[vr::k_unMaxTrackedDeviceCount];

    unsigned implIndexFromTrackedDeviceIndex_[vr::k_unMaxTrackedDeviceCount];
};

inline bool VRImpl::IsInitialized() const
{
    return vrSystem_ != 0;
}

inline unsigned VRImpl::GetNumDevices() const
{
    return deviceData_.Size();
}

inline VRImpl::DeviceData const& VRImpl::GetDevice(unsigned index) const
{
    return deviceData_[index];
}

inline VRImpl::DeviceData& VRImpl::AccessDevice(unsigned index)
{
    return deviceData_[index];
}

inline bool VRImpl::GetIndexForOpenVRDevice(vr::TrackedDeviceIndex_t deviceIndex, unsigned& implIndexOut) const
{
    assert(deviceIndex >= 0 && deviceIndex < vr::k_unMaxTrackedDeviceCount);
    if (trackedDeviceConnected_[deviceIndex])
    {
        implIndexOut = implIndexFromTrackedDeviceIndex_[deviceIndex];
        return true;
    }
    return false;
}

}
