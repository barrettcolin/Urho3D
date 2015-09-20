#pragma once

#include "Urho3D/Math/Vector2.h"

namespace Urho3D
{
	class Context;
}

class Quake2_Refresh
{
public:
    Quake2_Refresh();

	void Init(Urho3D::Context *context);
	
    bool SetMode(int& width, int& height, int mode, bool fullscreen);

    virtual void OnStart() = 0;

    virtual void OnStop() = 0;

    virtual void OnVidCheckChanges() = 0;
	
protected:
    virtual bool OnSetMode(int width, int height, bool fullscreen) = 0;

protected:
	Urho3D::Context *m_context;

    Urho3D::IntVector2 m_screenModeSize;
    bool m_screenModeFullscreen;
    bool m_screenModeDirty;
};

extern Quake2_Refresh *g_refresh;
