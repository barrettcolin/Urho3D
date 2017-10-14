#include "../Precompiled.h"

#include "VR.h"

#include "../Core/CoreEvents.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/RenderPath.h"
#include "../Graphics/Texture2D.h"
#include "../IO/Log.h"
#include "../Scene/Scene.h"

#include "../DebugNew.h"

namespace Urho3D
{

const Matrix3x4& VR::GetTrackingFromDeviceTransform(VRDeviceType vrDevice) const
{
    assert(vrDevice >= 0 && vrDevice < NUM_VR_DEVICE_TYPES);
    return trackingFromDevice_[vrDevice];
}

void VR::SendDeviceEvent(StringHash eventType, VRDeviceType deviceType, VRTrackingResult trackingResult)
{
    using namespace VRDeviceConnected;

    VariantMap& eventData = GetEventDataMap();

    eventData[P_DEVICETYPE] = deviceType;
    eventData[P_TRACKINGRESULT] = trackingResult;

    SendEvent(eventType, eventData);
}

}
