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

void VR::SendDeviceConnectedEvent(StringHash eventType, unsigned deviceId, VRDeviceClass deviceClass, VRControllerRole controllerRole)
{
    using namespace VRDeviceConnected;

    VariantMap& eventData = GetEventDataMap();

    eventData[P_DEVICEID] = deviceId;
    eventData[P_DEVICECLASS] = deviceClass;
    eventData[P_CONTROLLERROLE] = controllerRole;

    SendEvent(eventType, eventData);
}

}
