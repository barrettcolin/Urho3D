#pragma once

#include "Application.h"

namespace Urho3D
{
    class Model;
    class Scene;
    class Texture2D;
}

class Q2App : public Urho3D::Application
{
    OBJECT(Q2App);

public:
    static Q2App& GetInstance() { return *s_instance; }

    Q2App(Urho3D::Context* context);

    void OnSysInit();

    void OnSysQuit();

    void OnVidCheckChanges();

    bool OnRefSetMode(int& width, int& height, int mode, bool fullscreen);

    void OnRefSetPalette(unsigned char const* palette);

    // Application
    void Setup() override;

    void Start() override;

    void Stop() override;

protected:
    void CreateRenderScenes(int width, int height);

    void HandleKeyDown(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

    void HandleKeyUp(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

    void HandleUpdate(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

    void HandleExitRequested(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

private:
    static Q2App *s_instance;

    bool m_quitRequested;

    unsigned char m_screenPaletteData[1024];

    bool m_screenPaletteDirty;

    Urho3D::SharedPtr<Urho3D::Texture2D> m_screenPaletteTexture;

    Urho3D::Vector<unsigned char> m_screenBuffer;

    Urho3D::IntVector2 m_screenModeSize;

    bool m_screenModeFullscreen;

    bool m_screenModeDirty;

    Urho3D::SharedPtr<Urho3D::Texture2D> m_screenBufferTexture;

    Urho3D::SharedPtr<Urho3D::Scene> m_rttScene;

    Urho3D::SharedPtr<Urho3D::Scene> m_scene;
};

namespace Q2Util
{
    Urho3D::Texture2D *CreateScreenPaletteTexture(Urho3D::Context *context, const unsigned char *paletteData);

    Urho3D::Texture2D *CreateScreenBufferTexture(Urho3D::Context *context, int width, int height);

    Urho3D::Model *CreateScreenBufferModel(Urho3D::Context *context, int width, int height);

    int QuakeKeyForUrhoKey(int urhoKey);
}
