#pragma once

#if PROFILER

// TODO(nick): support other platforms
function f64 ReadOSTimer()
{
    static u64 win32_ticks_per_second = 0;
    static u64 win32_counter_offset = 0;

    if (win32_ticks_per_second == 0)
    {
        LARGE_INTEGER perf_frequency = {};
        if (QueryPerformanceFrequency(&perf_frequency)) {
            win32_ticks_per_second = perf_frequency.QuadPart;
        }
        LARGE_INTEGER perf_counter = {};
        if (QueryPerformanceCounter(&perf_counter)) {
            win32_counter_offset = perf_counter.QuadPart;
        }

        assert(win32_ticks_per_second != 0);
    }

    f64 result = 0;

    LARGE_INTEGER perf_counter;
    if (QueryPerformanceCounter(&perf_counter)) {
        perf_counter.QuadPart -= win32_counter_offset;
        result = (f64)(perf_counter.QuadPart) / win32_ticks_per_second;
    }

    return result;
}

force_inline u64 ReadCPUTimer(void)
{
    #if defined(COMPILER_MSVC) && !defined(__clang__)
        return __rdtsc();
    #elif defined(__i386__)
        u64 x;
        __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
        return x;
    #elif defined(__x86_64__)
        u32 hi, lo;
        __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
        return (cast(u64)lo) | ((cast(u64)hi)<<32);
    #else
        #error "[ReadCPUTimer] Unsupported OS/compiler!"
    #endif
}

struct Profile_Anchor
{
    u64 tsc_elapsed_exclusive;
    u64 tsc_elapsed_inclusive;
    u64 hit_count;
    u64 frame_count;
    const char *label;
};

struct Profiler
{
    Profile_Anchor anchors[4096];
    Profile_Anchor metrics[4096];

    u64 start_tsc;
    u64 end_tsc;
    u64 cpu_freq;
};

static Profiler g_profiler = {0};
static u32 g_profiler_parent = 0;

struct Profile_Block
{
    const char *label;
    u64 old_tsc_elapsed_inclusive;
    u64 start_tsc;
    u32 anchor_index;
    u32 parent_index;

    Profile_Block(const char *label_, u32 anchor_index_)
    {
        parent_index = g_profiler_parent;

        anchor_index = anchor_index_;
        label = label_;

        Profile_Anchor *anchor = g_profiler.anchors + anchor_index;
        old_tsc_elapsed_inclusive = anchor->tsc_elapsed_inclusive;

        g_profiler_parent = anchor_index;
        start_tsc = ReadCPUTimer();
    }

    ~Profile_Block()
    {
        u64 elapsed = ReadCPUTimer() - start_tsc;
        g_profiler_parent = parent_index;

        Profile_Anchor *parent = g_profiler.anchors + parent_index;
        Profile_Anchor *anchor = g_profiler.anchors + anchor_index;

        parent->tsc_elapsed_exclusive -= elapsed;

        anchor->tsc_elapsed_exclusive += elapsed;
        anchor->tsc_elapsed_inclusive = old_tsc_elapsed_inclusive + elapsed;
        anchor->hit_count += 1;
        anchor->label = label;
    }
};

#define NameConcat2(A, B) A##B
#define NameConcat(A, B) NameConcat2(A, B)
#define TimeBlock(Name) Profile_Block NameConcat(Block, __LINE__)(Name, __COUNTER__ + 1);
#define TimeFunction TimeBlock(__func__)

function u64 profiler__estimate_clocks_per_second()
{
    f64 estimate_time = 0.05;

    u64 CPUStart = ReadCPUTimer();

    f64 now = ReadOSTimer();
    f64 then = now + estimate_time;
    while (now < then)
    {
        now = ReadOSTimer();
    }

    u64 CPUEnd = ReadCPUTimer();

    u64 ClocksPerSecond = (u64)((CPUEnd - CPUStart) / estimate_time);
    return ClocksPerSecond;
}

function void profiler__print_time_elapsed(Profile_Anchor *anchor, u64 TotalTSCElapsed)
{
    f64 Percent = 100.0 * ((f64)anchor->tsc_elapsed_exclusive / (f64)TotalTSCElapsed);
    print("  %s[%llu]: %llu (%.2f%%", anchor->label, anchor->hit_count, anchor->tsc_elapsed_exclusive, Percent);
    if(anchor->tsc_elapsed_inclusive != anchor->tsc_elapsed_exclusive)
    {
        f64 PercentWithChildren = 100.0 * ((f64)anchor->tsc_elapsed_inclusive / (f64)TotalTSCElapsed);
        print(", %.2f%% w/children", PercentWithChildren);
    }
    print(")\n");
}

function void profiler__init()
{
    g_profiler.cpu_freq = profiler__estimate_clocks_per_second();
}

function void profiler__begin()
{
    if (!g_profiler.cpu_freq)
    {
        g_profiler.cpu_freq = profiler__estimate_clocks_per_second();
    }

    for (i64 index = 0; index < count_of(g_profiler.anchors); index += 1)
    {
        Profile_Anchor *anchor = g_profiler.anchors + index;
        if (!anchor->tsc_elapsed_inclusive) continue;

        Profile_Anchor *metric = g_profiler.metrics + index;

        if (!metric->label)
        {
            metric->tsc_elapsed_exclusive = anchor->tsc_elapsed_exclusive;
            metric->tsc_elapsed_inclusive = anchor->tsc_elapsed_inclusive;
            metric->label = anchor->label;
        }
        else
        {
            metric->tsc_elapsed_exclusive = metric->tsc_elapsed_exclusive * 0.9 + anchor->tsc_elapsed_exclusive * 0.1;
            metric->tsc_elapsed_inclusive = metric->tsc_elapsed_inclusive * 0.9 + anchor->tsc_elapsed_inclusive * 0.1;
        }

        metric->hit_count += anchor->hit_count;
        metric->frame_count += 1;
    }

    MemoryZero(g_profiler.anchors, sizeof(Profile_Anchor) * count_of(g_profiler.anchors));

    g_profiler.start_tsc = ReadCPUTimer();
}

function void profiler__end()
{
    g_profiler.end_tsc = ReadCPUTimer();
}

function void profiler__print()
{
    u64 CPUFreq = g_profiler.cpu_freq;
    u64 TotalCPUElapsed = g_profiler.end_tsc - g_profiler.start_tsc;

    if (CPUFreq)
    {
        print("Total time: %0.4fms (CPU time: %llu)\n", 1000.0 * (f64)TotalCPUElapsed / (f64)CPUFreq, TotalCPUElapsed);
        print("CPU freq: %llu\n", CPUFreq);
    }

    for (u32 index = 0; index < count_of(g_profiler.anchors); index += 1)
    {
        Profile_Anchor *anchor = g_profiler.anchors + index;
        if (anchor->tsc_elapsed_inclusive)
        {
            profiler__print_time_elapsed(anchor, TotalCPUElapsed);
        }
    }
}

#else // PROFILER

#define TimeBlock(Name)
#define TimeFunction

#define profiler__begin()
#define profiler__end()
#define profiler__print()

#endif // PROFILER

