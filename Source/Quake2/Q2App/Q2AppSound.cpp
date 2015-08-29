#include "Urho3D/Urho3D.h"

#include "Q2App.h"

#include "Urho3D/Scene/Node.h"
#include "Urho3D/Audio/SoundSource.h"

extern "C"
{
#include "client/client.h"
#include "client/snd_loc.h"
}

#include "Urho3D/DebugNew.h"

Q2SoundStream::Q2SoundStream()
: m_currentSample(0)
{
}

void Q2SoundStream::Init(unsigned bufferSizeInBytes)
{
    m_dmaBuffer.Resize(bufferSizeInBytes);
}

void Q2SoundStream::Shutdown()
{
    Urho3D::MutexLock lock(m_bufferMutex);

    m_dmaBuffer.Resize(0);
    m_currentSample = 0;
}

int Q2SoundStream::GetCurrentSample() const
{
    Urho3D::MutexLock lock(m_bufferMutex);

    return m_currentSample;
}

unsigned Q2SoundStream::GetData(signed char* dest, unsigned numBytes)
{
    Urho3D::MutexLock lock(m_bufferMutex);

    const int sampleBytes = dma.samplebits / 8;
    const int dmaBufferSize = m_dmaBuffer.Size();
    const int currentByte = m_currentSample * sampleBytes;
    const int numBytesToEndOfBuffer = dmaBufferSize - currentByte;

    const int bytes1 = Urho3D::Min(numBytes, numBytesToEndOfBuffer);
    std::memcpy(dest, &m_dmaBuffer[currentByte], bytes1);
    m_currentSample += (bytes1 / sampleBytes);

    if (bytes1 < numBytes)
    {
        const int bytes2 = numBytes - bytes1;
        std::memcpy(dest + bytes1, &m_dmaBuffer[0], bytes2);
        m_currentSample = (bytes2 / sampleBytes);
    }
    
    return numBytes;
}

void Q2App::OnSNDDMAInit()
{
    Urho3D::Context *const context = GetContext();

    // Create sound stream
    m_soundStream = new Q2SoundStream();
    {
        const bool is16Bit = (dma.samplebits == 16);
        const bool isStereo = (dma.channels == 2);
        m_soundStream->SetFormat(dma.speed, is16Bit, isStereo);

        const int sampleBytes = dma.samplebits / 8;
        dma.samples = Urho3D::NextPowerOfTwo(dma.speed * dma.channels);
        m_soundStream->Init(dma.samples * sampleBytes);

        // Set dma values
        dma.buffer = &m_soundStream->m_dmaBuffer[0];
    }

    // Create sound node and play
    m_soundNode = new Urho3D::Node(context);
    {
        Urho3D::SoundSource *soundSource = m_soundNode->CreateComponent<Urho3D::SoundSource>();
        {
            soundSource->Play(m_soundStream);
        }
    }
}

void Q2App::OnSNDDMAShutdown()
{
    m_soundStream->Shutdown();

    m_soundNode = NULL;
    m_soundStream = NULL;
}

int Q2App::OnSNDDMAGetDMAPos()
{
    return m_soundStream->GetCurrentSample();
}

void Q2App::OnSNDDMABeginPainting()
{
    m_soundStream->m_bufferMutex.Acquire();
}

void Q2App::OnSNDDMASubmit()
{
    m_soundStream->m_bufferMutex.Release();
}

// Sound
qboolean SNDDMA_Init()
{
    std::memset(&dma, 0, sizeof(dma));

    dma.channels = 2;
    dma.samplebits = 16;

    if (s_khz->value == 44)
        dma.speed = 44100;
    else if (s_khz->value == 22)
        dma.speed = 22050;
    else
        dma.speed = 11025;

    dma.submission_chunk = 1;

    Q2App::GetInstance().OnSNDDMAInit();

    return qtrue;
}

void SNDDMA_Shutdown()
{
    Q2App::GetInstance().OnSNDDMAShutdown();
}

int	SNDDMA_GetDMAPos()
{
    return Q2App::GetInstance().OnSNDDMAGetDMAPos();
}

void SNDDMA_BeginPainting()
{
    Q2App::GetInstance().OnSNDDMABeginPainting();
}

void SNDDMA_Submit()
{
    Q2App::GetInstance().OnSNDDMASubmit();    
}

// CD audio
void CDAudio_Play(int track, qboolean looping) {}
void CDAudio_Stop() {}
void CDAudio_Update() {}
int CDAudio_Init() { return 0; }
void CDAudio_Shutdown() {}
