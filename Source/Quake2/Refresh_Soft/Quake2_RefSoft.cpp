#include "Urho3D/Urho3D.h"

#include "Quake2_RefSoft.h"

#include "Urho3D/Core/Context.h"
#include "Urho3D/Graphics/Camera.h"
#include "Urho3D/Graphics/Graphics.h"
#include "Urho3D/Graphics/Geometry.h"
#include "Urho3D/Graphics/IndexBuffer.h"
#include "Urho3D/Graphics/Material.h"
#include "Urho3D/Graphics/Model.h"
#include "Urho3D/Graphics/Octree.h"
#include "Urho3D/Graphics/Renderer.h"
#include "Urho3D/Graphics/StaticModel.h"
#include "Urho3D/Graphics/Technique.h"
#include "Urho3D/Graphics/Texture2D.h"
#include "Urho3D/Graphics/VertexBuffer.h"
#include "Urho3D/Resource/ResourceCache.h"
#include "Urho3D/Scene/Scene.h"

extern "C"
{
#include "ref_soft/r_local.h"
}

#include "Urho3D/DebugNew.h"

Quake2_RefSoft g_refLocal;
Quake2_Refresh* g_refresh = &g_refLocal;

namespace
{
    Urho3D::Texture2D *CreateScreenPaletteTexture(Urho3D::Context* context, const unsigned char *paletteData)
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

    Urho3D::Texture2D *CreateScreenBufferTexture(Urho3D::Context* context, int width, int height)
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

    Urho3D::Model *CreateScreenBufferModel(Urho3D::Context *context, int width, int height)
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
}

Quake2_RefSoft::Quake2_RefSoft()
    : m_screenPaletteDirty(false)
{
}

void Quake2_RefSoft::SetPalette(const unsigned char *palette)
{
    std::memcpy(m_screenPaletteData, palette, sizeof(m_screenPaletteData));
    m_screenPaletteDirty = true;
}

