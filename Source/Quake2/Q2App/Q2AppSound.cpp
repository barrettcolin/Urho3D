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

void Q2App::OnSNDDMAInit()
{
    Urho3D::Context *const context = GetContext();

    const int sampleBytes = dma.samplebits / 8;
    const int bytesPerSecond = _MonoSamplesToBytes(dma.speed, dma.channels, sampleBytes);//dma.speed * dma.channels * sampleBytes;
    m_soundBuffer.Resize(bytesPerSecond);

    // Set dma values
    dma.buffer = &m_soundBuffer[0];
    dma.samples = m_soundBuffer.Size() / sampleBytes;

    // Create sound stream
    m_soundStream = new Urho3D::BufferedSoundStream();
    {
        const bool is16Bit = (dma.samplebits == 16);
        const bool isStereo = (dma.channels == 2);
        m_soundStream->SetFormat(dma.speed, is16Bit, isStereo);
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
    m_soundNode = NULL;
    m_soundStream = NULL;
    m_soundBuffer.Resize(0);
}

unsigned m_soundBytes = 0;

int Q2App::OnSNDDMAGetDMAPos()
{
    const unsigned sampleChannels = dma.channels;
    const unsigned sampleBytes = dma.samplebits / 8;
    const unsigned bufferSize = m_soundBuffer.Size();

    const unsigned elapsedMSec = m_soundTimer.GetMSec(false);
    const unsigned currentSample = 0.001f * elapsedMSec * dma.speed;
    const unsigned bufferSamples = _BytesToMonoSamples(bufferSize, sampleChannels, sampleBytes);

    return currentSample % bufferSamples;
#if 0
    const unsigned bufferedBytes = m_soundStream->GetBufferNumBytes();
    const unsigned bytesConsumed = m_soundBytes - bufferedBytes;
    const unsigned bufferBytes = bytesConsumed % bufferSize;
    const unsigned dmaPos = _BytesToMonoSamples(bufferBytes, sampleChannels, sampleBytes);

    LOGINFOF("OnSNDDMAGetDMAPos %d, %d", bufferedBytes, dmaPos);

    return dmaPos;
#endif
}

void Q2App::OnSNDDMASubmit()
{
    const unsigned sampleChannels = dma.channels;
    const unsigned sampleBytes = dma.samplebits / 8;
    const unsigned bufferSize = m_soundBuffer.Size();

    const unsigned paintedBytes = _MonoSamplesToBytes(paintedtime, sampleChannels, sampleBytes);
    LOGINFOF("OnSNDDMASubmit %d, %d", paintedBytes, m_soundBytes);

    const unsigned bufferBytes = m_soundBytes % bufferSize;
    const unsigned paintedBuf = paintedBytes % bufferSize;

    if (paintedBuf >= bufferBytes)
    {
        const int numBytes = paintedBuf - bufferBytes;
        m_soundStream->AddData(&m_soundBuffer[bufferBytes], numBytes);
        m_soundBytes += numBytes;
    }
    else
    {
        const int numBytes = bufferSize - bufferBytes - 1;
        m_soundStream->AddData(&m_soundBuffer[bufferBytes], numBytes);
        m_soundBytes += numBytes;
        m_soundStream->AddData(&m_soundBuffer[0], paintedBuf);
        m_soundBytes += paintedBuf;
    }
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

    //return qtrue;
    return qfalse;
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
