///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/time.cpp
// Purpose:     Implementation of time-related functions.
// Author:      Vadim Zeitlin
// Created:     2011-11-26
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

// This is a horrible hack which only works because we don't currently include
// <time.h> from wx/wxprec.h. It is needed because we need timezone-related
// stuff from MinGW time.h, but it is not compiled in strict ANSI mode and it
// is too complicated to be dealt with using wxDECL_FOR_STRICT_MINGW32(). So we
// just include the header after undefining __STRICT_ANSI__ to get all the
// declarations we need -- and then define it back to avoid inconsistencies in
// all our other headers.
//
// Note that the same hack is used for "environ" in utilscmn.cpp, so if the
// code here is modified because this hack becomes unnecessary or a better
// solution is found, the code there should be updated as well.
#ifdef wxNEEDS_STRICT_ANSI_WORKAROUNDS
    #undef __STRICT_ANSI__
    #include <time.h>
    #define __STRICT_ANSI__
#endif

#include "wx/time.h"

#ifndef WX_PRECOMP
    #ifdef __WINDOWS__
        #include "wx/msw/wrapwin.h"
    #endif
#endif

#ifndef WX_GMTOFF_IN_TM
    // Define it for some systems which don't (always) use configure but are
    // known to have tm_gmtoff field.
    #if defined(__DARWIN__)
        #define WX_GMTOFF_IN_TM
    #endif
#endif

#include <time.h>

#if !defined(__WXMAC__)
    #include <sys/types.h>      // for time_t
#endif

#if defined(HAVE_GETTIMEOFDAY)
    #include <sys/time.h>
    #include <unistd.h>
#elif defined(HAVE_FTIME)
    #include <sys/timeb.h>
#endif

namespace
{

const int MILLISECONDS_PER_SECOND = 1000;
const int MICROSECONDS_PER_MILLISECOND = 1000;
const int MICROSECONDS_PER_SECOND = 1000*1000;

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

#if (!defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)) && wxUSE_THREADS && !defined(__WINDOWS__)
static wxMutex timeLock;
#endif

#ifndef HAVE_LOCALTIME_R
struct tm *wxLocaltime_r(const time_t* ticks, struct tm* temp)
{
#if wxUSE_THREADS && !defined(__WINDOWS__)
  // No need to waste time with a mutex on windows since it's using
  // thread local storage for localtime anyway.
  wxMutexLocker locker(timeLock);
#endif

  const tm * const t = localtime(ticks);
  if ( !t )
      return NULL;

  memcpy(temp, t, sizeof(struct tm));
  return temp;
}
#endif // !HAVE_LOCALTIME_R

#ifndef HAVE_GMTIME_R
struct tm *wxGmtime_r(const time_t* ticks, struct tm* temp)
{
#if wxUSE_THREADS && !defined(__WINDOWS__)
  // No need to waste time with a mutex on windows since it's
  // using thread local storage for gmtime anyway.
  wxMutexLocker locker(timeLock);
#endif

  const tm * const t = gmtime(ticks);
  if ( !t )
      return NULL;

  memcpy(temp, gmtime(ticks), sizeof(struct tm));
  return temp;
}
#endif // !HAVE_GMTIME_R

// returns the time zone in the C sense, i.e. the difference UTC - local
// (in seconds)
int wxGetTimeZone()
{
#ifdef WX_GMTOFF_IN_TM
    // set to true when the timezone is set
    static bool s_timezoneSet = false;
    static long gmtoffset = LONG_MAX; // invalid timezone

    // ensure that the timezone variable is set by calling wxLocaltime_r
    if ( !s_timezoneSet )
    {
        // just call wxLocaltime_r() instead of figuring out whether this
        // system supports tzset(), _tzset() or something else
        time_t t = time(NULL);
        struct tm tm;

        wxLocaltime_r(&t, &tm);
        s_timezoneSet = true;

        // note that GMT offset is the opposite of time zone and so to return
        // consistent results in both WX_GMTOFF_IN_TM and !WX_GMTOFF_IN_TM
        // cases we have to negate it
        gmtoffset = -tm.tm_gmtoff;

        // this function is supposed to return the same value whether DST is
        // enabled or not, so we need to use an additional offset if DST is on
        // as tm_gmtoff already does include it
        if ( tm.tm_isdst )
            gmtoffset += 3600;
    }
    return (int)gmtoffset;
#elif defined(__VISUALC__)
    // We must initialize the time zone information before using it (this will
    // be done only once internally).
    _tzset();

    // Starting with VC++ 8 timezone variable is deprecated and is not even
    // available in some standard library version so use the new function for
    // accessing it instead.
    #if wxCHECK_VISUALC_VERSION(8)
        long t;
        _get_timezone(&t);
        return t;
    #else // VC++ < 8
        return timezone;
    #endif
#else // Use some kind of time zone variable.
    // In any case we must initialize the time zone before using it.
    tzset();

    #if defined(WX_TIMEZONE) // If WX_TIMEZONE was defined by configure, use it.
        return WX_TIMEZONE;
    #elif defined(__MINGW32__)
        return _timezone;
    #else // unknown platform -- assume it has timezone
        return timezone;
    #endif // different time zone variables
#endif // different ways to determine time zone
}

// Get UTC time as seconds since 00:00:00, Jan 1st 1970
long wxGetUTCTime()
{
    return (long)time(NULL);
}

#if wxUSE_LONGLONG

wxLongLong wxGetUTCTimeUSec()
{
#if defined(__WINDOWS__)
    FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);

