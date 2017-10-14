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

public:
    /// Convert OpenVR pose transform to Urho
    static Urho3D::Matrix3x4 UrhoAffineTransformFromOpenVR(vr::HmdMatrix34_t const& in);
    /// Convert OpenVR eye projection to Urho
    static Urho3D::Matrix4 UrhoProjectionFromOpenVR(vr::HmdMatrix44_t const& in);

private:
    /// OpenVR VRSystem interface
    vr::IVRSystem* vrSystem_;
    /// ETrackingResult from device index
    DeviceTrackingDataFromIndex trackingResultFromDeviceIndex_;
};

}
