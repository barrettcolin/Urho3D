#include "Q2App.h"

#include "Camera.h"
#include "CoreEvents.h"
#include "Engine.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "Graphics.h"
#include "IndexBuffer.h"
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
}

#include "DebugNew.h"

DEFINE_APPLICATION_MAIN(Q2App);

Q2App* Q2App::s_instance;

Q2App::Q2App(Urho3D::Context* context)
: Urho3D::Application(context),
m_quitRequested(false),
m_screenPaletteDirty(false),
m_screenModeFullscreen(false),
m_screenModeDirty(false)
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

    const char *argv[] = {
        "Quake2",
        //"+set", "logfile", "1",
        //"+set", "developer", "1",
        //"+set", "sw_mode", "1",
        //"+set", "vid_fullscreen", "0",
        //"+set", "s_initsound", "0",
        //"+gamemap", "q2dm5",
    };
    const int argc = sizeof(argv) / sizeof(argv[0]);

    Qcommon_Init(argc, const_cast<char**>(argv));

    engineParameters_["WindowTitle"] = GetTypeName();
    engineParameters_["LogName"] = GetTypeName() + ".log";
    engineParameters_["FullScreen"] = engineParameters_["FullScreen"].GetBool() && m_screenModeFullscreen;
    engineParameters_["Headless"] = false;
    engineParameters_["ResourcePaths"] = "Quake2Data;CoreData";

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

    SubscribeToEvent(Urho3D::E_UPDATE, HANDLER(Q2App, HandleUpdate));

    SubscribeToEvent(Urho3D::E_EXITREQUESTED, HANDLER(Q2App, HandleExitRequested));
}

void Q2App::Stop()
{
    Com_Quit();
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
#ifdef URHO3D_OPENGL
            cameraNode->SetPosition(Urho3D::Vector3(0.0f, 0.0f, 0.0f));
#else
            cameraNode->SetPosition(Urho3D::Vector3(0.5f, 0.0f, -0.5f));
#endif

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
#ifdef URHO3D_OPENGL
            cameraNode->SetPosition(Urho3D::Vector3(0.0f, 0.0f, 0.0f));
#else
            cameraNode->SetPosition(Urho3D::Vector3(0.5f, 0.0f, -0.5f));
#endif

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

void Q2App::HandleUpdate(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
{
    if (m_quitRequested)
    {
        engine_->Exit();
        return;
    }

    //<todo.cb Sys_Milliseconds updates curtime; necessary for cinematics at least
    Sys_Milliseconds();

    const float msec = eventData[Urho3D::Update::P_TIMESTEP].GetFloat() * 1000.0f;
    Qcommon_Frame(static_cast<int>(msec));

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
