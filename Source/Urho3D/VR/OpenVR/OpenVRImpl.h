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
    friend class VR;

public:
    /// DeviceTrackingData
    struct DeviceTrackingData
    {
        vr::TrackedDeviceIndex_t deviceIndex_;
        vr::ETrackedDeviceClass deviceClass_;
        vr::ETrackedControllerRole controllerRole_;
        vr::VRControllerState001_t controllerState_;
        Matrix3x4 trackingFromDevice_;
        bool poseValid_;

        /// Construct
        DeviceTrackingData(vr::TrackedDeviceIndex_t deviceIndex = vr::k_unTrackedDeviceIndexInvalid, 
            vr::ETrackedDeviceClass deviceClass = vr::TrackedDeviceClass_Invalid, 
            vr::ETrackedControllerRole controllerRole = vr::TrackedControllerRole_Invalid);
    };

public:
    /// Construct.
    VRImpl();
    /// Destruct.
    ~VRImpl();

public:

    int ActivateDevice(vr::TrackedDeviceIndex_t deviceIndex);

    void DeactivateDevice(vr::TrackedDeviceIndex_t deviceIndex);
    /// Convert OpenVR pose transform to Urho
    static void UrhoAffineTransformFromOpenVR(vr::HmdMatrix34_t const& in, Matrix3x4& out);
    /// Convert OpenVR eye projection to Urho
    static void UrhoProjectionFromOpenVR(vr::HmdMatrix44_t const& in, Matrix4& out);

private:
    int Initialize();

    /// OpenVR VRSystem interface
    vr::IVRSystem* vrSystem_;

    Vector<DeviceTrackingData> deviceTrackingData_;

    HashMap<vr::TrackedDeviceIndex_t, int> implIndexFromTrackedDeviceIndex_;
};

}
