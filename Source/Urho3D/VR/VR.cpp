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

void VR::SendDeviceConnectedEvent(unsigned deviceId, VRDeviceClass deviceClass, VRControllerRole controllerRole)
{
    URHO3D_LOGINFOF("VRDeviceConnected with device %d, class %d, controllerRole %d", deviceId, deviceClass, controllerRole);

    using namespace VRDeviceConnected;

    VariantMap& eventData = GetEventDataMap();

    eventData[P_DEVICEID] = deviceId;
    eventData[P_DEVICECLASS] = deviceClass;
    eventData[P_CONTROLLERROLE] = controllerRole;

    SendEvent(E_VRDEVICECONNECTED, eventData);
}

void VR::SendDeviceDisconnectedEvent(unsigned deviceId, VRDeviceClass deviceClass, VRControllerRole controllerRole)
{
    URHO3D_LOGINFOF("VRDeviceDisconnected with device %d, class %d, controllerRole %d", deviceId, deviceClass, controllerRole);

    using namespace VRDeviceDisconnected;

    VariantMap& eventData = GetEventDataMap();

    eventData[P_DEVICEID] = deviceId;
    eventData[P_DEVICECLASS] = deviceClass;
    eventData[P_CONTROLLERROLE] = controllerRole;

    SendEvent(E_VRDEVICEDISCONNECTED, eventData);
}

}
