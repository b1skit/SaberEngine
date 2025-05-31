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
#include <barrier>
#include <fstream>
#include <future>
#include <iostream>
#include <latch>
#include <limits>
#include <numbers>
#include <queue>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>


// Win32 API:
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <comdef.h> // HRESULTs to error messages
#endif // defined(_WIN32) || defined(_WIN64)


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
#include <glm/gtx/string_cast.hpp>


// EnTT:
#define ENTT_USE_ATOMIC
#include <entt/entity/registry.hpp>


// nlohmann-json:
#if defined(_DEBUG)
// Enable extended diagnostics in debug configurations: https://json.nlohmann.me/api/macros/json_diagnostics/
#define JSON_DIAGNOSTICS 1
#else
#define JSON_DIAGNOSTICS 0
#endif
#include <nlohmann/json.hpp>


// Macros:
#define ENUM_TO_STR(x) #x