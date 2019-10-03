#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <vector>

#define filename "assets/faxanadu.mid"

#if defined(WIN32)
#include <Audioclient.h>
#include <Mmdeviceapi.h>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

UINT32 bufferFrameCount = 0;
WAVEFORMATEX* pWaveFormat = nullptr;
IMMDeviceEnumerator* pEnumerator = nullptr;
IMMDevice* pDevice = nullptr;
IAudioClient* pAudioClient = nullptr;
HANDLE pEventHandler = nullptr;
IAudioRenderClient* pRenderClient = nullptr;
#else
#error Unimplemented audio engine for that platform
#endif

bool init_audio();
bool update_audio();
void cleanup_audio();
void progress(int frameCount, int sampleRate, int channelCount, float* pOut);
bool open_midi();
void update_midi(float dt);

uint32_t sample_rate = 0;
uint32_t total_ticks = 0;

#define EVENT_NOTE_OFF 0
#define EVENT_NOTE_ON 1
#define EVENT_VOLUME 2
#define EVENT_END_OF_TRACK 3

static float volume = 1.0f;
#define MAX_VOLUME 0.25f;

struct Event
{
    uint32_t time;
    int track;
    int type;
    int note;
    float vel;
};

struct Instrument
{
    float freq = 0.0f;
    float period = 0.0f;
    float vol = 0.0f;
    float sustain = 0.0f;
    float shift = 0.5f;
    int next_event = 0;
    std::vector<Event> events;
};
Instrument instruments[4];

uint8_t *pMidiData = nullptr;
uint32_t playback_ticks = 0;

// Note frequencies
static const float NOTE_FREQS[] = {
    16.35f, // C0
    17.32f, // C#0
    18.35f, // D0
    19.45f, // D#0
    20.60f, // E0
    21.83f, // F0
    23.12f, // F#0
    24.50f, // G0
    25.96f, // G#0
    27.50f, // A0
    29.14f, // A#0
    30.87f, // B0
    32.70f, // C1
    34.65f,
    36.71f,
    38.89f,
    41.20f,
    43.65f,
    46.25f,
    49.00f,
    51.91f,
    55.00f,
    58.27f,
    61.74f,
    65.41f, // C2
    69.30f,
    73.42f,
    77.78f,
    82.41f,
    87.31f,
    92.50f,
    98.00f,
    103.83f,
    110.00f,
    116.54f,
    123.47f,
    130.81f, // C3
    138.59f,
    146.83f,
    155.56f,
    164.81f,
    174.61f,
    185.00f,
    196.00f,
    207.65f,
    220.00f,
    233.08f,
    246.94f,
    261.63f, // C4
    277.18f,
    293.66f,
    311.13f,
    329.63f,
    349.23f,
    369.99f,
    392.00f,
    415.30f,
    440.00f,
    466.16f,
    493.88f,
    523.25f, // C5
    554.37f,
    587.33f,
    622.25f,
    659.25f,
    698.46f,
    739.99f,
    783.99f,
    830.61f,
    880.00f,
    932.33f,
    987.77f,
    1046.50f, // C6
    1108.73f,
    1174.66f,
    1244.51f,
    1318.51f,
    1396.91f,
    1479.98f,
    1567.98f,
    1661.22f,
    1760.00f,
    1864.66f,
    1975.53f,
    2093.00f, // C7
    2217.46f,
    2349.32f,
    2489.02f,
    2637.02f,
    2793.83f,
    2959.96f,
    3135.96f,
    3322.44f,
    3520.00f,
    3729.31f,
    3951.07f,
    4186.01f, // C8
    4434.92f,
    4698.63f,
    4978.03f,
    5274.04f,
    5587.65f,
    5919.91f,
    6271.93f,
    6644.88f,
    7040.00f,
    7458.62f,
    7902.13f
};
static const int NOTE_C4 = 12 * 4;
static const int NOTE_COUNT = sizeof(NOTE_FREQS) / sizeof(float);

int main()
{
    srand((unsigned int)time(0));

    if (!init_audio())
    {
        printf("Failed to init audio\n");
        cleanup_audio();
        system("pause");
        return 1;
    }

    if (!open_midi())
    {
        printf("Failed to load midi file\n");
        cleanup_audio();
        system("pause");
        return 2;
    }

    instruments[0].sustain = (float)(0.75 / (double)sample_rate);
    instruments[1].sustain = instruments[0].sustain;
    instruments[2].sustain = instruments[0].sustain;
    instruments[3].sustain = (float)(8 / (double)sample_rate);

    int printDelay = 0;
    while (update_audio())
    {
        printDelay++;
        if (printDelay > 10)
        {
            printDelay = 0;
            printf("\r");
            int percent = (playback_ticks * 70) / total_ticks;
            for (int i = 0; i < percent; ++i)
            {
                printf("-");
            }
            printf("|");
            for (int i = percent + 1; i < 70; ++i)
            {
                printf("-");
            }
        }
    }
    printf("\n");

    cleanup_audio();
    delete[] pMidiData;

    system("pause");
    return 0;
}

