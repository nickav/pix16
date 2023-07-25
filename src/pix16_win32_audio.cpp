struct Audio_Context;

#define AUDIO_CALLBACK(name) void name(Audio_Context *ctx, void *samples, u32 sample_count, void *user)
typedef AUDIO_CALLBACK(Audio_Callback);

struct Audio_Context
{
    // Input
    u32 num_channels;
    u32 samples_per_second;

    Audio_Callback *callback;
    void *user_data;

    // Output
    u32 bits_per_channel;
    u32 output_sample_position;
    u32 latency_samples;
};

#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

// ole32.dll
typedef HRESULT CoInitializeEx_t(LPVOID pvReserved, WORD  dwCoInit); 
typedef HRESULT CoCreateInstance_t(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv);
typedef HRESULT CoTaskMemFree_t(LPVOID pv);

// winmm.dll
typedef MMRESULT waveOutOpen_t(LPHWAVEOUT phwo, UINT uDeviceID, LPCWAVEFORMATEX pwfx, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen);
typedef MMRESULT waveOutClose_t(HWAVEOUT hwo);
typedef MMRESULT waveOutWrite_t(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh);
typedef MMRESULT waveOutPrepareHeader_t(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh);
typedef MMRESULT waveOutUnprepareHeader_t(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh);
typedef MMRESULT waveOutGetPosition_t(HWAVEOUT hwo, LPMMTIME pmmt, UINT cbmmt);
typedef MMRESULT timeBeginPeriod_t(UINT uPeriod);

// avrt.dll
typedef HANDLE AvSetMmThreadCharacteristicsW_t(LPCWSTR TaskName, LPDWORD TaskIndex);
typedef BOOL AvRevertMmThreadCharacteristics_t(HANDLE AvrtHandle);

struct Win32_Audio_Context
{
    Audio_Context *ctx;

    CoInitializeEx_t   *CoInitializeEx;
    CoCreateInstance_t *CoCreateInstance;
    CoTaskMemFree_t    *CoTaskMemFree;

    waveOutOpen_t *waveOutOpen;
    waveOutClose_t *waveOutClose;
    waveOutWrite_t *waveOutWrite;
    waveOutPrepareHeader_t *waveOutPrepareHeader;
    waveOutUnprepareHeader_t *waveOutUnprepareHeader;
    waveOutGetPosition_t *waveOutGetPosition;
    timeBeginPeriod_t *timeBeginPeriod;

    AvSetMmThreadCharacteristicsW_t *AvSetMmThreadCharacteristicsW;
    AvRevertMmThreadCharacteristics_t *AvRevertMmThreadCharacteristics;

    DWORD thread_id;
    //u64 output_position;
};

static Win32_Audio_Context win32_audio = {};

//
// Winmm backend
//

bool winmm__submit_empty_audio(HWAVEOUT WaveOut, WAVEHDR *Header, u32 SampleCount) {
    bool result = false;
    
    i16 *Samples = (i16 *)Header->lpData;
    MemoryZero(Samples, SampleCount);

    DWORD error = win32_audio.waveOutPrepareHeader(WaveOut, Header, sizeof(*Header));
    if (error == MMSYSERR_NOERROR)
    {
        error = win32_audio.waveOutWrite(WaveOut, Header, sizeof(*Header));
        if (error == MMSYSERR_NOERROR)
        {
            result = true;
        }
    }
    
    return result;
}

bool winmm__submit_audio(HWAVEOUT WaveOut, WAVEHDR *Header, u32 SampleCount, f32 *mix_buffer) {
    bool Result = false;
    
    /*
    u32 mix_buffer_size = SampleCount*2*sizeof(f32);
    memory_zero(mix_buffer, mix_buffer_size);
    */
    
    i16 *Samples = (i16 *)Header->lpData;

    Audio_Context *ctx = win32_audio.ctx;
    if (ctx->callback)
    {
       ctx->callback(ctx, Samples, SampleCount, ctx->user_data);
    }

    DWORD error = win32_audio.waveOutPrepareHeader(WaveOut, Header, sizeof(*Header));
    if (error == MMSYSERR_NOERROR)
    {
        error = win32_audio.waveOutWrite(WaveOut, Header, sizeof(*Header));
        if (error == MMSYSERR_NOERROR)
        {
            Result = true;
        }
    }
    
    return Result;
}

