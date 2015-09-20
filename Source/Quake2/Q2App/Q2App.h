#pragma once

#include "Urho3D/Engine/Application.h"
#include "Urho3D/Core/Mutex.h"
#include "Urho3D/Audio/SoundStream.h"
#include "Urho3D/Core/Timer.h"

namespace Urho3D
{
    class Node;
}

// Q2SoundDMA
class Q2SoundStream : public Urho3D::SoundStream
{
public:
    Q2SoundStream();

    void Init(unsigned bufferSizeInBytes);

    void Shutdown();

    int GetCurrentSample() const;

    // Urho3D::SoundStream
    virtual unsigned GetData(signed char* dest, unsigned numBytes) override;

    mutable Urho3D::Mutex m_bufferMutex;

    Urho3D::Vector<unsigned char> m_dmaBuffer;

    int m_currentSample;
};

// Q2Input
typedef struct usercmd_s usercmd_t;

class Q2Input : public Urho3D::Object
{
    OBJECT(Q2Input);

public:
    Q2Input(Urho3D::Context *context);

    void Update();

    void AddCommands();

    void Move(usercmd_t& cmd);
};

class Quake2_Refresh;

class Q2App : public Urho3D::Application
{
    OBJECT(Q2App);

public:
    static Q2App& GetInstance() { return *s_instance; }

    Q2App(Urho3D::Context* context);

    void OnSysInit();

    void OnSysQuit();

    void OnSNDDMAInit();

    void OnSNDDMAShutdown();

    int OnSNDDMAGetDMAPos();

    void OnSNDDMABeginPainting();

    void OnSNDDMASubmit();

    Q2Input& GetInput() { return *m_input; }

    // Application
    virtual void Setup() override;

    virtual void Start() override;

    virtual void Stop() override;

protected:
    void HandleKeyDown(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

    void HandleKeyUp(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

    void HandleMouseButtonDown(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

    void HandleMouseButtonUp(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

    void HandleUpdate(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

    void HandleExitRequested(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);

private:
    static Q2App *s_instance;

    bool m_quitRequested;

    Urho3D::SharedPtr<Urho3D::Node> m_soundNode;

    Urho3D::SharedPtr<Q2SoundStream> m_soundStream;

    Urho3D::SharedPtr<Q2Input> m_input;
};

namespace Q2Util
{
    int QuakeKeyForUrhoKey(int urhoKey);

    int QuakeMouseButtonForUrhoMouseButton(int urhoMouseButton);
}
