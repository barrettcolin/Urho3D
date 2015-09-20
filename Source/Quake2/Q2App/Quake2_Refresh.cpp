#include "Urho3D/Urho3D.h"

#include "Quake2_Refresh.h"

#include "Urho3D/DebugNew.h"

namespace
{
    struct vidmode_t
    {
        const char *description;
        int width, height, mode;
    };

    vidmode_t s_vid_modes[] =
    {
        { "Mode 0: 320x240", 320, 240, 0 },
        { "Mode 1: 400x300", 400, 300, 1 },
        { "Mode 2: 512x384", 512, 384, 2 },
        { "Mode 3: 640x480", 640, 480, 3 },
        { "Mode 4: 800x600", 800, 600, 4 },
        { "Mode 5: 960x720", 960, 720, 5 },
        { "Mode 6: 1024x768", 1024, 768, 6 },
        { "Mode 7: 1152x864", 1152, 864, 7 },
        { "Mode 8: 1280x960", 1280, 960, 8 },
        { "Mode 9: 1600x1200", 1600, 1200, 9 },
    };

    const int VID_NUM_MODES = sizeof(s_vid_modes) / sizeof(s_vid_modes[0]);
}

Quake2_Refresh::Quake2_Refresh()
    : m_screenModeFullscreen(false),
    m_screenModeDirty(false)
{
}

void Quake2_Refresh::Init(Urho3D::Context *context)
{
	m_context = context;
}

bool Quake2_Refresh::SetMode(int& width, int& height, int mode, bool fullscreen)
{
    if (mode >= 0 && mode < VID_NUM_MODES)
    {
        const vidmode_t& vidMode = s_vid_modes[mode];
        const bool ret = OnSetMode(vidMode.width, vidMode.height, fullscreen);
        if (ret)
        {
            width = vidMode.width;
            height = vidMode.height;

            m_screenModeSize = Urho3D::IntVector2(width, height);
            m_screenModeFullscreen = fullscreen;
            m_screenModeDirty = true;
        }

        return ret;
    }

    return false;
}
