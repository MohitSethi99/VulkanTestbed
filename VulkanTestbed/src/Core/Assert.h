#pragma once

#include <filesystem>

#include "Log.h"

#ifdef TESTBED_DEBUG
#if defined(_WIN32)
#define TESTBED_DEBUGBREAK() __debugbreak()
#else
#error "Platform doesn't support debugbreak yet!"
#endif
#define TESTBED_ENABLE_ASSERTS
#else
#define TESTBED_DEBUGBREAK()
#endif

#define TESTBED_EXPAND_MACRO(x) x
#define TESTBED_STRINGIFY_MACRO(x) #x

#ifdef TESTBED_ENABLE_ASSERTS
// Alteratively we could use the same "default" message for both "WITH_MSG" and "NO_MSG" and
// provide support for custom formatting by concatenating the formatting string instead of having the format inside the default message
#define TESTBED_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { LOG_ERROR(msg, __VA_ARGS__); TESTBED_DEBUGBREAK(); } }
#define TESTBED_INTERNAL_ASSERT_WITH_MSG(type, check, ...) TESTBED_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
#define TESTBED_INTERNAL_ASSERT_NO_MSG(type, check) TESTBED_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", TESTBED_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

#define TESTBED_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
#define TESTBED_INTERNAL_ASSERT_GET_MACRO(...) TESTBED_EXPAND_MACRO( TESTBED_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, TESTBED_INTERNAL_ASSERT_WITH_MSG, TESTBED_INTERNAL_ASSERT_NO_MSG) )

// Currently accepts at least the condition and one additional parameter (the message) being optional
#define TESTBED_ASSERT(...) TESTBED_EXPAND_MACRO( TESTBED_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
#else
#define TESTBED_ASSERT(...)
#endif