bool init_audio()
{
#if defined(WIN32)
    HRESULT hr;

    hr = CoInitialize(0);
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    hr = pAudioClient->GetMixFormat(&pWaveFormat);
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 50000, 0, pWaveFormat, NULL);
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    pEventHandler = CreateEvent(nullptr, false, false, nullptr);
    assert(pEventHandler);
    if (pEventHandler == nullptr) return false;

    hr = pAudioClient->SetEventHandle(pEventHandler);
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    hr = pAudioClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClient);
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    hr = pAudioClient->Start();  // Start playing.
    assert(hr == S_OK);
    if (hr != S_OK) return false;

    sample_rate = (uint32_t)pWaveFormat->nSamplesPerSec;
#else
#endif

    return true;
}

void cleanup_audio()
{
#if defined(WIN32)
    CoTaskMemFree(pWaveFormat);
    if (pEnumerator) pEnumerator->Release();
    if (pDevice) pDevice->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pRenderClient) pRenderClient->Release();
#else
#endif
}

bool update_audio()
{
#if defined(WIN32)
    HRESULT hr;
    UINT32 numFramesAvailable;
    UINT32 numFramesPadding;
    BYTE *pData;
    DWORD retval;

    retval = WaitForSingleObject(pEventHandler, 2000);
    if (retval != WAIT_OBJECT_0)
    {
        // Event handle timed out after a 2-second wait.
        pAudioClient->Stop();
        return false;
    }

    // See how much buffer space is available.
    hr = pAudioClient->GetCurrentPadding(&numFramesPadding);
    assert(hr == S_OK);
    if (hr != S_OK)
    {
        return false;
    }

    numFramesAvailable = bufferFrameCount - numFramesPadding;
    if (numFramesAvailable > 0)
    {
        // Grab all the available space in the shared buffer.
        hr = pRenderClient->GetBuffer(numFramesAvailable, &pData);
        assert(hr == S_OK);
        if (hr != S_OK)
        {
            return false;
        }

        progress(numFramesAvailable, pWaveFormat->nSamplesPerSec, pWaveFormat->nChannels, (float*)pData);

        hr = pRenderClient->ReleaseBuffer(numFramesAvailable, 0);
        assert(hr == S_OK);
        if (hr != S_OK)
        {
            return false;
        }
    }
#else
#endif

    // Are we done?
    bool done = true;
    for (int i = 0; i < 4; ++i)
    {
        if (instruments[i].next_event < (int)instruments[i].events.size())
        {
            done = false;
            break;
        }
    }

    return !done;
}

struct MidiChunk
{
    char type[4];
    uint32_t len;
    uint8_t *pData;
};

void readType(char *out, uint32_t *pos, uint8_t *pData)
{
    memcpy(out, pData + *pos, 4);
    *pos += 4;
}

uint8_t readByte(uint32_t *pos, uint8_t *pData)
{
    uint8_t out = pData[*pos];
    *pos += 1;
    return out;
}

uint16_t readUint16(uint32_t *pos, uint8_t *pData)
{
    uint16_t out;
    out =
        (((pData[*pos + 0]) << 8) & 0xFF00) |
         ((pData[*pos + 1])       & 0x00FF);
    *pos += 2;
    return out;
}


uint32_t readUint24(uint32_t *pos, uint8_t *pData)
{
    uint32_t out;
    out =
        (((pData[*pos + 0]) << 16) & 0x00FF0000) |
        (((pData[*pos + 1]) << 8)  & 0x0000FF00) |
         ((pData[*pos + 2])        & 0x000000FF);
    *pos += 3;
    return out;
}

uint32_t readUint32(uint32_t *pos, uint8_t *pData)
{
    uint32_t out;
    out =
        (((pData[*pos + 0]) << 24) & 0xFF000000) |
        (((pData[*pos + 1]) << 16) & 0x00FF0000) |
        (((pData[*pos + 2]) << 8)  & 0x0000FF00) |
         ((pData[*pos + 3])        & 0x000000FF);
    *pos += 4;
    return out;
}

