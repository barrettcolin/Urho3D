//
// Copyright (c) 2008-2017 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Urho3D/Engine/Application.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Engine/Console.h>
#include <Urho3D/UI/Cursor.h>
#include <Urho3D/Engine/DebugHud.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Input/InputEvents.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SceneEvents.h>
#include <Urho3D/UI/Sprite.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Core/Timer.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Graphics/GraphicsEvents.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/VR/VR.h>

Sample::Sample(Context* context) :
    Application(context),
    yaw_(0.0f),
    pitch_(0.0f),
    touchEnabled_(false),
    useMouseMode_(MM_ABSOLUTE),
    screenJoystickIndex_(M_MAX_UNSIGNED),
    screenJoystickSettingsIndex_(M_MAX_UNSIGNED),
    paused_(false)
{
}

void Sample::Setup()
{
    // Modify engine startup parameters
    engineParameters_[EP_WINDOW_TITLE] = GetTypeName();
    engineParameters_[EP_LOG_NAME]     = GetSubsystem<FileSystem>()->GetAppPreferencesDir("urho3d", "logs") + GetTypeName() + ".log";
    engineParameters_[EP_FULL_SCREEN]  = false;
    engineParameters_[EP_HEADLESS]     = false;
    engineParameters_[EP_SOUND]        = false;

    // Construct a search path to find the resource prefix with two entries:
    // The first entry is an empty path which will be substituted with program/bin directory -- this entry is for binary when it is still in build tree
    // The second and third entries are possible relative paths from the installed program/bin directory to the asset directory -- these entries are for binary when it is in the Urho3D SDK installation location
    if (!engineParameters_.Contains(EP_RESOURCE_PREFIX_PATHS))
        engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ";../share/Resources;../share/Urho3D/Resources";
}

void Sample::Start()
{
    if (GetPlatform() == "Android" || GetPlatform() == "iOS")
        // On mobile platform, enable touch by adding a screen joystick
        InitTouchInput();
    else if (GetSubsystem<Input>()->GetNumJoysticks() == 0)
        // On desktop platform, do not detect touch when we already got a joystick
        SubscribeToEvent(E_TOUCHBEGIN, URHO3D_HANDLER(Sample, HandleTouchBegin));

    // Create logo
    CreateLogo();

    // Set custom window Title & Icon
    SetWindowTitleAndIcon();

    // Create console and debug HUD
    CreateConsoleAndDebugHud();

    // Subscribe key down event
    SubscribeToEvent(E_KEYDOWN, URHO3D_HANDLER(Sample, HandleKeyDown));
    // Subscribe key up event
    SubscribeToEvent(E_KEYUP, URHO3D_HANDLER(Sample, HandleKeyUp));
    // Subscribe scene update event
    SubscribeToEvent(E_SCENEUPDATE, URHO3D_HANDLER(Sample, HandleSceneUpdate));

#if defined(URHO3D_VR)
    // Register VR subsystem
    context_->RegisterSubsystem(new VR(context_));
    // Subscribe VR device connected event
    SubscribeToEvent(E_VRDEVICECONNECTED, URHO3D_HANDLER(Sample, HandleVRDeviceConnected));
    // Subscribe VR device disconnected event
    SubscribeToEvent(E_VRDEVICEDISCONNECTED, URHO3D_HANDLER(Sample, HandleVRDeviceDisconnected));
    // Subscribe VR device pose updated for rendering event
    SubscribeToEvent(E_VRDEVICEPOSEUPDATEDFORRENDERING, URHO3D_HANDLER(Sample, HandleVRDevicePoseUpdatedForRendering));
    // Subscribe end rendering event
    SubscribeToEvent(E_ENDRENDERING, URHO3D_HANDLER(Sample, HandleEndRendering));
#endif
}

void Sample::Stop()
{
    engine_->DumpResources(true);
}

