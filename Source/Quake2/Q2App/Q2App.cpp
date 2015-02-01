#include "Q2App.h"

#include "BufferedSoundStream.h"
#include "Camera.h"
#include "CoreEvents.h"
#include "Engine.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "Graphics.h"
#include "IndexBuffer.h"
#include "Input.h"
#include "InputEvents.h"
#include "Material.h"
#include "Model.h"
#include "Octree.h"
#include "Renderer.h"
#include "ResourceCache.h"
#include "Scene.h"
#include "StaticModel.h"
#include "Technique.h"
#include "Texture2D.h"
#include "UI.h"
#include "VertexBuffer.h"

extern "C"
{
#include "client/client.h"

    extern cvar_t *cl_maxfps;
}

#include "DebugNew.h"

Remotery *rmt;

DEFINE_APPLICATION_MAIN(Q2App);

Q2App* Q2App::s_instance;

Q2App::Q2App(Urho3D::Context* context)
: Urho3D::Application(context),
m_quitRequested(false),
m_screenPaletteDirty(false),
m_screenModeFullscreen(false),
m_screenModeDirty(false),
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

void Q2App::OnVidCheckChanges()
{
    if (m_screenModeDirty)
    {
        m_scene = NULL;
        m_rttScene = NULL;
        m_screenBufferTexture = NULL;
        
        CreateRenderScenes(m_screenModeSize.x_, m_screenModeSize.y_);

        m_screenModeDirty = false;
    }
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
    Qcommon_Init(q2Args.Size(), const_cast<char**>(&q2Args[0]));

    // Update engine parameters
    engineParameters_["WindowTitle"] = GetTypeName();
    engineParameters_["LogName"] = GetTypeName() + ".log";
    engineParameters_["Headless"] = false;
    engineParameters_["ResourcePaths"] = "Quake2Data;Data;CoreData";

    // if started with '-w', engineParameters_ has key "Fullscreen" with value 'false'
    // in which case, Urho can be windowed but Quake can think it is fullscreen (vid_fullscreen 1)
    // otherwise, defer to Quake completely (m_screenModeFullscreen)
    if (engineParameters_.Contains("FullScreen"))
    {
        const bool paramsFullscreen = engineParameters_["FullScreen"].GetBool();
        engineParameters_["FullScreen"] = paramsFullscreen && m_screenModeFullscreen;
    }
    else
    {
        engineParameters_["FullScreen"] = m_screenModeFullscreen;
    }

    // if Quake is windowed, use its screen dimensions exactly, otherwise Urho will stretch screen
    if (!m_screenModeFullscreen)
    {
        engineParameters_["WindowWidth"] = m_screenModeSize.x_;
        engineParameters_["WindowHeight"] = m_screenModeSize.y_;
    }
}

void Q2App::Start()
{
    Urho3D::Context* const context = GetContext();

    // Create screen palette texture
    m_screenPaletteTexture = Q2Util::CreateScreenPaletteTexture(context, m_screenPaletteData);

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

    // Shutdown Remotery
    rmt_DestroyGlobalInstance(rmt);
}

