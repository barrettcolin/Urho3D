#pragma once

#include "../Core/Object.h"

namespace Urho3D
{

URHO3D_EVENT(E_VRDEVICECONNECTED, VRDeviceConnected)
{
    URHO3D_PARAM(P_DEVICEID, DeviceId);             // int
    URHO3D_PARAM(P_DEVICECLASS, DeviceClass);       // int
    URHO3D_PARAM(P_CONTROLLERROLE, ControllerRole); // int
}

URHO3D_EVENT(E_VRDEVICEDISCONNECTED, VRDeviceDisconnected)
{
    URHO3D_PARAM(P_DEVICEID, DeviceId);             // int
    URHO3D_PARAM(P_DEVICECLASS, DeviceClass);       // int
    URHO3D_PARAM(P_CONTROLLERROLE, ControllerRole); // int
}

URHO3D_EVENT(E_VRDEVICEPOSESUPDATEDFORRENDERING, VRDevicePosesUpdatedForRendering)
{
}

class Node;
class RenderPath;
class Scene;
class Texture2D;
class VRImpl;

enum VRDeviceClass
{
    VRDEVICE_INVALID = 0,
    VRDEVICE_HMD,
    VRDEVICE_CONTROLLER,

    NUM_VR_DEVICE_CLASSES
};

enum VRControllerRole
{
    VRCONTROLLER_INVALID = 0,
    VRCONTROLLER_LEFTHAND,
    VRCONTROLLER_RIGHTHAND,

    NUM_CONTROLLER_ROLES
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

    bool IsInitialized() const;

    unsigned GetNumDevices() const;

    unsigned GetDeviceId(unsigned deviceIndex) const;

    VRDeviceClass GetDeviceClass(unsigned deviceIndex) const;

    VRControllerRole GetControllerRole(unsigned deviceIndex) const;

    void GetRecommendedRenderTargetSize(unsigned& widthOut, unsigned& heightOut) const;

    void GetEyeProjection(VREye eye, float nearClip, float farClip, Matrix4& projOut) const;

    void GetHeadFromEyeTransform(VREye eye, Matrix3x4& headFromEyeOut) const;

    const Matrix3x4& GetTrackingFromDeviceTransform(unsigned deviceIndex) const;

    void SubmitEyeTexture(VREye eye, Texture2D* texture);

protected:

    void SendDeviceConnectedEvent(unsigned deviceId, VRDeviceClass deviceClass, VRControllerRole controllerRole);

    void SendDeviceDisconnectedEvent(unsigned deviceId, VRDeviceClass deviceClass, VRControllerRole controllerRole);

private:

    void HandleBeginFrame(StringHash eventType, VariantMap& eventData);

    void HandleBeginRendering(StringHash eventType, VariantMap& eventData);

private:

    VRImpl* vrImpl_;
};

}
