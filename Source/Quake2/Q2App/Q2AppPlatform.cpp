#include "Q2App.h"

extern "C"
{
#include "client/client.h"
#include "game/game.h"
}

#include "DebugNew.h"

extern "C"
{
    // System
    unsigned sys_frame_time;
    qboolean stdin_active; //<todo.cb Linux

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
void Sys_SendKeyEvents() {}
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

void IN_Init() {}
void IN_Shutdown() {}
void IN_Commands() {}
void IN_Frame() {}
void IN_Move(usercmd_t *cmd) {}
void IN_Activate(qboolean active) {}
void IN_ActivateMouse() {}
void IN_DeactivateMouse() {}