void Q2App::CreateRenderScenes(int width, int height)
{
    Urho3D::Context *const context = GetContext();
    Urho3D::Graphics *const graphics = GetSubsystem<Urho3D::Graphics>();
    Urho3D::ResourceCache *const resourceCache = GetSubsystem<Urho3D::ResourceCache>();

    // Create screen buffer texture
    m_screenBufferTexture = Q2Util::CreateScreenBufferTexture(context, width, height);

    // Screen buffer model
    Urho3D::SharedPtr<Urho3D::Model> screenModel(Q2Util::CreateScreenBufferModel(context, width, height));

    // RTT texture
    Urho3D::SharedPtr<Urho3D::Texture2D> renderTexture(new Urho3D::Texture2D(context));
    {
        renderTexture->SetSize(width, height, graphics->GetRGBFormat(), Urho3D::TEXTURE_RENDERTARGET);
        renderTexture->SetFilterMode(Urho3D::FILTER_BILINEAR);
    }

    // RTT scene
    m_rttScene = new Urho3D::Scene(context);
    {
        m_rttScene->CreateComponent<Urho3D::Octree>();

        Urho3D::Node* screenNode = m_rttScene->CreateChild();
        {
#ifdef URHO3D_OPENGL
            screenNode->SetPosition(Urho3D::Vector3(0.0f, 0.0f, 1.0f));
#else
            screenNode->SetPosition(Urho3D::Vector3(-0.5f, 0.5f, 1.0f));
#endif
            Urho3D::StaticModel* staticModel = screenNode->CreateComponent<Urho3D::StaticModel>();
            {
                staticModel->SetModel(screenModel);

                Urho3D::SharedPtr<Urho3D::Material> material(new Urho3D::Material(context));
                {
                    material->SetTechnique(0, resourceCache->GetResource<Urho3D::Technique>("Techniques/ConvertPalette.xml"));
                    material->SetTexture(Urho3D::TU_DIFFUSE, m_screenBufferTexture);
                    material->SetTexture(Urho3D::TU_EMISSIVE, m_screenPaletteTexture);
                }

                staticModel->SetMaterial(material);
            }
        }

        Urho3D::Node* cameraNode = m_rttScene->CreateChild();
        {
            Urho3D::Camera* camera = cameraNode->CreateComponent<Urho3D::Camera>();
            {
                camera->SetOrthographic(true);
                camera->SetOrthoSize(Urho3D::Vector2(width, height));
            }

            Urho3D::SharedPtr<Urho3D::Viewport> viewport(new Urho3D::Viewport(context, m_rttScene, camera));
            renderTexture->GetRenderSurface()->SetViewport(0, viewport);
        }
    }

    // Main scene
    m_scene = new Urho3D::Scene(context);
    {
        m_scene->CreateComponent<Urho3D::Octree>();

        Urho3D::Node* screenNode = m_scene->CreateChild();
        {
#ifdef URHO3D_OPENGL
            screenNode->SetPosition(Urho3D::Vector3(0.0f, 0.0f, 1.0f));
#else
            const float xOffset = 0.5f * m_screenModeSize.x_ / graphics->GetWidth();
            const float yOffset = 0.5f * m_screenModeSize.y_ / graphics->GetHeight();
            screenNode->SetPosition(Urho3D::Vector3(-xOffset, yOffset, 1.0f));
#endif
            Urho3D::StaticModel* staticModel = screenNode->CreateComponent<Urho3D::StaticModel>();
            {
                staticModel->SetModel(screenModel);

                Urho3D::SharedPtr<Urho3D::Material> material(new Urho3D::Material(context));
                {
                    material->SetTechnique(0, resourceCache->GetResource<Urho3D::Technique>("Techniques/DiffUnlit.xml"));
                    material->SetTexture(Urho3D::TU_DIFFUSE, renderTexture);
                }

                staticModel->SetMaterial(material);
            }
        }

        Urho3D::Node* cameraNode = m_scene->CreateChild();
        {
            Urho3D::Camera* camera = cameraNode->CreateComponent<Urho3D::Camera>();
            {
                camera->SetOrthographic(true);
                camera->SetOrthoSize(height);
            }

            Urho3D::SharedPtr<Urho3D::Viewport> viewport(new Urho3D::Viewport(context, m_scene, camera));
            GetSubsystem<Urho3D::Renderer>()->SetViewport(0, viewport);
        }
    }
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

    // Update screen palette texture
    if (m_screenPaletteDirty)
    {
        m_screenPaletteTexture->SetData(0, 0, 0, 256, 1, m_screenPaletteData);
        m_screenPaletteDirty = false;
    }

    // Update screen buffer texture
    if (m_screenBufferTexture)
        m_screenBufferTexture->SetData(0, 0, 0, m_screenModeSize.x_, m_screenModeSize.y_, &m_screenBuffer[0]);
}

