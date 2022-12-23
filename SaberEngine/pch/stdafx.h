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