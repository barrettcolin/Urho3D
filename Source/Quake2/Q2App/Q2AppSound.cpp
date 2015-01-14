#include "Q2App.h"

#include "BufferedSoundStream.h"
#include "Log.h"
#include "Node.h"
#include "SoundSource.h"

extern "C"
{
#include "client/client.h"
#include "client/snd_loc.h"
}

#include "DebugNew.h"

// 'paintedtime' with:
// dma.speed=22050, dma.samplebits=16, dma.channels=2, mixahead=0.2
// at sound dma pos = 0, paintedtime = 4410 (= 22050 * 0.2)
// so, paintedtime is 'mono' samples
namespace
{
    const unsigned DMA_BLOCK_SIZE = 512;
}

static unsigned _MonoSamplesToBytes(unsigned samples, int sampleChannels, int sampleBytes)
{
    return samples * sampleChannels * sampleBytes;
}

static unsigned _BytesToMonoSamples(unsigned bytes, int sampleChannels, int sampleBytes)
{
    return bytes / sampleChannels / sampleBytes;
}

Q2SoundStream::Q2SoundStream()
: m_soundBufferPos(0)
{
}

void Q2SoundStream::Init(unsigned bufferSizeInBytes)
{
    Urho3D::MutexLock lock(m_bufferMutex);

    m_soundBuffer.Resize(bufferSizeInBytes);
}

void Q2SoundStream::Shutdown()
{
    Urho3D::MutexLock lock(m_bufferMutex);

    m_soundBuffer.Resize(0);
    m_soundBufferPos = 0;
}

int Q2SoundStream::GetSoundBufferPos() const
{
    Urho3D::MutexLock lock(m_bufferMutex);

    return m_soundBufferPos;
}

unsigned Q2SoundStream::GetData(signed char* dest, unsigned numBytes)
{
    Urho3D::MutexLock lock(m_bufferMutex);

    const unsigned soundBufferSize = m_soundBuffer.Size();
    const unsigned newPos = m_soundBufferPos + numBytes;
    const int overrun = newPos - soundBufferSize;
    if (overrun <= 0)
    {
        memcpy(dest, &m_soundBuffer[m_soundBufferPos], numBytes);
    }
    else
    {
        memcpy(dest, &m_soundBuffer[m_soundBufferPos], numBytes - overrun);
        memcpy(dest, &m_soundBuffer[0], overrun);
    }

    m_soundBufferPos = newPos % soundBufferSize;

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
        const int bytesPerSecond = _MonoSamplesToBytes(dma.speed, dma.channels, sampleBytes);
        m_soundStream->Init(bytesPerSecond);

        // Set dma values
        dma.buffer = &m_soundStream->m_soundBuffer[0];
        dma.samples = m_soundStream->m_soundBuffer.Size() / sampleBytes;
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
    const unsigned sampleChannels = dma.channels;
    const unsigned sampleBytes = dma.samplebits / 8;

    const int bufferSamplePos = m_soundStream->GetSoundBufferPos();
    return _BytesToMonoSamples(bufferSamplePos, sampleChannels, sampleBytes);
}

void Q2App::OnSNDDMASubmit()
{
}

// Sound
qboolean SNDDMA_Init()
{
    std::memset(&dma, 0, sizeof(dma));

    dma.channels = 2;
    dma.samplebits = 16;

    if (s_khz->value == 44)
        dma.speed = 44100;
    if (s_khz->value == 22)
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

void SNDDMA_BeginPainting() {}

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
