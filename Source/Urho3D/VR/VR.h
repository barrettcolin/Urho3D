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

private:

    int Initialize();

    void Shutdown();

    void HandleBeginFrame(StringHash eventType, VariantMap& eventData);

    void HandleBeginViewUpdate(StringHash eventType, VariantMap& eventData);

    void HandleEndViewRender(StringHash eventType, VariantMap& eventData);

    void UpdatePosesThisFrame();

private:

    VRImpl* vrImpl_;

    float renderResolutionScale_;

    float nearClip_;

    float farClip_;

    bool renderParamsDirty_;

    bool posesUpdatedThisFrame_;

    WeakPtr<RenderPath> renderPath_;

    WeakPtr<Scene> scene_;

    SharedPtr<Node> headNode_;

    SharedPtr<Texture2D> cameraTextures_[2];

    Matrix3x4 worldFromVR_;
};

}
