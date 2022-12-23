#pragma once

// ImGui
// Supress error C4996 ("This function or variable may be unsafe"), e.g. 'sscanf', 'strcpy', 'strcat', 'sscanf'
// Note: This block needs to come before the std includes
#define _CRT_SECURE_NO_WARNINGS
#include "imgui.h"
#undef _CRT_SECURE_NO_WARNINGS


// std library:
#include <any>
#include <array>
#include <assert.h>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <numbers>
#include <queue>
#include <regex>
#include <shared_mutex>
#include <sstream>
#include <stack>
#include <stdarg.h>
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>


// GLM:
//#define GLM_FORCE_MESSAGES // View compilation/configuration details. Currently:
//		GLM: GLM_FORCE_DEPTH_ZERO_TO_ONE is undefined.Using negative one to one depth clip space.
//		GLM: GLM_FORCE_LEFT_HANDED is undefined.Using right handed coordinate system.

#define GLM_FORCE_SWIZZLE // Enable swizzle operators
#define GLM_ENABLE_EXPERIMENTAL // Recommended for common.hpp
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/common.hpp>	// fmod
#include <glm/gtx/matrix_decompose.hpp>