void Q2App::HandleExitRequested(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
    m_quitRequested = true;
}

Urho3D::Texture2D *Q2Util::CreateScreenPaletteTexture(Urho3D::Context* context, const unsigned char *paletteData)
{
    Urho3D::Graphics *const graphics = context->GetSubsystem<Urho3D::Graphics>();

    Urho3D::Texture2D *const texture = new Urho3D::Texture2D(context);
    {
        texture->SetNumLevels(1);
        texture->SetSize(256, 1, graphics->GetRGBAFormat(), Urho3D::TEXTURE_DYNAMIC);
        texture->SetFilterMode(Urho3D::FILTER_NEAREST);
        texture->SetAddressMode(Urho3D::COORD_U, Urho3D::ADDRESS_CLAMP);
        texture->SetAddressMode(Urho3D::COORD_V, Urho3D::ADDRESS_CLAMP);
        texture->SetData(0, 0, 0, 256, 1, paletteData);
    }

    return texture;
}

Urho3D::Texture2D *Q2Util::CreateScreenBufferTexture(Urho3D::Context* context, int width, int height)
{
    Urho3D::Graphics *const graphics = context->GetSubsystem<Urho3D::Graphics>();

    Urho3D::Texture2D *const texture = new Urho3D::Texture2D(context);
    {
        texture->SetNumLevels(1);
        texture->SetSize(width, height, graphics->GetAlphaFormat(), Urho3D::TEXTURE_DYNAMIC);
        texture->SetAddressMode(Urho3D::COORD_U, Urho3D::ADDRESS_CLAMP);
        texture->SetAddressMode(Urho3D::COORD_V, Urho3D::ADDRESS_CLAMP);
        texture->SetFilterMode(Urho3D::FILTER_NEAREST);
    }

    return texture;
}

Urho3D::Model *Q2Util::CreateScreenBufferModel(Urho3D::Context *context, int width, int height)
{
    Urho3D::Model *const screenModel = new Urho3D::Model(context);
    {
        Urho3D::SharedPtr<Urho3D::Geometry> geom(new Urho3D::Geometry(context));
        {
            const int numVertices = 4;
            Urho3D::SharedPtr<Urho3D::VertexBuffer> vb(new Urho3D::VertexBuffer(context));
            {
                const float vertexData[] =
                {
                    -width * 0.5f, height * 0.5f, 0.0f, 0.0f, 0.0f,
                    width * 0.5f, height * 0.5f, 0.0f, 1.0f, 0.0f,
                    -width * 0.5f, -height * 0.5f, 0.0f, 0.0f, 1.0f,
                    width * 0.5f, -height * 0.5f, 0.0f, 1.0f, 1.0f,
                };

                vb->SetShadowed(true);
                vb->SetSize(numVertices, Urho3D::MASK_POSITION | Urho3D::MASK_TEXCOORD1);
                vb->SetData(vertexData);
            }

            const int numIndices = 6;
            Urho3D::SharedPtr<Urho3D::IndexBuffer> ib(new Urho3D::IndexBuffer(context));
            {
                const unsigned short indexData[] =
                {
                    0, 1, 2,
                    2, 1, 3,
                };

                ib->SetShadowed(true);
                ib->SetSize(numIndices, false);
                ib->SetData(indexData);
            }

            geom->SetVertexBuffer(0, vb);
            geom->SetIndexBuffer(ib);
            geom->SetDrawRange(Urho3D::TRIANGLE_LIST, 0, numIndices); //<todo.cb TRIANGLE_STRIP?
        }

        screenModel->SetNumGeometries(1);
        screenModel->SetGeometry(0, 0, geom);

        const Urho3D::Vector3 bbMin(-width * 0.5f, -height * 0.5f, 0.0f);
        const Urho3D::Vector3 bbMax(width * 0.5f, height * 0.5f, 0.0f);
        screenModel->SetBoundingBox(Urho3D::BoundingBox(bbMin, bbMax));
    }

    return screenModel;
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
