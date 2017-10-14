#pragma once

#include "../Core/Object.h"

namespace Urho3D
{

URHO3D_EVENT(E_VRDEVICECONNECTED, VRDeviceConnected)
{
    URHO3D_PARAM(P_DEVICETYPE, DeviceType);         // int
    URHO3D_PARAM(P_TRACKINGRESULT, TrackingResult); // int
}

URHO3D_EVENT(E_VRDEVICEDISCONNECTED, VRDeviceDisconnected)
{
    URHO3D_PARAM(P_DEVICETYPE, DeviceType);         // int
    URHO3D_PARAM(P_TRACKINGRESULT, TrackingResult); // int
}

URHO3D_EVENT(E_VRDEVICETRACKINGCHANGED, VRDeviceTrackingChanged)
{
    URHO3D_PARAM(P_DEVICETYPE, DeviceType);         // int
    URHO3D_PARAM(P_TRACKINGRESULT, TrackingResult); // int
}

URHO3D_EVENT(E_VRDEVICEPOSEUPDATEDFORRENDERING, VRDevicePoseUpdatedForRendering)
{
    URHO3D_PARAM(P_DEVICETYPE, DeviceType);         // int
}

class Node;
class RenderPath;
class Scene;
class Texture2D;
class VRImpl;

enum VRDeviceType
{
    VRDEVICE_INVALID = -1,
    VRDEVICE_HMD = 0,
    VRDEVICE_CONTROLLER_LEFT,
    VRDEVICE_CONTROLLER_RIGHT,

    NUM_VR_DEVICE_TYPES
};

enum VRTrackingResult
{
    VRTRACKING_INVALID = 1,
    VRTRACKING_CALIBRATION_INPROGRESS = 100,
    VRTRACKING_CALIBRATION_OUTOFRANGE = 101,
    VRTRACKING_RUNNING_OK = 200,
    VRTRACKING_RUNNING_OUTOFRANGE = 201
};

enum VREye
{
    VREYE_LEFT = 0,
    VREYE_RIGHT,

    NUM_EYES
};

class URHO3D_API VR : public Object
{
    URHO3D_OBJECT(VR, Object);

public:

    VR(Context* context_);

    virtual ~VR();

    void GetRecommendedRenderTargetSize(unsigned& widthOut, unsigned& heightOut) const;

    void GetEyeProjection(VREye eye, float nearClip, float farClip, Matrix4& projOut) const;

    void GetHeadFromEyeTransform(VREye eye, Matrix3x4& headFromEyeOut) const;

    const Matrix3x4& GetTrackingFromDeviceTransform(VRDeviceType vrDevice) const;

    void SubmitEyeTexture(VREye eye, Texture2D* texture);

private:

    int InitializeVR();

    void ShutdownVR();

    void SendDeviceEvent(StringHash eventType, VRDeviceType deviceType, VRTrackingResult trackingResult);

    void HandleBeginRendering(StringHash eventType, VariantMap& eventData);

private:

    VRImpl* vrImpl_;

    Matrix3x4 trackingFromDevice_[NUM_VR_DEVICE_TYPES];
};

}