void Sample::InitTouchInput()
{
    touchEnabled_ = true;

    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Input* input = GetSubsystem<Input>();
    XMLFile* layout = cache->GetResource<XMLFile>("UI/ScreenJoystick_Samples.xml");
    const String& patchString = GetScreenJoystickPatchString();
    if (!patchString.Empty())
    {
        // Patch the screen joystick layout further on demand
        SharedPtr<XMLFile> patchFile(new XMLFile(context_));
        if (patchFile->FromString(patchString))
            layout->Patch(patchFile);
    }
    screenJoystickIndex_ = (unsigned)input->AddScreenJoystick(layout, cache->GetResource<XMLFile>("UI/DefaultStyle.xml"));
    input->SetScreenJoystickVisible(screenJoystickSettingsIndex_, true);
}

void Sample::InitMouseMode(MouseMode mode)
{
    useMouseMode_ = mode;

    Input* input = GetSubsystem<Input>();

    if (GetPlatform() != "Web")
    {
        if (useMouseMode_ == MM_FREE)
            input->SetMouseVisible(true);

        Console* console = GetSubsystem<Console>();
        if (useMouseMode_ != MM_ABSOLUTE)
        {
            input->SetMouseMode(useMouseMode_);
            if (console && console->IsVisible())
                input->SetMouseMode(MM_ABSOLUTE, true);
        }
    }
    else
    {
        input->SetMouseVisible(true);
        SubscribeToEvent(E_MOUSEBUTTONDOWN, URHO3D_HANDLER(Sample, HandleMouseModeRequest));
        SubscribeToEvent(E_MOUSEMODECHANGED, URHO3D_HANDLER(Sample, HandleMouseModeChange));
    }
}

void Sample::SetLogoVisible(bool enable)
{
    if (logoSprite_)
        logoSprite_->SetVisible(enable);
}

void Sample::CreateLogo()
{
    // Get logo texture
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Texture2D* logoTexture = cache->GetResource<Texture2D>("Textures/FishBoneLogo.png");
    if (!logoTexture)
        return;

    // Create logo sprite and add to the UI layout
    UI* ui = GetSubsystem<UI>();
    logoSprite_ = ui->GetRoot()->CreateChild<Sprite>();

    // Set logo sprite texture
    logoSprite_->SetTexture(logoTexture);

    int textureWidth = logoTexture->GetWidth();
    int textureHeight = logoTexture->GetHeight();

    // Set logo sprite scale
    logoSprite_->SetScale(256.0f / textureWidth);

    // Set logo sprite size
    logoSprite_->SetSize(textureWidth, textureHeight);

    // Set logo sprite hot spot
    logoSprite_->SetHotSpot(textureWidth, textureHeight);

    // Set logo sprite alignment
    logoSprite_->SetAlignment(HA_RIGHT, VA_BOTTOM);

    // Make logo not fully opaque to show the scene underneath
    logoSprite_->SetOpacity(0.9f);

    // Set a low priority for the logo so that other UI elements can be drawn on top
    logoSprite_->SetPriority(-100);
}

void Sample::SetWindowTitleAndIcon()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Graphics* graphics = GetSubsystem<Graphics>();
    Image* icon = cache->GetResource<Image>("Textures/UrhoIcon.png");
    graphics->SetWindowIcon(icon);
    graphics->SetWindowTitle("Urho3D Sample");
}

void Sample::CreateConsoleAndDebugHud()
{
    // Get default style
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    XMLFile* xmlFile = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");

    // Create console
    Console* console = engine_->CreateConsole();
    console->SetDefaultStyle(xmlFile);
    console->GetBackground()->SetOpacity(0.8f);

    // Create debug HUD.
    DebugHud* debugHud = engine_->CreateDebugHud();
    debugHud->SetDefaultStyle(xmlFile);
}

