#include "Urho3D/Urho3D.h"

#include "Q2App.h"

#include "Urho3D/Core/CoreEvents.h"
#include "Urho3D/Engine/Engine.h"
#include "Urho3D/Input/Input.h"
#include "Urho3D/Input/InputEvents.h"
#include "Urho3D/IO/FileSystem.h"
#include "Urho3D/Resource/ResourceCache.h"
#include "Urho3D/Scene/Node.h"
#include "Urho3D/UI/UI.h"

extern "C"
{
#include "client/client.h"
}

#include "Quake2_Refresh.h"

#include "Urho3D/DebugNew.h"

extern "C" cvar_t *cl_maxfps;
extern "C" cvar_t *vid_fullscreen;
extern viddef_t viddef;

Remotery *rmt;

DEFINE_APPLICATION_MAIN(Q2App);

Q2App* Q2App::s_instance;

Q2App::Q2App(Urho3D::Context* context)
: Urho3D::Application(context),
m_quitRequested(false),
m_input(new Q2Input(context))
{
}

void Q2App::OnSysInit()
{
}

void Q2App::OnSysQuit()
{
    m_quitRequested = true;
}

void Q2App::Setup()
{
    s_instance = this;

    // Init Remotery
    if (rmt_CreateGlobalInstance(&rmt) != RMT_ERROR_NONE)
    {
        Com_Printf("Remotery failed to initialize\n");
    }

    rmt_SetCurrentThreadName("Q2App Main");

    // Read Quake 2 command line from file
    Urho3D::Vector<const char*> q2Args(1);
    q2Args[0] = NULL;
    {
        Urho3D::FileSystem *const fileSystem = GetContext()->GetSubsystem<Urho3D::FileSystem>();
        const Urho3D::String cmdFileName = fileSystem->GetProgramDir() + "Quake2Data/CommandLine.txt";

        Urho3D::SharedPtr<Urho3D::File> cmdFile(new Urho3D::File(GetContext()));
        if (cmdFile->Open(cmdFileName, Urho3D::FILE_READ))
        {
            Urho3D::String cmdLine = cmdFile->ReadLine();
            cmdFile->Close();

            // Parse (overwrites Urho3D arguments)
            Urho3D::ParseArguments(cmdLine, false);

            q2Args.Reserve(Urho3D::GetArguments().Size() + 1);
            {
                for (int i = 0; i < Urho3D::GetArguments().Size(); ++i)
                    q2Args.Push(Urho3D::GetArguments()[i].CString());
            }
        }
    }

    // Init Quake 2
    g_refresh->Init(GetContext());

    Qcommon_Init(q2Args.Size(), const_cast<char**>(&q2Args[0]));

    // Update engine parameters
    engineParameters_["WindowTitle"] = GetTypeName();
    engineParameters_["LogName"] = GetTypeName() + ".log";
    engineParameters_["Headless"] = false;
    engineParameters_["ResourcePaths"] = "Quake2Data;Data;CoreData";

    // if started with '-w', engineParameters_ has key "Fullscreen" with value 'false'
    // in which case, Urho can be windowed but Quake can think it is fullscreen (vid_fullscreen 1)
    // otherwise, defer to Quake completely (m_screenModeFullscreen)
    const bool screenModeFullscreen = (vid_fullscreen->value != 0);
    if (engineParameters_.Contains("FullScreen"))
    {
        const bool paramsFullscreen = engineParameters_["FullScreen"].GetBool();
        engineParameters_["FullScreen"] = paramsFullscreen && screenModeFullscreen;
    }
    else
    {
        engineParameters_["FullScreen"] = screenModeFullscreen;
    }

    // if Quake is windowed, use its screen dimensions exactly, otherwise Urho will stretch screen
    if (!screenModeFullscreen)
    {
        engineParameters_["WindowWidth"] = viddef.width;
        engineParameters_["WindowHeight"] = viddef.height;
    }
}

void Q2App::Start()
{
    Urho3D::Context* const context = GetContext();

    // Start refresh
    g_refresh->OnStart();

    SubscribeToEvent(Urho3D::E_KEYDOWN, HANDLER(Q2App, HandleKeyDown));
    SubscribeToEvent(Urho3D::E_KEYUP, HANDLER(Q2App, HandleKeyUp));

    SubscribeToEvent(Urho3D::E_MOUSEBUTTONDOWN, HANDLER(Q2App, HandleMouseButtonDown));
    SubscribeToEvent(Urho3D::E_MOUSEBUTTONUP, HANDLER(Q2App, HandleMouseButtonUp));

    SubscribeToEvent(Urho3D::E_UPDATE, HANDLER(Q2App, HandleUpdate));

    SubscribeToEvent(Urho3D::E_EXITREQUESTED, HANDLER(Q2App, HandleExitRequested));
}

void Q2App::Stop()
{
    Com_Quit();

    Qcommon_Shutdown();

    // Stop refresh
    g_refresh->OnStop();

    // Shutdown Remotery
    rmt_DestroyGlobalInstance(rmt);
}

void Q2App::HandleKeyDown(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
    const int key = eventData[Urho3D::KeyDown::P_KEY].GetInt();
    const unsigned time = GetSubsystem<Urho3D::Time>()->GetSystemTime();
    const int qkey = Q2Util::QuakeKeyForUrhoKey(key);

    Key_Event(qkey, qtrue, time);
}

