// Indycompiled: force-included into every C file of the clang/MinGW build (PROJECT.md §5.3).
// MSVC's headers pull these in implicitly; upstream code relies on that (static_assert, FLT_MAX, ERANGE, EDOM).
#pragma once
#include <assert.h>
#include <errno.h>
#include <float.h>