void Sample::CreateHMDNodeAndTextures(float nearClip, float farClip, RenderPath* renderPath)
{
#if defined(URHO3D_VR)
    VR* vr = GetSubsystem<VR>();

    unsigned textureWidth, textureHeight;
    vr->GetRecommendedRenderTargetSize(textureWidth, textureHeight);

    HMDNode_ = new Node(context_);

    for (int eye = 0; eye < 2; ++eye)
    {
        // Create texture
        cameraTextures_[eye] = new Texture2D(context_);
        cameraTextures_[eye]->SetSize(textureWidth, textureHeight, Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
        cameraTextures_[eye]->SetFilterMode(FILTER_BILINEAR);

        // Set render surface to always update
        RenderSurface* surface = cameraTextures_[eye]->GetRenderSurface();
        surface->SetUpdateMode(SURFACE_UPDATEALWAYS);

        // Create camera node
        Node* cameraNode = HMDNode_->CreateChild(0 == eye ? "LeftEye" : "RightEye");
        {
            Matrix3x4 headFromEye;
            vr->GetHeadFromEyeTransform(VREye(eye), headFromEye);

            cameraNode->SetTransform(headFromEye);

            // Create camera component
            Camera* eyeCamera = cameraNode->CreateComponent<Camera>();
            {
                Matrix4 proj;
                vr->GetEyeProjection(VREye(eye), nearClip, farClip, proj);
                eyeCamera->SetProjection(proj);

                // Set scene camera viewport
                SharedPtr<Viewport> eyeViewport(new Viewport(context_, scene_, eyeCamera));

                // Set viewport render path
                eyeViewport->SetRenderPath(renderPath);

                surface->SetViewport(0, eyeViewport);
            }
        }
    }
#endif
}

void Sample::DestroyHMDNodeAndTextures()
{
#if defined(URHO3D_VR)
    HMDNode_ = 0;

    for (int eye = 0; eye < 2; ++eye)
    {
        cameraTextures_[eye] = 0;
    }
#endif
}

void Sample::HandleKeyUp(StringHash /*eventType*/, VariantMap& eventData)
{
    using namespace KeyUp;

    int key = eventData[P_KEY].GetInt();

    // Close console (if open) or exit when ESC is pressed
    if (key == KEY_ESCAPE)
    {
        Console* console = GetSubsystem<Console>();
        if (console->IsVisible())
            console->SetVisible(false);
        else
        {
            if (GetPlatform() == "Web")
            {
                GetSubsystem<Input>()->SetMouseVisible(true);
                if (useMouseMode_ != MM_ABSOLUTE)
                    GetSubsystem<Input>()->SetMouseMode(MM_FREE);
            }
            else
                engine_->Exit();
        }
    }
}

void Sample::HandleKeyDown(StringHash /*eventType*/, VariantMap& eventData)
{
    using namespace KeyDown;

    int key = eventData[P_KEY].GetInt();

    // Toggle console with F1
    if (key == KEY_F1)
        GetSubsystem<Console>()->Toggle();

    // Toggle debug HUD with F2
    else if (key == KEY_F2)
        GetSubsystem<DebugHud>()->ToggleAll();

    // Common rendering quality controls, only when UI has no focused element
    else if (!GetSubsystem<UI>()->GetFocusElement())
    {
        Renderer* renderer = GetSubsystem<Renderer>();

        // Preferences / Pause
        if (key == KEY_SELECT && touchEnabled_)
        {
            paused_ = !paused_;

            Input* input = GetSubsystem<Input>();
            if (screenJoystickSettingsIndex_ == M_MAX_UNSIGNED)
            {
                // Lazy initialization
                ResourceCache* cache = GetSubsystem<ResourceCache>();
                screenJoystickSettingsIndex_ = (unsigned)input->AddScreenJoystick(cache->GetResource<XMLFile>("UI/ScreenJoystickSettings_Samples.xml"), cache->GetResource<XMLFile>("UI/DefaultStyle.xml"));
            }
            else
                input->SetScreenJoystickVisible(screenJoystickSettingsIndex_, paused_);
        }

        // Texture quality
        else if (key == '1')
        {
            int quality = renderer->GetTextureQuality();
            ++quality;
            if (quality > QUALITY_HIGH)
                quality = QUALITY_LOW;
            renderer->SetTextureQuality(quality);
        }

        // Material quality
        else if (key == '2')
        {
            int quality = renderer->GetMaterialQuality();
            ++quality;
            if (quality > QUALITY_HIGH)
                quality = QUALITY_LOW;
            renderer->SetMaterialQuality(quality);
        }

        // Specular lighting
        else if (key == '3')
            renderer->SetSpecularLighting(!renderer->GetSpecularLighting());

        // Shadow rendering
        else if (key == '4')
            renderer->SetDrawShadows(!renderer->GetDrawShadows());

        // Shadow map resolution
        else if (key == '5')
        {
            int shadowMapSize = renderer->GetShadowMapSize();
            shadowMapSize *= 2;
            if (shadowMapSize > 2048)
                shadowMapSize = 512;
            renderer->SetShadowMapSize(shadowMapSize);
        }

        // Shadow depth and filtering quality
        else if (key == '6')
        {
            ShadowQuality quality = renderer->GetShadowQuality();
            quality = (ShadowQuality)(quality + 1);
            if (quality > SHADOWQUALITY_BLUR_VSM)
                quality = SHADOWQUALITY_SIMPLE_16BIT;
            renderer->SetShadowQuality(quality);
        }

        // Occlusion culling
        else if (key == '7')
        {
            bool occlusion = renderer->GetMaxOccluderTriangles() > 0;
            occlusion = !occlusion;
            renderer->SetMaxOccluderTriangles(occlusion ? 5000 : 0);
        }

        // Instancing
        else if (key == '8')
            renderer->SetDynamicInstancing(!renderer->GetDynamicInstancing());

        // Take screenshot
        else if (key == '9')
        {
            Graphics* graphics = GetSubsystem<Graphics>();
            Image screenshot(context_);
            graphics->TakeScreenShot(screenshot);
            // Here we save in the Data folder with date and time appended
            screenshot.SavePNG(GetSubsystem<FileSystem>()->GetProgramDir() + "Data/Screenshot_" +
                Time::GetTimeStamp().Replaced(':', '_').Replaced('.', '_').Replaced(' ', '_') + ".png");
        }
    }
}

void Sample::HandleSceneUpdate(StringHash /*eventType*/, VariantMap& eventData)
{
    // Move the camera by touch, if the camera node is initialized by descendant sample class
    if (touchEnabled_ && cameraNode_)
    {
        Input* input = GetSubsystem<Input>();
        for (unsigned i = 0; i < input->GetNumTouches(); ++i)
        {
            TouchState* state = input->GetTouch(i);
            if (!state->touchedElement_)    // Touch on empty space
            {
                if (state->delta_.x_ ||state->delta_.y_)
                {
                    Camera* camera = cameraNode_->GetComponent<Camera>();
                    if (!camera)
                        return;

                    Graphics* graphics = GetSubsystem<Graphics>();
                    yaw_ += TOUCH_SENSITIVITY * camera->GetFov() / graphics->GetHeight() * state->delta_.x_;
                    pitch_ += TOUCH_SENSITIVITY * camera->GetFov() / graphics->GetHeight() * state->delta_.y_;

                    // Construct new orientation for the camera scene node from yaw and pitch; roll is fixed to zero
                    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
                }
                else
                {
                    // Move the cursor to the touch position
                    Cursor* cursor = GetSubsystem<UI>()->GetCursor();
                    if (cursor && cursor->IsVisible())
                        cursor->SetPosition(state->position_);
                }
            }
        }
    }
}

void Sample::HandleTouchBegin(StringHash /*eventType*/, VariantMap& eventData)
{
    // On some platforms like Windows the presence of touch input can only be detected dynamically
    InitTouchInput();
    UnsubscribeFromEvent("TouchBegin");
}

// If the user clicks the canvas, attempt to switch to relative mouse mode on web platform
void Sample::HandleMouseModeRequest(StringHash /*eventType*/, VariantMap& eventData)
{
    Console* console = GetSubsystem<Console>();
    if (console && console->IsVisible())
        return;
    Input* input = GetSubsystem<Input>();
    if (useMouseMode_ == MM_ABSOLUTE)
        input->SetMouseVisible(false);
    else if (useMouseMode_ == MM_FREE)
        input->SetMouseVisible(true);
    input->SetMouseMode(useMouseMode_);
}

void Sample::HandleMouseModeChange(StringHash /*eventType*/, VariantMap& eventData)
{
    Input* input = GetSubsystem<Input>();
    bool mouseLocked = eventData[MouseModeChanged::P_MOUSELOCKED].GetBool();
    input->SetMouseVisible(!mouseLocked);
}

void Sample::HandleVRDeviceConnected(StringHash eventType, VariantMap& eventData)
{
#if defined(URHO3D_VR)
    using namespace VRDeviceConnected;

    const VRDeviceType deviceType = VRDeviceType(eventData[P_DEVICETYPE].GetInt());

    switch (deviceType)
    {
    case VRDEVICE_HMD:
        if (scene_ && cameraNode_)
        {
            Camera* camera = cameraNode_->GetComponent<Camera>();
            if (camera)
            {
                Renderer* renderer = GetSubsystem<Renderer>();
                CreateHMDNodeAndTextures(camera->GetNearClip(), camera->GetFarClip(), renderer->GetViewport(0)->GetRenderPath());

                worldFromVR_ = Matrix3x4(cameraNode_->GetPosition(), Quaternion::IDENTITY, 1.0f);
            }
        }
        break;

    case VRDEVICE_CONTROLLER_LEFT:
    case VRDEVICE_CONTROLLER_RIGHT:
        {
            const int controllerIndex = (deviceType - VRDEVICE_CONTROLLER_LEFT);
            VRControllerNode_[controllerIndex] = 
                scene_->CreateChild(deviceType == VRDEVICE_CONTROLLER_LEFT ? "VRLeftController" : "VRRightController");

            Node* VRControllerModelNode = VRControllerNode_[controllerIndex]->CreateChild();
            VRControllerModelNode->SetScale(0.1f);

            StaticModel* controllerModel = VRControllerModelNode->CreateComponent<StaticModel>();
            {
                ResourceCache* cache = GetSubsystem<ResourceCache>();

                controllerModel->SetModel(cache->GetResource<Model>("Models/Editor/Axes.mdl"));

                controllerModel->SetMaterial(0, cache->GetResource<Material>("Materials/Editor/RedUnlit.xml"));
                controllerModel->SetMaterial(1, cache->GetResource<Material>("Materials/Editor/GreenUnlit.xml"));
                controllerModel->SetMaterial(2, cache->GetResource<Material>("Materials/Editor/BlueUnlit.xml"));
            }
        }
        break;
    }
#endif
}

void Sample::HandleVRDeviceDisconnected(StringHash eventType, VariantMap& eventData)
{
#if defined(URHO3D_VR)
    using namespace VRDeviceDisconnected;

    const VRDeviceType deviceType = VRDeviceType(eventData[P_DEVICETYPE].GetInt());

    switch (deviceType)
    {
    case VRDEVICE_HMD:
        DestroyHMDNodeAndTextures();
        break;

    case VRDEVICE_CONTROLLER_LEFT:
    case VRDEVICE_CONTROLLER_RIGHT:
        {
            const int controllerIndex = (deviceType - VRDEVICE_CONTROLLER_LEFT);

            if (VRControllerNode_[controllerIndex])
            {
                VRControllerNode_[controllerIndex]->Remove();
                VRControllerNode_[controllerIndex] = 0;
            }
        }
        break;
    }
#endif
}

void Sample::HandleVRDevicePoseUpdatedForRendering(StringHash eventType, VariantMap& eventData)
{
#if defined(URHO3D_VR)
    using namespace VRDevicePoseUpdatedForRendering;

    const VRDeviceType deviceType = VRDeviceType(eventData[P_DEVICETYPE].GetInt());

    switch (deviceType)
    {
    case VRDEVICE_HMD:
        if(HMDNode_)
        {
            VR* vr = GetSubsystem<VR>();
            HMDNode_->SetTransform(worldFromVR_ * vr->GetTrackingFromDeviceTransform(deviceType));
        }
        break;

    case VRDEVICE_CONTROLLER_LEFT:
    case VRDEVICE_CONTROLLER_RIGHT:
        {
            const int controllerIndex = (deviceType - VRDEVICE_CONTROLLER_LEFT);

            if (VRControllerNode_[controllerIndex])
            {
                VR* vr = GetSubsystem<VR>();
                VRControllerNode_[controllerIndex]->SetTransform(worldFromVR_ * vr->GetTrackingFromDeviceTransform(deviceType));
            }
        }
        break;
    }
#endif
}

void Sample::HandleEndRendering(StringHash eventType, VariantMap& eventData)
{
#if defined(URHO3D_VR)
    VR* vr = GetSubsystem<VR>();

    for (int eye = 0; eye < 2; ++eye)
    {
        if (cameraTextures_[eye])
        {
            vr->SubmitEyeTexture(VREye(eye), cameraTextures_[eye]);
        }
    }
#endif
}
