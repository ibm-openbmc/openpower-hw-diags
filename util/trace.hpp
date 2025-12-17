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
inline void inf(const char* format, va_list args1)
{
    // Need to make a copy of the given va_list because we'll be iterating the
    // list twice with the vnsprintf() calls below.
    va_list args2;
    va_copy(args2, args1);

    int sz = vsnprintf(nullptr, 0, format, args1); // hack to get required size
    char* msg = new char[sz + 1](); // allocate room for terminating character
    vsnprintf(msg, sz + 1, format, args2); // print the message

#ifdef TEST_TRACE
    fprintf(stdout, "%s\n", msg);
#else
    phosphor::logging::log<phosphor::logging::level::INFO>(msg);
#endif

    delete[] msg; // clean up
}

/** @brief Error trace (va_list format). */
inline void err(const char* format, va_list args1)
{
    // Need to make a copy of the given va_list because we'll be iterating the
    // list twice with the vnsprintf() calls below.
    va_list args2;
    va_copy(args2, args1);

    int sz = vsnprintf(nullptr, 0, format, args1); // hack to get required size
    char* msg = new char[sz + 1](); // allocate room for terminating character
    vsnprintf(msg, sz + 1, format, args2); // print the message

#ifdef TEST_TRACE
    fprintf(stderr, "%s\n", msg);
#else
    phosphor::logging::log<phosphor::logging::level::ERR>(msg);
#endif

    delete[] msg; // clean up
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
