#include "Q2App.h"

extern "C"
{
#include "ref_soft/r_local.h"
}

#include "DebugNew.h"

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

bool Q2App::OnRefSetMode(int& width, int& height, int mode, bool fullscreen)
{
    if (mode < 0 || mode > VID_NUM_MODES)
    {
        m_screenBuffer.Resize(0);

        vid.buffer = NULL;
        vid.rowbytes = 0;

        return false;
    }

    width = s_vid_modes[mode].width;
    height = s_vid_modes[mode].height;

    m_screenBuffer.Resize(width * height);

    vid.buffer = &m_screenBuffer[0];
    vid.rowbytes = width;

    const char *fullscreenString = fullscreen ? "fullscreen" : "windowed";

    m_screenModeSize = Urho3D::IntVector2(width, height);
    m_screenModeFullscreen = fullscreen;
    m_screenModeDirty = true;

    return true;
}

void Q2App::OnRefSetPalette(unsigned char const* palette)
{
    std::memcpy(m_screenPaletteData, palette, sizeof(m_screenPaletteData));
    m_screenPaletteDirty = true;
}

// Software renderer
int SWimp_Init(void *hInstance, void *wndProc) { return 0; }
void SWimp_Shutdown() {}
void SWimp_BeginFrame(float camera_separation) {}
void SWimp_EndFrame() {}
void SWimp_AppActivate(qboolean active) {}

rserr_t SWimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
    ri.Con_Printf(PRINT_ALL, "setting mode %d:", mode);

    if (!Q2App::GetInstance().OnRefSetMode(*pwidth, *pheight, mode, fullscreen == qtrue))
    {
        ri.Con_Printf(PRINT_ALL, " invalid mode\n");

        return rserr_invalid_mode;
    }

    ri.Vid_NewWindow(*pwidth, *pheight);

    ri.Con_Printf(PRINT_ALL, " %d %d %s\n", *pwidth, *pheight, fullscreen == qtrue ? "fullscreen" : "windowed");

    R_GammaCorrectAndSetPalette((const unsigned char *)d_8to24table);

    return rserr_ok;
}

void SWimp_SetPalette(const unsigned char *palette)
{
    Q2App::GetInstance().OnRefSetPalette(palette);
}
