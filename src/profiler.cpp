#pragma once

#if PROFILER

// TODO(nick): support other platforms
function f64 ReadOSTimer()
{
    #if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200

        #ifndef CLOCK_MONOTONIC_RAW
            #error "CLOCK_MONOTONIC_RAW not found. Please verify that <time.h> is included from the MacOSX SDK rather than /usr/local/include"
        #endif

        static f64 macos_initial_clock = 0;
        if (!macos_initial_clock)
        {
            macos_initial_clock = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
        }

        return (f64)(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - macos_initial_clock) / (f64)(1e9);
    #else

    static f64 macos_perf_frequency = 0;
    static f64 macos_perf_counter = 0;

    if (macos_perf_frequency == 0)
    {
        mach_timebase_info_data_t rate_nsec;
        mach_timebase_info(&rate_nsec);

        macos_perf_frequency = 1000000000LL * rate_nsec.numer / rate_nsec.denom;
        macos_perf_counter = mach_absolute_time();
    }

    f64 now = mach_absolute_time();
    return (now - macos_perf_counter) / macos_perf_frequency;

    #endif
}

function u64 ReadCPUTimer(void)
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
    #elif defined(__arm64__)
        u64 x;
        __asm__ volatile("mrs \t%0, cntvct_el0" : "=r"(x));
        return x;
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

function f64 profiler__seconds_from_clocks(u64 clocks)
{
    f64 result = 0;

    u64 cpu_freq = g_profiler.cpu_freq;
    if (cpu_freq != 0)
    {
        result = (f64)clocks / (f64)cpu_freq;
    }

    return result;
}

function void profiler__print_time_elapsed(Profile_Anchor *anchor, u64 TotalTSCElapsed)
{
    f64 Percent = 100.0 * ((f64)anchor->tsc_elapsed_exclusive / (f64)TotalTSCElapsed);
    f64 Time    = 1000.0 * profiler__seconds_from_clocks(anchor->tsc_elapsed_exclusive);
    print("  %s[%llu]: %llu|%.2fms (%.2f%%", anchor->label, anchor->hit_count, anchor->tsc_elapsed_exclusive, Time, Percent);
    if(anchor->tsc_elapsed_inclusive != anchor->tsc_elapsed_exclusive)
    {
        f64 PercentWithChildren = 100.0 * ((f64)anchor->tsc_elapsed_inclusive / (f64)TotalTSCElapsed);
        f64 TimeWithChildren    = 1000.0 * profiler__seconds_from_clocks(anchor->tsc_elapsed_inclusive);
        print(", %.2fms|%.2f%% w/children", TimeWithChildren, PercentWithChildren);
    }
    print(")\n");
}

function void profiler__init()
{
    static b32 initted = false;
    if (initted) return;
    initted = true;

    g_profiler.cpu_freq = profiler__estimate_clocks_per_second();
}

function void profiler__begin()
{
    profiler__init();

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