uint32_t readVariableInt(uint32_t *pos, uint8_t *pData)
{
    uint32_t out = 0;

    uint32_t next = pData[*pos];
    *pos += 1;
    if (next & 0x80)
    {
        out = ((out << 7) & 0xFFFFFF80) | (next & 0x7F);
    }
    else
    {
        return next;
    }

    while (next & 0x80)
    {
        next = pData[*pos];
        out = ((out << 7) & 0xFFFFFF80) | (next & 0x7F);
        *pos += 1;
    }

    return out;
}

uint8_t *readData(uint32_t *pos, uint8_t *pData, uint32_t len)
{
    uint8_t *out = pData + *pos;
    *pos += len;
    return out;
}

bool open_midi()
{
    FILE *file = fopen(filename, "rb");
    fseek(file, 0, SEEK_END);
    auto size = ftell(file);
    fseek(file, 0, SEEK_SET);
    pMidiData = new uint8_t[size];
    fread(pMidiData, 1, size, file);
    fclose(file);

    uint32_t pos = 0;
    MidiChunk chunk;
    uint32_t quarterNoteTime;
    int currentTrack = 0;
    uint32_t tempo = 120;
    while (pos < (uint32_t)size)
    {
        // Read next chunk
        readType(chunk.type, &pos, pMidiData);
        chunk.len = readUint32(&pos, pMidiData);
        chunk.pData = readData(&pos, pMidiData, chunk.len);

        if (strncmp(chunk.type, "MThd", 4) == 0)
        {
            uint32_t i = 0;
            uint16_t format = readUint16(&i, chunk.pData);
            if (format != 0 && format != 1 && format != 2)
            {
                assert(false);
                return false;
            }
            uint16_t tracks = (int)readUint16(&i, chunk.pData);
            if (tracks == 0)
            {
                assert(false);
                return false;
            }
            uint16_t division = (int)readUint16(&i, chunk.pData);
            if (division & 0x8000)
            {
                assert(false);
            }
            else
            {
                quarterNoteTime = (uint32_t)division;
            }
        }
        else if (strncmp(chunk.type, "MTrk", 4) == 0)
        {
            if (currentTrack == 4)
            {
                break; // Stop here, we emulate only 4 tracks for our NES toy
            }

            Instrument *pInst = instruments + currentTrack;

            uint32_t t = 0;
            uint32_t i = 0;

            while (i < chunk.len)
            {
                Event e;
                e.track = currentTrack;

                uint32_t delta_time = readVariableInt(&i, chunk.pData);
                t += delta_time;

                double tick_per_second = (double)tempo * (double)quarterNoteTime / 60.0;
                double samples_per_tick = (double)sample_rate / tick_per_second;
                e.time = (uint32_t)((double)t * samples_per_tick);

                uint8_t status_byte = readByte(&i, chunk.pData);
                uint8_t channel = status_byte & 0xF; // Probably don't care

                switch ((status_byte >> 4) & 0xF)
                {
                    case 0x8: // Note Off event
                    {
                        e.note = (int)readByte(&i, chunk.pData) & 0x7F;
                        e.vel = (float)((int)readByte(&i, chunk.pData) & 0x7F) / 127.0f;
                        e.type = EVENT_NOTE_OFF;
                        pInst->events.push_back(e);
                        break;
                    }
                    case 0x9: // Note On event
                    {
                        e.note = (int)readByte(&i, chunk.pData) & 0x7F;
                        e.vel = (float)((int)readByte(&i, chunk.pData) & 0x7F) / 127.0f;
                        e.type = EVENT_NOTE_ON;
                        pInst->events.push_back(e);
                        break;
                    }
                    case 0xA: // Polyphonic Key Pressure (Aftertouch)
                    {
                        readByte(&i, chunk.pData);
                        readByte(&i, chunk.pData);
                        break;
                    }
                    case 0xB: // Control Change
                    {
                        auto controller = readByte(&i, chunk.pData) & 0x7F;
                        auto val = (float)((int)readByte(&i, chunk.pData) & 0x7F) / 127.0f;
                        if (controller == 7)
                        {
                            e.vel = volume;
                            e.type = EVENT_VOLUME;
                            pInst->events.push_back(e);
                        }
                        break;
                    }
                    case 0xC: // Program Change
                    {
                        readByte(&i, chunk.pData);
                        break;
                    }
                    case 0xD: // Channel Pressure (After-touch)
                    {
                        readByte(&i, chunk.pData);
                        break;
                    }
                    case 0xE: // Pitch Wheel Change
                    {
                        readByte(&i, chunk.pData);
                        readByte(&i, chunk.pData);
                        break;
                    }
                    case 0xF: // System Common Messages
                    {
                        switch (status_byte)
                        {
                            case 0xFF: // Meta Events
                            {
                                uint8_t type = readByte(&i, chunk.pData);
                                uint32_t meta_len = readVariableInt(&i, chunk.pData);
                                auto pMetaData = readData(&i, chunk.pData, meta_len);
                                switch (type)
                                {
                                    case 0x51: // Set Tempo
                                    {
                                        uint32_t j = 0;
                                        uint32_t new_tempo = readUint24(&j, pMetaData);
                                        double beat_per_second = 1.0 / ((double)new_tempo / 1000000.0);
                                        tempo = (uint32_t)(beat_per_second * 60);
                                        break;
                                    }
                                    case 0x2F: // End of track
                                    {
                                        e.type = EVENT_END_OF_TRACK;
                                        pInst->events.push_back(e);
                                        total_ticks = std::max<uint32_t>(total_ticks, e.time);
                                        break;
                                    }
                                    case 0x03:
                                    {
                                        char name[250] = { '\0' };
                                        memcpy(name, pMetaData, meta_len);
                                        printf("Track %i name: %s\n", currentTrack, name);
                                        break;
                                    }
                                    default:
                                    {
                                        break;
                                    }
                                }
                                break;
                            }
                            default:
                            {
                                assert(false);
                                return false;
                            }
                        }
                        break;
                    }
                    default:
                    {
                        assert(false);
                        return false;
                    }
                }
            }

            ++currentTrack;
        }
    }

    printf("Midi file loaded\n");
    return true;
}