void Q2App::HandleKeyUp(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
    const int key = eventData[Urho3D::KeyDown::P_KEY].GetInt();
    const unsigned time = GetSubsystem<Urho3D::Time>()->GetSystemTime();
    const int qkey = Q2Util::QuakeKeyForUrhoKey(key);

    Key_Event(qkey, qfalse, time);
}

void Q2App::HandleMouseButtonDown(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
    const int button = eventData[Urho3D::MouseButtonDown::P_BUTTON].GetInt();
    const unsigned time = GetSubsystem<Urho3D::Time>()->GetSystemTime();

    const int qbutton = Q2Util::QuakeMouseButtonForUrhoMouseButton(button);
    if (qbutton)
    {
        Key_Event(qbutton, qtrue, time);
    }
}

void Q2App::HandleMouseButtonUp(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
    const int button = eventData[Urho3D::MouseButtonDown::P_BUTTON].GetInt();
    const unsigned time = GetSubsystem<Urho3D::Time>()->GetSystemTime();

    const int qbutton = Q2Util::QuakeMouseButtonForUrhoMouseButton(button);
    if (qbutton)
    {
        Key_Event(qbutton, qfalse, time);
    }
}

void Q2App::HandleUpdate(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
    if (m_quitRequested)
    {
        engine_->Exit();
        return;
    }

    // Update Max FPS
    {
        Urho3D::Engine* const engine = GetSubsystem<Urho3D::Engine>();

        const int targetFps = Urho3D::Min(static_cast<int>(cl_maxfps->value), engine->GetMaxFps());
        engine->SetMaxFps(targetFps);
    }

    //<todo.cb Sys_Milliseconds updates curtime; necessary for cinematics at least
    Sys_Milliseconds();

    const float msec = ceilf(eventData[Urho3D::Update::P_TIMESTEP].GetFloat() * 1000.0f);
    rmt_BeginCPUSample(Qcommon_Frame);
    Qcommon_Frame(static_cast<int>(msec));
    rmt_EndCPUSample();
}

void Q2App::HandleExitRequested(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
    m_quitRequested = true;
}

int Q2Util::QuakeKeyForUrhoKey(int key)
{
    switch (key)
    {
    case Urho3D::KEY_TAB: return K_TAB;
    case Urho3D::KEY_RETURN: return K_ENTER;
    case Urho3D::KEY_ESC: return K_ESCAPE;
    case Urho3D::KEY_SPACE: return K_SPACE;

    case Urho3D::KEY_BACKSPACE: return K_BACKSPACE;
    case Urho3D::KEY_UP: return K_UPARROW;
    case Urho3D::KEY_DOWN: return K_DOWNARROW;
    case Urho3D::KEY_LEFT: return K_LEFTARROW;
    case Urho3D::KEY_RIGHT: return K_RIGHTARROW;

    case Urho3D::KEY_ALT:
    case Urho3D::KEY_RALT: return K_ALT;
    case Urho3D::KEY_CTRL:
    case Urho3D::KEY_RCTRL: return K_CTRL;
    case Urho3D::KEY_SHIFT:
    case Urho3D::KEY_RSHIFT: return K_SHIFT;

    case Urho3D::KEY_F1:
    case Urho3D::KEY_F2:
    case Urho3D::KEY_F3:
    case Urho3D::KEY_F4:
    case Urho3D::KEY_F5:
    case Urho3D::KEY_F6:
    case Urho3D::KEY_F7:
    case Urho3D::KEY_F8:
    case Urho3D::KEY_F9:
    case Urho3D::KEY_F10:
    case Urho3D::KEY_F11: return (key - Urho3D::KEY_F1) + K_F1;

    case Urho3D::KEY_INSERT: return K_INS;
    case Urho3D::KEY_DELETE: return K_DEL;
    case Urho3D::KEY_PAGEUP: return K_PGUP;
    case Urho3D::KEY_PAGEDOWN: return K_PGDN;
    case Urho3D::KEY_HOME: return K_HOME;
    case Urho3D::KEY_END: return K_END;

        //< SDL keys not exposed in Urho
    case SDLK_BACKQUOTE: return '~';
    case SDLK_MINUS: return '-';
    case SDLK_LEFTBRACKET: return '[';
    case SDLK_RIGHTBRACKET: return ']';
    case SDLK_SEMICOLON: return ';';
    case SDLK_COMMA: return ',';
    case SDLK_PERIOD: return '.';

    case Urho3D::KEY_0:
    case Urho3D::KEY_1:
    case Urho3D::KEY_2:
    case Urho3D::KEY_3:
    case Urho3D::KEY_4:
    case Urho3D::KEY_5:
    case Urho3D::KEY_6:
    case Urho3D::KEY_7:
    case Urho3D::KEY_8:
    case Urho3D::KEY_9: return (key - Urho3D::KEY_0) + '0';

    default: return (key - Urho3D::KEY_A) + 'a';
    }
}

int Q2Util::QuakeMouseButtonForUrhoMouseButton(int urhoMouseButton)
{
    switch (urhoMouseButton)
    {
    case Urho3D::MOUSEB_LEFT: return K_MOUSE1;
    case Urho3D::MOUSEB_RIGHT: return K_MOUSE2;
    case Urho3D::MOUSEB_MIDDLE: return K_MOUSE3;
    default: return 0;
    }
}
