// © 2024 Adam Badke. All rights reserved.
#pragma once

#define WIN32_LEAN_AND_MEAN // Limit the number of header files included via Windows.h
#ifndef NOMINMAX
#define NOMINMAX
#endif


// ImGui
// Supress error C4996 ("This function or variable may be unsafe"), e.g. 'sscanf', 'strcpy', 'strcat', 'sscanf'
// Note: This block needs to come before the std includes
#define _CRT_SECURE_NO_WARNINGS
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#undef _CRT_SECURE_NO_WARNINGS


// std library:
#include <any>
#include <array>
#include <barrier>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <latch>
#include <limits>
#include <queue>
#include <regex>
#include <string_view>
#include <unordered_set>
#include <shared_mutex>
#include <typeindex>
#include <variant>


// Win32 API:
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <hidusage.h>
#include <shlobj_core.h> // Windows shell
#include <shellapi.h>
#include <shellscalingapi.h>
#include <oleidl.h>
#include <shobjidl.h>
#endif // defined(_WIN32) || defined(_WIN64)


// Macros:
#define ENUM_TO_STR(x) #x