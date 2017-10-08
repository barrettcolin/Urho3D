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
    VRDEVICE_INVALID = 0,
    VRDEVICE_HMD,
    VRDEVICE_CONTROLLER_LEFT,
    VRDEVICE_CONTROLLER_RIGHT,

    NUM_VR_DEVICE_TYPES,
    NUM_VALID_VR_DEVICE_TYPES = NUM_VR_DEVICE_TYPES - 1
};

enum VRTrackingResult
{
    VRTRACKING_INVALID = 1,
    VRTRACKING_CALIBRATION_INPROGRESS = 100,
    VRTRACKING_CALIBRATION_OUTOFRANGE = 101,
    VRTRACKING_RUNNING_OK = 200,
    VRTRACKING_RUNNING_OUTOFRANGE = 201
};

class URHO3D_API VR : public Object
{
    URHO3D_OBJECT(VR, Object);

public:

    VR(Context* context_);

    virtual ~VR();

    void SetRenderResolutionScale(float renderResolutionScale);

    void SetNearClip(float nearClip);

    void SetFarClip(float farClip);

    void SetRenderPath(RenderPath* renderPath);

    void SetScene(Scene* scene);

    void SetWorldFromVRTransform(const Matrix3x4& worldFromVR);

    const Matrix3x4& GetWorldFromVRTransform() const { return worldFromVR_; }

    const Matrix3x4& GetVRFromDeviceTransform(VRDeviceType vrDevice) const;

private:

    int InitializeVR();

    void ShutdownVR();

    void CreateHMDNodeAndTextures();

    void DestroyHMDNodeAndTextures();
        
    void SubscribeToViewEvents();

    void UnsubscribeFromViewEvents();

    void UpdatePosesThisFrame();

    void SendDeviceEvent(StringHash eventType, VRDeviceType deviceType, VRTrackingResult trackingResult);

    void HandleBeginFrame(StringHash eventType, VariantMap& eventData);

    void HandleBeginViewUpdate(StringHash eventType, VariantMap& eventData);

    void HandleEndViewRender(StringHash eventType, VariantMap& eventData);

private:

    VRImpl* vrImpl_;

    float renderResolutionScale_;

    float nearClip_;

    float farClip_;

    bool renderParamsDirty_;

    unsigned currentFrame_;

    unsigned posesUpdatedFrame_;

    WeakPtr<RenderPath> renderPath_;

    WeakPtr<Scene> scene_;

    SharedPtr<Node> HMDNode_;

    SharedPtr<Texture2D> cameraTextures_[2];

    Matrix3x4 worldFromVR_;

    unsigned deviceFrame_[NUM_VALID_VR_DEVICE_TYPES];

    Matrix3x4 VRFromDevice_[NUM_VALID_VR_DEVICE_TYPES];
};

}