void winmm__run(Audio_Context *ctx)
{
    //
    // NOTE(casey): Set up our audio output buffer
    //
    u32 SamplesPerSecond = ctx->samples_per_second;
    u32 SamplesPerBuffer = 16*SamplesPerSecond/1000;
    u32 ChannelCount = ctx->num_channels;
    u32 BytesPerChannelValue = ctx->bits_per_channel/8;
    u32 BytesPerSample = ChannelCount*BytesPerChannelValue;
    
    u32 BufferCount = 4;
    u32 BufferSize = SamplesPerBuffer*BytesPerSample;
    u32 HeaderSize = sizeof(WAVEHDR);
    u32 TotalBufferSize = (BufferSize+HeaderSize);
    u32 MixBufferSize = (SamplesPerBuffer*ChannelCount*sizeof(f32));
    u32 TotalAudioMemorySize = BufferCount*TotalBufferSize + MixBufferSize;

    ctx->latency_samples = BufferCount * BufferSize;

    //
    // NOTE(casey): Initialize audio out
    //
    HWAVEOUT WaveOut = {};
    
    WAVEFORMATEX Format = {};
    Format.wFormatTag = WAVE_FORMAT_PCM;
    Format.nChannels = (WORD)ChannelCount;
    Format.wBitsPerSample = (WORD)(8*BytesPerChannelValue);
    Format.nSamplesPerSec = SamplesPerSecond;
    Format.nBlockAlign = (Format.nChannels*Format.wBitsPerSample)/8;
    Format.nAvgBytesPerSec = Format.nBlockAlign * Format.nSamplesPerSec;
    
    bool quit = false;
    
    void *MixBuffer = 0;
    void *AudioBufferMemory = 0;
    MMRESULT error = win32_audio.waveOutOpen(&WaveOut, WAVE_MAPPER, &Format, GetCurrentThreadId(), 0, CALLBACK_THREAD);
    if (error == MMSYSERR_NOERROR)
    {
        AudioBufferMemory = VirtualAlloc(0, TotalAudioMemorySize, MEM_COMMIT, PAGE_READWRITE);
        if (AudioBufferMemory)
        {
            u8 *At = (u8 *)AudioBufferMemory;
            MixBuffer = At;
            At += MixBufferSize;
            
            for (u32 BufferIndex = 0; BufferIndex < BufferCount; ++BufferIndex)
            {
                WAVEHDR *Header = (WAVEHDR *)At;
                Header->lpData = (char *)(Header + 1);
                Header->dwBufferLength = BufferSize;
                
                At += TotalBufferSize;
                
                winmm__submit_empty_audio(WaveOut, Header, SamplesPerBuffer);
            }
        }
        else
        {
            print("[audio] error allocating audio buffer memory!\n");
            quit = true;
        }
    }
    else
    {
        print("[audio] waveOutOpen returned error code: %d\n", error);
        quit = true;
    }
    
    //
    // NOTE(casey): Serve audio forever (until we are told to stop)
    //

    for (;!quit;)
    {
        MSG Message = {};
        GetMessage(&Message, 0, 0, 0);
        if (Message.message == MM_WOM_DONE)
        {
            // @Incomplete: we might want this to happen more than every MM_WOM_DONE message?
            MMTIME time;
            time.wType = TIME_SAMPLES;
            win32_audio.waveOutGetPosition(WaveOut, &time, sizeof(time));
            // NOTE(nick): subtract initial empty queued buffers size
            u32 played_samples = time.u.sample - SamplesPerBuffer * BufferCount;
            win32_audio.ctx->output_sample_position = played_samples;
            

            WAVEHDR *Header = (WAVEHDR *)Message.lParam;
            if (Header->dwFlags & WHDR_DONE)
            {
                Header->dwFlags &= ~WHDR_DONE;
            }

            win32_audio.waveOutUnprepareHeader(WaveOut, Header, sizeof(*Header));
            
            winmm__submit_audio(WaveOut, Header, SamplesPerBuffer, (f32*)MixBuffer);
        }
    }
    
    if (WaveOut)
    {
        win32_audio.waveOutClose(WaveOut);
    }
    
    if (AudioBufferMemory)
    {
        VirtualFree(AudioBufferMemory, 0, MEM_RELEASE);
    }
}