void update_midi(float dt)
{
    ++playback_ticks;

    for (int i = 0; i < 4; ++i)
    {
        auto pInst = instruments + i;
        pInst->vol = std::max<float>(0.0f, pInst->vol - pInst->sustain);

        if (pInst->next_event < (int)pInst->events.size())
        {
            auto& e = pInst->events[pInst->next_event];
            if (e.time <= playback_ticks)
            {
                ++pInst->next_event;
                switch (e.type)
                {
                    case EVENT_NOTE_OFF:
                    {
                        int note_id = e.note - 60 + NOTE_C4;
                        if (note_id >= 0 && note_id < NOTE_COUNT)
                        {
                            pInst->freq = NOTE_FREQS[note_id];
                            pInst->vol = 0.0f;
                        }
                        break;
                    }
                    case EVENT_NOTE_ON:
                    {
                        int note_id = e.note - 60 + NOTE_C4;
                        if (note_id >= 0 && note_id < NOTE_COUNT)
                        {
                            pInst->freq = NOTE_FREQS[note_id];
                            pInst->vol = e.vel;
                            if (i == 3)
                            {
                                // Drum, adjust shift
                                //pInst->sustain = 0.05f / pInst->freq;
                            }
                        }
                        break;
                    }
                    case EVENT_VOLUME:
                    {
                        volume = e.vel;
                        break;
                    }
                }
            }
        }
    }
}

void update_instrument(int index, float dt)
{
    instruments[index].period = std::fmodf(instruments[index].period + instruments[index].freq * dt, 1.0f);
}

float amp_to_4bits(float amplitude)
{
    return (float)(int)((amplitude + 1.0f) * 8.0f) / 8.0f - 1.0f;
}

float progress_pulse1(float dt)
{
    update_instrument(0, dt);
    return amp_to_4bits((instruments[0].period < instruments[0].shift ? 1.0f : -1.0f) * instruments[0].vol);
}

float progress_pulse2(float dt)
{
    update_instrument(1, dt);
    return amp_to_4bits((instruments[1].period < instruments[1].shift ? 1.0f : -1.0f) * instruments[1].vol);
}

float progress_triangle(float dt)
{
    update_instrument(2, dt);
    return amp_to_4bits((instruments[2].period < instruments[2].shift ? (instruments[2].period * 4.0f - 1.0f) : (1.0f - (instruments[2].period * 4.0f - 2.0f))) * instruments[2].vol);
}

float progress_noise(float dt)
{
    update_instrument(3, dt);
    return amp_to_4bits((float)rand() / (float)RAND_MAX * instruments[3].vol);
}

void progress(int frameCount, int sampleRate, int channelCount, float* pOut)
{
    memset(pOut, 0, sizeof(float) * frameCount * channelCount);

    float dt = 1.0f / (float)sampleRate;

    for (int i = 0; i < frameCount; ++i)
    {
        update_midi(dt);

        float sample =
            progress_pulse1(dt) +
            progress_pulse2(dt) +
            progress_triangle(dt) +
            progress_noise(dt);

        sample = std::min<float>(1.0f, sample);
        sample = std::max<float>(-1.0f, sample) * volume * MAX_VOLUME;

        for (int c = 0; c < channelCount; ++c)
        {
            pOut[i * channelCount + c] = sample;
        }
    }
}
