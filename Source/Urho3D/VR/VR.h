#pragma once

#include "../Core/Object.h"

namespace Urho3D
{

class Node;
class RenderPath;
class Scene;
class Texture2D;
class VRImpl;

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

    const Matrix3x4& GetWorldFromHMDTransform() const { return worldFromHMD_; }

    const Matrix3x4& GetWorldFromControllerTransform(int controllerIndex) const { return worldFromController_[controllerIndex]; }

private:

    int InitializeVR();

    void ShutdownVR();

    void CreateHMDNodeAndTextures();

    void DestroyHMDNodeAndTextures();
        
    void SubscribeToViewEvents();

    void UnsubscribeFromViewEvents();

    void UpdatePosesThisFrame();

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

    bool posesUpdatedThisFrame_;

    WeakPtr<RenderPath> renderPath_;

    WeakPtr<Scene> scene_;

    SharedPtr<Node> HMDNode_;

    SharedPtr<Texture2D> cameraTextures_[2];

    Matrix3x4 worldFromVR_;

    unsigned HMDFrame_;

    unsigned controllerFrame_[2];

    Matrix3x4 worldFromHMD_;

    Matrix3x4 worldFromController_[2];
};

}
