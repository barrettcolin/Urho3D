#pragma once

#include "../Core/Object.h"

namespace vr
{
    class IVRSystem;
}

namespace Urho3D
{

class Texture2D;

class URHO3D_API VR : public Object
{
    URHO3D_OBJECT(VR, Object);

    struct EyeData
    {
        Urho3D::Matrix3x4 eyeToHeadTransform_;
        Urho3D::Matrix4 projection_;
    };

public:

    enum Eye
    {
        EYE_LEFT,
        EYE_RIGHT,

        NUM_EYES
    };

    VR(Context* context_);

    virtual ~VR();

    unsigned GetRecommendedRenderTargetWidth() const { return recommendedRenderTargetWidth_; }

    unsigned GetRecommendedRenderTargetHeight() const { return recommendedRenderTargetHeight_; }

    const Matrix3x4& GetWorldFromHeadTransform() const;

    const Matrix3x4& GetHeadFromEyeTransform(Eye eye) const { return eyeData_[eye].eyeToHeadTransform_; }

    const Matrix4& GetProjection(Eye eye) const { return eyeData_[eye].projection_; }

    void SetEyeNearAndFar(float eyeNear, float eyeFar);

    void UpdatePosesBeforeRendering();

    void SubmitEyeTexturesAfterRendering(Texture2D* eyeTextures[NUM_EYES]);

private:

    void Initialize();

private:

    struct VRData* vrData_;

    vr::IVRSystem* vrSystem_;

    unsigned recommendedRenderTargetWidth_;

    unsigned recommendedRenderTargetHeight_;

    EyeData eyeData_[NUM_EYES];
};

}
