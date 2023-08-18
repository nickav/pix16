#pragma once

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

    // NOTE(nick): output info
    u32 samples_per_second;
    u32 bits_per_channel;
    u32 num_channels;
    u32 latency_samples;
    u32 samples_per_buffer;
    u32 buffer_size;
    u32 buffer_count;
    u32 sample_count;

    u32 played_samples;
    u32 queued_samples;
    u32 written_samples;

    i16 *samples;
    u32 sample_offset;
    u32 sample_size;

    i16 *user_samples;
};

static Win32_Audio_Context win32_audio = {};

function void audio_output_sine_wave_i16(u32 samples_per_second, u32 sample_count, f32 tone_hz, i32 tone_volume, i16 *samples);
function void audio_test_jingle(u32 samples_per_second, void *samples, u32 sample_count, void *user);

DWORD win32_audio_thread(void *user)
{
    b32 quit = false;

    //
    // NOTE(nick): load required DLLs
    //

    HMODULE ole32lib = LoadLibraryA("ole32.dll");
    if (ole32lib)
    {
        win32_audio.CoInitializeEx = (CoInitializeEx_t *)GetProcAddress(ole32lib, "CoInitializeEx");
        win32_audio.CoCreateInstance = (CoCreateInstance_t *)GetProcAddress(ole32lib, "CoCreateInstance");
        win32_audio.CoTaskMemFree = (CoTaskMemFree_t *)GetProcAddress(ole32lib, "CoTaskMemFree");
    }
    else
    {
        print("[audio] Failed to load ole32.dll\n");
        quit = true;
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
    else
    {
        print("[audio] Failed to load winmm.dll\n");
        quit = true;
    }

    HMODULE avrtlib = LoadLibraryA("avrt.dll");
    if (avrtlib)
    {
        win32_audio.AvSetMmThreadCharacteristicsW = (AvSetMmThreadCharacteristicsW_t *)GetProcAddress(avrtlib, "AvSetMmThreadCharacteristicsW");
        win32_audio.AvRevertMmThreadCharacteristics = (AvRevertMmThreadCharacteristics_t *)GetProcAddress(avrtlib, "AvRevertMmThreadCharacteristics");
    }
    else
    {
        print("[audio] Warning, failed to load avrt.dll\n");
    }

    if (quit)
    {
        if (ole32lib)
        {
            FreeLibrary(ole32lib);
        }

        if (winmmlib)
        {
            FreeLibrary(winmmlib);
        }

        MemoryZero(&win32_audio, sizeof(Win32_Audio_Context));
        print("[audio] Failed to load required DLLs\n");
    }

    DWORD index = 0;
    HANDLE task = 0;
    if (win32_audio.AvSetMmThreadCharacteristicsW)
    {
        task = win32_audio.AvSetMmThreadCharacteristicsW(L"Pro Audio", &index);
    }

    //
    // NOTE(casey): Set up our audio output buffer
    //
    u32 BitsPerChannel = 16;
    u32 SamplesPerSecond = 44100;
    u32 SamplesPerBuffer = BitsPerChannel*SamplesPerSecond/1000;
    u32 ChannelCount = 2;
    u32 BytesPerChannelValue = BitsPerChannel/8;
    u32 BytesPerSample = ChannelCount*BytesPerChannelValue;
    
    u32 BufferCount = 4;
    u32 BufferSize = SamplesPerBuffer*BytesPerSample;
    u32 HeaderSize = sizeof(WAVEHDR);
    u32 TotalBufferSize = (BufferSize+HeaderSize);
    u32 MixBufferSize = (BufferCount * SamplesPerBuffer*ChannelCount*sizeof(i16));
    u32 TotalAudioMemorySize = BufferCount*TotalBufferSize + MixBufferSize;

    u32 NextBufferIndexToPlay = 0;
    u32 SampleCount = SamplesPerBuffer;

    win32_audio.samples_per_second = SamplesPerSecond;
    win32_audio.bits_per_channel = BitsPerChannel;
    win32_audio.num_channels = ChannelCount;
    win32_audio.latency_samples = BufferCount * BufferSize;
    win32_audio.samples_per_buffer = SamplesPerBuffer;
    win32_audio.buffer_size = BufferSize;
    win32_audio.buffer_count = BufferCount;
    win32_audio.played_samples = 0;

    win32_audio.samples = (i16 *)VirtualAlloc(0, 2*BufferCount*BufferSize, MEM_COMMIT, PAGE_READWRITE);
    win32_audio.sample_offset = 0;
    win32_audio.sample_size = 2*BufferCount*BufferSize;
    win32_audio.sample_count = SampleCount;

    win32_audio.user_samples = (i16 *)VirtualAlloc(0, 2*BufferCount*BufferSize, MEM_COMMIT, PAGE_READWRITE);

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
                Header->dwFlags |= WHDR_DONE;
                
                At += TotalBufferSize;
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

    while (!quit)
    {
        MMTIME time;
        time.wType = TIME_SAMPLES;
        win32_audio.waveOutGetPosition(WaveOut, &time, sizeof(time));
        u32 played_samples = time.u.sample;
        win32_audio.played_samples = played_samples;

        u32 BufferIndex = NextBufferIndexToPlay;
        u8 *At = (u8 *)MixBuffer;
        At += MixBufferSize;
        At += TotalBufferSize * BufferIndex;

        WAVEHDR *Header = (WAVEHDR *)At;
        b32 done = (Header->dwFlags & WHDR_DONE);

        if (done)
        {
            i16 *Samples = (i16 *)Header->lpData;

            u32 SampleOffset = win32_audio.written_samples % (2 * BufferCount * SamplesPerBuffer);
            i16 *OutputSamples = (i16 *)((u8 *)(win32_audio.samples) + SampleOffset * 2 * sizeof(i16));
            MemoryCopy(Samples, OutputSamples, SampleCount * ChannelCount * sizeof(i16));
            win32_audio.written_samples += SampleCount;

            win32_audio.waveOutUnprepareHeader(WaveOut, Header, sizeof(*Header));

            DWORD error = win32_audio.waveOutPrepareHeader(WaveOut, Header, sizeof(*Header));
            if (error == MMSYSERR_NOERROR)
            {
                error = win32_audio.waveOutWrite(WaveOut, Header, sizeof(*Header));
                if (error == MMSYSERR_NOERROR)
                {
                    //Result = true;
                }
            }

            NextBufferIndexToPlay += 1;
            NextBufferIndexToPlay %= BufferCount;
        }
        
        // NOTE(nick): this call blocks waiting for new messages
        MSG Message = {};
        GetMessage(&Message, 0, 0, 0);
    }

    if (task)
    {
        win32_audio.AvRevertMmThreadCharacteristics(task);
        task = 0;
    }
    
    if (WaveOut)
    {
        win32_audio.waveOutClose(WaveOut);
    }
    
    if (AudioBufferMemory)
    {
        VirtualFree(AudioBufferMemory, 0, MEM_RELEASE);
    }

    return 0;
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
        *sample_out++ += sample_value;
        *sample_out++ += sample_value;

        t_sine += TAU / (f32)wave_period;
        if (t_sine >= TAU) {
            t_sine -= TAU;
        }
    }
}

function void audio_test_jingle(u32 samples_per_second, void *samples, u32 sample_count, void *user)
{
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

function bool win32_audio_init()
{
    HANDLE handle = CreateThread(0, 0, win32_audio_thread, 0, 0, &win32_audio.thread_id);
    SetThreadPriority(handle, THREAD_PRIORITY_TIME_CRITICAL);
    CloseHandle(handle);

    return true;
}

#if 0
function Win32_Audio_Frame win32_audio_get_frame()
{
    MMTIME time;
    time.wType = TIME_SAMPLES;
    win32_audio.waveOutGetPosition(WaveOut, &time, sizeof(time));

    i32 played_samples = time.u.sample;
    // NOTE(nick): subtract initial empty queued buffers size
    played_samples -= SamplesPerBuffer * BufferCount;

    win32_audio.cursor = played_samples;
}
#endif

function void win32_audio_shutdown()
{
    // TODO(nick): fade out audio here quickly to prevent a popping sound
}
