#pragma once

#include "Application.h"
#include "Mutex.h"
#include "SoundStream.h"
#include "Timer.h"

namespace Urho3D
{
    class Model;
    class Node;
    class Scene;
    class Texture2D;
}

class Q2SoundStream : public Urho3D::SoundStream
{
public:
    Q2SoundStream();

    void Init(unsigned bufferSizeInBytes);

    void Shutdown();

    int GetSoundBufferPos() const;

    // Urho3D::SoundStream
    virtual unsigned GetData(signed char* dest, unsigned numBytes) override;

    mutable Urho3D::Mutex m_bufferMutex;

    Urho3D::Vector<unsigned char> m_soundBuffer;

    int m_soundBufferPos;
};

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

    void OnSNDDMAInit();

    void OnSNDDMAShutdown();

    int OnSNDDMAGetDMAPos();

    void OnSNDDMASubmit();

    // Application
    virtual void Setup() override;

    virtual void Start() override;

    virtual void Stop() override;

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

    Urho3D::SharedPtr<Urho3D::Node> m_soundNode;

    Urho3D::SharedPtr<Q2SoundStream> m_soundStream;
};

namespace Q2Util
{
    Urho3D::Texture2D *CreateScreenPaletteTexture(Urho3D::Context *context, const unsigned char *paletteData);

    Urho3D::Texture2D *CreateScreenBufferTexture(Urho3D::Context *context, int width, int height);

    Urho3D::Model *CreateScreenBufferModel(Urho3D::Context *context, int width, int height);

    int QuakeKeyForUrhoKey(int urhoKey);
}
