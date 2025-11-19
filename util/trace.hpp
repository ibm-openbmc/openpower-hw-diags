#pragma once

#include <inttypes.h>
#include <stdio.h>

#include <phosphor-logging/lg2.hpp>

#include <cstdarg>

#ifndef TEST_TRACE
#include <phosphor-logging/log.hpp>
#endif

namespace trace
{

/** @brief Information trace (va_list format). */
inline void inf(const char* format, va_list args)
{
#ifdef TEST_TRACE

    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");

#else

    int sz = vsnprintf(nullptr, 0, format, args); // hack to get required size
    char* msg = new char[sz + 1]();   // allocate room for terminating character
    vsnprintf(msg, sz, format, args); // print the message
    phosphor::logging::log<phosphor::logging::level::INFO>(msg);
    delete[] msg;                     // clean up

#endif
}

/** @brief Error trace (va_list format). */
inline void err(const char* format, va_list args)
{
#ifdef TEST_TRACE

    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

#else

    int sz = vsnprintf(nullptr, 0, format, args); // hack to get required size
    char* msg = new char[sz + 1]();   // allocate room for terminating character
    vsnprintf(msg, sz, format, args); // print the message
    phosphor::logging::log<phosphor::logging::level::ERR>(msg);
    delete[] msg;                     // clean up

#endif
}

/** @brief Information trace (printf format). */
inline void inf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    trace::inf(format, args);
    va_end(args);
}

/** @brief Error trace (printf format). */
inline void err(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    trace::err(format, args);
    va_end(args);
}

} // namespace trace
