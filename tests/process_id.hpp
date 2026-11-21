#pragma once
#ifdef _WIN32
#include <process.h>
inline int jsdTestProcessId() { return ::_getpid(); }
#else
#include <unistd.h>
inline int jsdTestProcessId() { return ::getpid(); }
#endif
