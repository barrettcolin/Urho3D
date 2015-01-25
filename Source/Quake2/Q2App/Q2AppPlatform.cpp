#include "Q2App.h"
#include "Graphics.h"
#include "Input.h"

extern "C"
{
#include "client/client.h"
#include "game/game.h"
}

#include "DebugNew.h"

extern "C"
{
    // System
    qboolean stdin_active; //<todo.cb Linux
    unsigned sys_frame_time; //<todo.cb required for input (held keys)

    // Video
    refexport_t GetRefAPI(refimport_t ri);

    // Input
    cvar_t *in_joystick;
}

// System
void Sys_Init()
{
    Q2App::GetInstance().OnSysInit();
}

void Sys_Quit()
{
    Q2App::GetInstance().OnSysQuit();
}

void Sys_Error(char *error, ...) {}

//<todo.cb game.h has GetGameApi, not GetGameAPI
extern "C" game_export_t *GetGameAPI(game_import_t *import);

void *Sys_GetGameAPI(void *parms)
{
    return GetGameAPI(static_cast<game_import_t*>(parms));
}

void Sys_UnloadGame() {}
char *Sys_ConsoleInput() { return NULL; }
void Sys_ConsoleOutput(char *string) {}

void Sys_SendKeyEvents()
{
    sys_frame_time = Q2App::GetInstance().GetSubsystem<Urho3D::Time>()->GetSystemTime();
}

void Sys_AppActivate() {}
void Sys_CopyProtect() {}
char *Sys_GetClipboardData() { return NULL; }

// Video
refexport_t re;
viddef_t viddef;

void VID_Printf(int print_level, char *fmt, ...) {}
void VID_Error(int err_level, char *fmt, ...) {}
qboolean VID_GetModeInfo(int *width, int *height, int mode) { return qfalse; }

void VID_NewWindow(int width, int height)
{
    viddef.width = width;
    viddef.height = height;

    cl.force_refdef = qtrue;
}

void VID_Init()
{
    refimport_t ri;
    {
        ri.Cmd_AddCommand = Cmd_AddCommand;
        ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
        ri.Cmd_Argc = Cmd_Argc;
        ri.Cmd_Argv = Cmd_Argv;
        ri.Cmd_ExecuteText = Cbuf_ExecuteText;
        ri.Con_Printf = VID_Printf;
        ri.Sys_Error = VID_Error;
        ri.FS_LoadFile = FS_LoadFile;
        ri.FS_FreeFile = FS_FreeFile;
        ri.FS_Gamedir = FS_Gamedir;
        ri.Cvar_Get = Cvar_Get;
        ri.Cvar_Set = Cvar_Set;
        ri.Cvar_SetValue = Cvar_SetValue;
        ri.Vid_GetModeInfo = VID_GetModeInfo;
        ri.Vid_MenuInit = VID_MenuInit;
        ri.Vid_NewWindow = VID_NewWindow;
    }

    re = GetRefAPI(ri);

    if (re.Init(NULL, NULL) == qfalse)
    {
        re.Shutdown();
    }
}

void VID_Shutdown()
{
    re.Shutdown();
}

void VID_CheckChanges()
{
    Q2App::GetInstance().OnVidCheckChanges();
}

void VID_MenuInit() {}
void VID_MenuDraw() {}
const char *VID_MenuKey(int key) { return NULL; }

void IN_Init()
{
    // pass
}

void IN_Shutdown()
{
    // pass
}

void IN_Frame()
{
    Q2App::GetInstance().GetInput().Update();
}

void IN_Commands()
{
    Q2App::GetInstance().GetInput().AddCommands();
}

void IN_Move(usercmd_t *cmd)
{
    Q2App::GetInstance().GetInput().Move(*cmd);
}

Q2Input::Q2Input(Urho3D::Context *context)
    : Urho3D::Object(context)
{

}

void Q2Input::Update()
{
    Urho3D::Input *const input = GetSubsystem<Urho3D::Input>();

    // Update mouse mode
    const Urho3D::MouseMode mouseMode = input->GetMouseMode();
    const bool showMouse = (cl.refresh_prepped == qfalse) || (cls.key_dest == key_console || cls.key_dest == key_menu);
    if (showMouse)
    {
        input->SetMouseMode(Urho3D::MM_ABSOLUTE);
        if (!GetSubsystem<Urho3D::Graphics>()->GetFullscreen())
        {
            input->SetMouseVisible(true);
        }
    }
    else
    {
        input->SetMouseMode(Urho3D::MM_WRAP);
        input->SetMouseVisible(false);
    }
}

void Q2Input::AddCommands()
{
    // Joystick goes here
}

void Q2Input::Move(usercmd_t& cmd)
{
    LOGINFO("Q2Input::Move");

    Urho3D::Input *const input = GetSubsystem<Urho3D::Input>();

    // mouseMode == MM_WRAP when game has input focus (not console, not menu)
    const Urho3D::MouseMode mouseMode = input->GetMouseMode();
    if (mouseMode == Urho3D::MM_WRAP)
    {
        const Urho3D::IntVector2& mouseMove = input->GetMouseMove();

        const float yawDelta = sensitivity->value * mouseMove.x_ * m_yaw->value;
        cl.viewangles[YAW] -= yawDelta;

        const float pitchDelta = sensitivity->value * mouseMove.y_ * m_pitch->value;
        cl.viewangles[PITCH] += pitchDelta;
    }
}