void Quake2_RefSoft::CreateRenderScenes(int width, int height)
{
    Urho3D::Graphics *const graphics = m_context->GetSubsystem<Urho3D::Graphics>();
    Urho3D::ResourceCache *const resourceCache = m_context->GetSubsystem<Urho3D::ResourceCache>();

    // Create screen buffer texture
    m_screenBufferTexture = CreateScreenBufferTexture(m_context, width, height);

    // Screen buffer model
    Urho3D::SharedPtr<Urho3D::Model> screenModel(CreateScreenBufferModel(m_context, width, height));

    // RTT texture
    Urho3D::SharedPtr<Urho3D::Texture2D> renderTexture(new Urho3D::Texture2D(m_context));
    {
        renderTexture->SetSize(width, height, graphics->GetRGBFormat(), Urho3D::TEXTURE_RENDERTARGET);
        renderTexture->SetFilterMode(Urho3D::FILTER_BILINEAR);
    }

    // RTT scene
    m_rttScene = new Urho3D::Scene(m_context);
    {
        m_rttScene->CreateComponent<Urho3D::Octree>();

        Urho3D::Node* screenNode = m_rttScene->CreateChild();
        {
#if (defined URHO3D_OPENGL || defined URHO3D_D3D11)
            screenNode->SetPosition(Urho3D::Vector3(0.0f, 0.0f, 1.0f));
#else
            screenNode->SetPosition(Urho3D::Vector3(-0.5f, 0.5f, 1.0f));
#endif
            Urho3D::StaticModel* staticModel = screenNode->CreateComponent<Urho3D::StaticModel>();
            {
                staticModel->SetModel(screenModel);

                Urho3D::SharedPtr<Urho3D::Material> material(new Urho3D::Material(m_context));
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

            Urho3D::SharedPtr<Urho3D::Viewport> viewport(new Urho3D::Viewport(m_context, m_rttScene, camera));
            renderTexture->GetRenderSurface()->SetViewport(0, viewport);
        }
    }

    // Main scene
    m_scene = new Urho3D::Scene(m_context);
    {
        m_scene->CreateComponent<Urho3D::Octree>();

        Urho3D::Node* screenNode = m_scene->CreateChild();
        {
#if (defined URHO3D_OPENGL || defined URHO3D_D3D11)
            screenNode->SetPosition(Urho3D::Vector3(0.0f, 0.0f, 1.0f));
#else
            const float xOffset = 0.5f * m_screenModeSize.x_ / graphics->GetWidth();
            const float yOffset = 0.5f * m_screenModeSize.y_ / graphics->GetHeight();
            screenNode->SetPosition(Urho3D::Vector3(-xOffset, yOffset, 1.0f));
#endif
            Urho3D::StaticModel* staticModel = screenNode->CreateComponent<Urho3D::StaticModel>();
            {
                staticModel->SetModel(screenModel);

                Urho3D::SharedPtr<Urho3D::Material> material(new Urho3D::Material(m_context));
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

            Urho3D::SharedPtr<Urho3D::Viewport> viewport(new Urho3D::Viewport(m_context, m_scene, camera));
            m_context->GetSubsystem<Urho3D::Renderer>()->SetViewport(0, viewport);
        }
    }
}

void Quake2_RefSoft::EndFrame()
{
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

void Quake2_RefSoft::OnStart()
{
    m_screenPaletteTexture = CreateScreenPaletteTexture(m_context, m_screenPaletteData);
}

void Quake2_RefSoft::OnStop()
{
    m_scene = NULL;
    m_rttScene = NULL;
    m_screenBufferTexture = NULL;

    m_screenPaletteTexture = NULL;
}

bool Quake2_RefSoft::OnSetMode(int width, int height, bool fullscreen)
{
    m_screenBuffer.Resize(width * height);

    vid.buffer = &m_screenBuffer[0];
    vid.rowbytes = width;

    return true;
}

void Quake2_RefSoft::OnVidCheckChanges()
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

int SWimp_Init(void *hInstance, void *wndProc) { return 0; }
void SWimp_Shutdown() {}
void SWimp_BeginFrame(float camera_separation) {}
void SWimp_AppActivate(qboolean active) {}

void SWimp_EndFrame()
{
    g_refLocal.EndFrame();
}

rserr_t SWimp_SetMode(int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
    ri.Con_Printf(PRINT_ALL, "setting mode %d:", mode);

    if (g_refLocal.SetMode(*pwidth, *pheight, mode, fullscreen == qtrue))
    {
        ri.Vid_NewWindow(*pwidth, *pheight);
        ri.Con_Printf(PRINT_ALL, " %d %d %s\n", *pwidth, *pheight, fullscreen == qtrue ? "fullscreen" : "windowed");

        R_GammaCorrectAndSetPalette((const unsigned char *)d_8to24table);
        return rserr_ok;
    }
    else
    {
        ri.Con_Printf(PRINT_ALL, " invalid mode\n");

        return rserr_invalid_mode;
    }
}

void SWimp_SetPalette(const unsigned char *palette)
{
    g_refLocal.SetPalette(palette);
}

#if defined(WIN32)

extern "C"
{
#include "win32/rw_win.h"
#include "win32/winquake.h"

    unsigned fpu_ceil_cw, fpu_chop_cw, fpu_full_cw, fpu_cw, fpu_pushed_cw;
    unsigned fpu_sp24_cw, fpu_sp24_ceil_cw;
}

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
    DWORD  flOldProtect;

    if (!VirtualProtect((LPVOID)startaddr, length, PAGE_EXECUTE_READWRITE, &flOldProtect))
        ri.Sys_Error(ERR_FATAL, "Protection change failed\n");
}

/*
** Sys_SetFPCW
**
** For reference:
**
** 1
** 5               0
** xxxxRRPP.xxxxxxxx
**
** PP = 00 = 24-bit single precision
** PP = 01 = reserved
** PP = 10 = 53-bit double precision
** PP = 11 = 64-bit extended precision
**
** RR = 00 = round to nearest
** RR = 01 = round down (towards -inf, floor)
** RR = 10 = round up (towards +inf, ceil)
** RR = 11 = round to zero (truncate/towards 0)
**
*/
void Sys_SetFPCW(void)
{
    __asm xor eax, eax

    __asm fnstcw  word ptr fpu_cw
    __asm mov ax, word ptr fpu_cw

    __asm and ah, 0f0h
    __asm or  ah, 003h; round to nearest mode, extended precision
    __asm mov fpu_full_cw, eax

    __asm and ah, 0f0h
    __asm or  ah, 00fh; RTZ / truncate / chop mode, extended precision
    __asm mov fpu_chop_cw, eax

    __asm and ah, 0f0h
    __asm or  ah, 00bh; ceil mode, extended precision
    __asm mov fpu_ceil_cw, eax

    __asm and ah, 0f0h; round to nearest, 24 - bit single precision
    __asm mov fpu_sp24_cw, eax

    __asm and ah, 0f0h; ceil mode, 24 - bit single precision
    __asm or  ah, 008h;
    __asm mov fpu_sp24_ceil_cw, eax
}
#else

#include <sys/mman.h>
#include <unistd.h>

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
    int r;
    unsigned long addr;
    int psize = getpagesize();

    addr = (startaddr & ~(psize-1)) - psize;

    //fprintf(stderr, "writable code %lx(%lx)-%lx, length=%lx\n", startaddr, addr, startaddr+length, length);

    r = mprotect((char*)addr, length + startaddr - addr + psize, 7);

    if(r < 0)
         Sys_Error("Protection change failed\n");
}
#endif
