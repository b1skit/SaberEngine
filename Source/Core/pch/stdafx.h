// © 2024 Adam Badke. All rights reserved.
#pragma once

// ImGui
// Supress error C4996 ("This function or variable may be unsafe"), e.g. 'sscanf', 'strcpy', 'strcat', 'sscanf'
// Note: This block needs to come before the std includes
#define _CRT_SECURE_NO_WARNINGS
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#undef _CRT_SECURE_NO_WARNINGS

#include <any>
#include <array>
#include <barrier>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <latch>
#include <queue>
#include <regex>
#include <shared_mutex>


// Win32 API:
#define WIN32_LEAN_AND_MEAN // Limit the number of header files included via Windows.h
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shlobj_core.h> // Windows shell
// TODO: Move these OS-specific out of the PCH and into platform-specific files that require them


// Macros:
#define ENUM_TO_STR(x) #x