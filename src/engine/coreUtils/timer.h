#ifndef TIMER
#define TIMER

#include <chrono>
// no nesting, but makes for a very simple interface
	// could probably do something stack based, have Tick() push and Tock() pop
#define NOW std::chrono::high_resolution_clock::now()

// option to report ms/us
#define USE_MS

// if not milliseconds, do microseconds
#ifndef USE_MS
#define USE_US
#endif

#ifdef USE_MS
#define TIMECAST(x) std::chrono::duration_cast<std::chrono::microseconds>(x).count()/1000.0f
#define TIMEUNIT " ms"
#endif

#ifdef USE_US
#define TIMECAST(x) std::chrono::duration_cast<std::chrono::microseconds>(x).count()
#define TIMEUNIT " us"
#endif

static auto tInit = NOW;
static auto t = NOW;

// set base time
static inline void Tick () { t = NOW; }
// get difference between base time and current time, return value in useconds
static inline float Tock () { return TIMECAST( NOW - t ); }
// getting the time since the engine was started
static inline float TotalTime () { return TIMECAST( NOW - tInit ); }

#endif
