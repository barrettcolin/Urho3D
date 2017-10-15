#include "../../Precompiled.h"

#include "OpenVRImpl.h"

#include "../../IO/Log.h"

#include "../../DebugNew.h"

namespace Urho3D
{

VRImpl::VRImpl()
{
    vr::EVRInitError err;
    vrSystem_ = vr::VR_Init(&err, vr::VRApplication_Scene);

    if (err != vr::VRInitError_None)
    {
        URHO3D_LOGERRORF("vr::VR_Init failed with error %d", err);
    }
}

VRImpl::~VRImpl()
{
    if (vrSystem_)
    {
        vr::VR_Shutdown();
        vrSystem_ = 0;
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
Urho3D::Matrix3x4 VRImpl::UrhoAffineTransformFromOpenVR(vr::HmdMatrix34_t const& in)
{
    return Urho3D::Matrix3x4(
        in.m[0][0], in.m[0][1], -in.m[0][2], in.m[0][3],
        in.m[1][0], in.m[1][1], -in.m[1][2], in.m[1][3],
        -in.m[2][0], -in.m[2][1], in.m[2][2], -in.m[2][3]
    );
}

Urho3D::Matrix4 VRImpl::UrhoProjectionFromOpenVR(vr::HmdMatrix44_t const& in)
{
    return Urho3D::Matrix4(
        in.m[0][0], in.m[0][1], -in.m[0][2], in.m[0][3],
        in.m[1][0], in.m[1][1], -in.m[1][2], in.m[1][3],
        in.m[2][0], in.m[2][1], -in.m[2][2], in.m[2][3],
        in.m[3][0], in.m[3][1], -in.m[3][2], in.m[3][3]
    );
}

}
