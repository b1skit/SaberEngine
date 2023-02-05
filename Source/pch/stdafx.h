// © 2022 Adam Badke. All rights reserved.
#pragma once

// ImGui
// Supress error C4996 ("This function or variable may be unsafe"), e.g. 'sscanf', 'strcpy', 'strcat', 'sscanf'
// Note: This block needs to come before the std includes
#define _CRT_SECURE_NO_WARNINGS
#include "imgui.h"
#undef _CRT_SECURE_NO_WARNINGS


// std library:
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING // codecvt is deprecated. TODO: Handle wstring -> string better

#include <any>
#include <array>
#include <barrier>
#include <cassert>
#include <chrono>
#include <codecvt>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <latch>
#include <memory>
#include <mutex>
#include <numbers>
#include <queue>
#include <regex>
#include <shared_mutex>
#include <sstream>
#include <stack>
#include <stdarg.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>


// Win32 API:
#define WIN32_LEAN_AND_MEAN // Limit the number of header files included via Windows.h
#include <Windows.h>
#include <shellapi.h>
#include <hidusage.h>
// TODO: Move these OS-specific out of the PCH and into platform-specific files that require them


// GLM:
//#define GLM_FORCE_MESSAGES // View compilation/configuration details. Currently:
//1 > GLM: GLM_FORCE_DEPTH_ZERO_TO_ONE is defined.Using zero to one depth clip space.
//1 > GLM: GLM_FORCE_LEFT_HANDED is undefined.Using right handed coordinate system.

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_SWIZZLE // Enable swizzle operators
#define GLM_ENABLE_EXPERIMENTAL // Recommended for common.hpp
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/common.hpp>	// fmod
#include <glm/gtx/matrix_decompose.hpp>


// Macros:
#define ENUM_TO_STR(x) #x