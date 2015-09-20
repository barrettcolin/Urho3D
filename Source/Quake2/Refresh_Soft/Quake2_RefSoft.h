#pragma once

#include "Quake2_Refresh.h"

#include "Urho3D/Container/Ptr.h"
#include "Urho3D/Container/Vector.h"

namespace Urho3D
{
    class Scene;
    class Texture2D;
}

class Quake2_RefSoft : public Quake2_Refresh
{
public:
    Quake2_RefSoft();

    void SetPalette(const unsigned char *palette);

    void CreateRenderScenes(int width, int height);

    void EndFrame();

protected:
    // Quake2_Refresh interface
    virtual void OnStart() override;

    virtual void OnStop() override;

    virtual bool OnSetMode(int width, int height, bool fullscreen) override;

    virtual void OnVidCheckChanges() override;

protected:
    unsigned char m_screenPaletteData[1024];
    bool m_screenPaletteDirty;
    Urho3D::SharedPtr<Urho3D::Texture2D> m_screenPaletteTexture;

    Urho3D::Vector<unsigned char> m_screenBuffer;
    Urho3D::SharedPtr<Urho3D::Texture2D> m_screenBufferTexture;

    Urho3D::SharedPtr<Urho3D::Scene> m_rttScene;

    Urho3D::SharedPtr<Urho3D::Scene> m_scene;
};

extern Quake2_RefSoft g_refLocal;
