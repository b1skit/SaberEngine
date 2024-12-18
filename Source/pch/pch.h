// © 2022 Adam Badke. All rights reserved.
#pragma once

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
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <latch>
#include <map>
#include <mutex>
#include <numbers>
#include <queue>
#include <set>
#include <shared_mutex>
#include <stack>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>


// Win32 API:
#define WIN32_LEAN_AND_MEAN // Limit the number of header files included via Windows.h
#include <Windows.h>
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
#include <glm/gtx/string_cast.hpp>


// EnTT:
#define ENTT_USE_ATOMIC
#include <entt/entity/registry.hpp>


// Macros:
#define ENUM_TO_STR(x) #x