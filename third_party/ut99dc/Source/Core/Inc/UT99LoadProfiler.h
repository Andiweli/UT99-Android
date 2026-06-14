/*=============================================================================
	UT99LoadProfiler.h: Lightweight load-time profiling helpers.
	Android logcat tag: UT99_LOADPROF
=============================================================================*/

#ifndef UT99_LOAD_PROFILER_H
#define UT99_LOAD_PROFILER_H

#include <stdio.h>

/*
 * UE1 defines a function-like macro named clock(Timer) in UnFile.h.
 * Android/Bionic's <time.h> declares clock_t clock(void), so including <time.h>
 * while that macro is active breaks the build.  Temporarily hide the UE macro,
 * include the system header, then restore the original UE helper macro.
 */
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(PLATFORM_VITA) || defined(PLATFORM_PSVITA)
#ifdef clock
#define UT99_LOADPROF_RESTORE_UE_CLOCK_MACRO 1
#undef clock
#endif
#include <time.h>
#ifdef UT99_LOADPROF_RESTORE_UE_CLOCK_MACRO
#define clock(Timer)   {Timer -= appCycles();}
#undef UT99_LOADPROF_RESTORE_UE_CLOCK_MACRO
#endif
#endif

#ifdef PLATFORM_ANDROID
#include <android/log.h>
#endif

static inline DOUBLE UT99LoadProfSeconds()
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(PLATFORM_VITA) || defined(PLATFORM_PSVITA)
	struct timespec Ts;
	clock_gettime(CLOCK_MONOTONIC, &Ts);
	return (DOUBLE)Ts.tv_sec + ((DOUBLE)Ts.tv_nsec / 1000000000.0);
#else
	return appSeconds();
#endif
}

#ifdef PLATFORM_ANDROID
#define UT99_LOADPROF_LOG(Fmt, ...) \
	do { __android_log_print(ANDROID_LOG_INFO, "UT99_LOADPROF", Fmt, ##__VA_ARGS__); } while(0)
#else
#define UT99_LOADPROF_LOG(Fmt, ...) \
	do { printf("UT99_LOADPROF " Fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)
#endif

#define UT99_LOADPROF_MS(StartSeconds) ((UT99LoadProfSeconds() - (StartSeconds)) * 1000.0)

#endif // UT99_LOAD_PROFILER_H