DWORD win32_audio_thread(void *user)
{
    Audio_Context *ctx = win32_audio.ctx;
    {
        HMODULE ole32lib = LoadLibraryA("ole32.dll");
        if (ole32lib)
        {
            win32_audio.CoInitializeEx = (CoInitializeEx_t *)GetProcAddress(ole32lib, "CoInitializeEx");
            win32_audio.CoCreateInstance = (CoCreateInstance_t *)GetProcAddress(ole32lib, "CoCreateInstance");
            win32_audio.CoTaskMemFree = (CoTaskMemFree_t *)GetProcAddress(ole32lib, "CoTaskMemFree");
        }

        HMODULE winmmlib = LoadLibraryA("winmm.dll");
        if (winmmlib)
        {
            win32_audio.waveOutOpen = (waveOutOpen_t *)GetProcAddress(winmmlib, "waveOutOpen");
            win32_audio.waveOutClose = (waveOutClose_t *)GetProcAddress(winmmlib, "waveOutClose");
            win32_audio.waveOutWrite = (waveOutWrite_t *)GetProcAddress(winmmlib, "waveOutWrite");
            win32_audio.waveOutPrepareHeader = (waveOutPrepareHeader_t *)GetProcAddress(winmmlib, "waveOutPrepareHeader");
            win32_audio.waveOutUnprepareHeader = (waveOutUnprepareHeader_t *)GetProcAddress(winmmlib, "waveOutUnprepareHeader");
            win32_audio.waveOutGetPosition = (waveOutGetPosition_t *)GetProcAddress(winmmlib, "waveOutGetPosition");
            win32_audio.timeBeginPeriod = (timeBeginPeriod_t *)GetProcAddress(winmmlib, "timeBeginPeriod");
        }

        HMODULE avrtlib = LoadLibraryA("avrt.dll");
        if (avrtlib)
        {
            win32_audio.AvSetMmThreadCharacteristicsW = (AvSetMmThreadCharacteristicsW_t *)GetProcAddress(avrtlib, "AvSetMmThreadCharacteristicsW");
            win32_audio.AvRevertMmThreadCharacteristics = (AvRevertMmThreadCharacteristics_t *)GetProcAddress(avrtlib, "AvRevertMmThreadCharacteristics");
        }
    }

    DWORD index = 0;
    HANDLE task = 0;
    if (win32_audio.AvSetMmThreadCharacteristicsW)
    {
        task = win32_audio.AvSetMmThreadCharacteristicsW(L"Pro Audio", &index);
    }

    winmm__run(win32_audio.ctx);

    if (task)
    {
        win32_audio.AvRevertMmThreadCharacteristics(task);
        task = 0;
    }
    
    return(0);
}

function void audio_output_sine_wave_i16(u32 samples_per_second, u32 sample_count, f32 tone_hz, i32 tone_volume, i16 *samples)
{
    static f32 t_sine = 0;
    int wave_period = samples_per_second / tone_hz;

    i16 *sample_out = samples;
    for (int sample_index = 0; sample_index < sample_count; sample_index++)
    {
        f32 sine_value   = sin_f32(t_sine);
        i16 sample_value = (i16)(sine_value * tone_volume);
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

        t_sine += TAU / (f32)wave_period;
        if (t_sine >= TAU) {
            t_sine -= TAU;
        }
    }
}

function void audio_test_jingle(Audio_Context *ctx, void *samples, u32 sample_count, void *user)
{
    u32 samples_per_second = ctx->samples_per_second;

    f32 tone_hz = 440;
    {
        static u64 total_samples_written = 0;

        i32 buffer_index = total_samples_written % samples_per_second;
        i32 buffer_size = samples_per_second;

        tone_hz = 440;
        if (total_samples_written % (samples_per_second * 8) >= samples_per_second * 4) tone_hz = 466.16;

        if (buffer_index > buffer_size * 1 / 4) tone_hz = 523.25;
        if (buffer_index > buffer_size * 2 / 4) tone_hz = 659.25;
        if (buffer_index > buffer_size * 3 / 4) tone_hz = 783.99;

        total_samples_written += sample_count;
    }

    i32 tone_volume = 3000;
    audio_output_sine_wave_i16(samples_per_second, sample_count, tone_hz, tone_volume, (i16 *)samples);
}

function bool audio_init(Audio_Context *ctx)
{
    win32_audio.ctx = ctx;

    ctx->bits_per_channel = 16;
    ctx->output_sample_position = 0;
    ctx->latency_samples = 0;

    // TODO(nick): support buffer other formats
    assert(ctx->samples_per_second == 48000);
    assert(ctx->bits_per_channel == 16);
    assert(ctx->num_channels == 2);

    DWORD thread_id = 0;
    HANDLE handle = CreateThread(0, 0, win32_audio_thread, 0, 0, &thread_id);
    SetThreadPriority(handle, THREAD_PRIORITY_TIME_CRITICAL);
    CloseHandle(handle);

    win32_audio.thread_id = thread_id;

    return true;
}