    // FILETIME is in 100ns or 0.1us since 1601-01-01, transform to us since
    // 1970-01-01.
    wxLongLong t(ft.dwHighDateTime, ft.dwLowDateTime);
    t /= 10;
    t -= wxLL(11644473600000000); // Unix - Windows epochs difference in us.
    return t;
#else // non-MSW

#ifdef HAVE_GETTIMEOFDAY
    timeval tv;
    if ( wxGetTimeOfDay(&tv) != -1 )
    {
        wxLongLong val(tv.tv_sec);
        val *= MICROSECONDS_PER_SECOND;
        val += tv.tv_usec;
        return val;
    }
#endif // HAVE_GETTIMEOFDAY

    // Fall back to lesser precision function.
    return wxGetUTCTimeMillis()*MICROSECONDS_PER_MILLISECOND;
#endif // MSW/!MSW
}

// Get local time as milliseconds since 00:00:00, Jan 1st 1970
wxLongLong wxGetUTCTimeMillis()
{
    // If possible, use a function which avoids conversions from
    // broken-up time structures to milliseconds
#if defined(__WINDOWS__)
    FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);

    // FILETIME is expressed in 100ns (or 0.1us) units since 1601-01-01,
    // transform them to ms since 1970-01-01.
    wxLongLong t(ft.dwHighDateTime, ft.dwLowDateTime);
    t /= 10000;
    t -= wxLL(11644473600000); // Unix - Windows epochs difference in ms.
    return t;
#else // !__WINDOWS__
    wxLongLong val = MILLISECONDS_PER_SECOND;

#if defined(HAVE_GETTIMEOFDAY)
    struct timeval tp;
    if ( wxGetTimeOfDay(&tp) != -1 )
    {
        val *= tp.tv_sec;
        return (val + (tp.tv_usec / MICROSECONDS_PER_MILLISECOND));
    }
    return 0;
#elif defined(HAVE_FTIME)
    struct timeb tp;

    // ftime() is void and not int in some mingw32 headers, so don't
    // test the return code (well, it shouldn't fail anyhow...)
    (void)::ftime(&tp);
    val *= tp.time;
    return (val + tp.millitm);
#else // no gettimeofday() nor ftime()
    // If your platform/compiler does not support ms resolution please
    // do NOT just shut off these warnings, drop me a line instead at
    // <guille@iies.es>

    #if defined(__VISUALC__)
        #pragma message("wxStopWatch will be up to second resolution!")
    #else
        #warning "wxStopWatch will be up to second resolution!"
    #endif // compiler

    val *= wxGetUTCTime();
    return val;
#endif // time functions

#endif // __WINDOWS__/!__WINDOWS__
}

wxLongLong wxGetLocalTimeMillis()
{
    return wxGetUTCTimeMillis() - wxGetTimeZone()*MILLISECONDS_PER_SECOND;
}

#else // !wxUSE_LONGLONG

double wxGetLocalTimeMillis(void)
{
    return (double(clock()) / double(CLOCKS_PER_SEC)) * 1000.0;
}

#endif // wxUSE_LONGLONG/!wxUSE_LONGLONG
