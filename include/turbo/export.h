// SPDX-License-Identifier: Apache-2.0
#pragma once
#if defined(_WIN32) && defined(TURBO_SHARED)
#if defined(TURBO_BUILDING_LIBRARY)
#define TURBO_API __declspec(dllexport)
#else
#define TURBO_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define TURBO_API __attribute__((visibility("default")))
#else
#define TURBO_API
#endif
