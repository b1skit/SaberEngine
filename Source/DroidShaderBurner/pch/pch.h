// � 2024 Adam Badke. All rights reserved.
#pragma once

#define WIN32_LEAN_AND_MEAN // Limit the number of header files included via Windows.h
#ifndef NOMINMAX
#define NOMINMAX
#endif


// std library:
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <limits>
#include <queue>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

// Win32 API:
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <processthreadsapi.h>
#endif // defined(_WIN32) || defined(_WIN64)

// nlohmann-json:
#if defined(_DEBUG)
// Enable extended diagnostics in debug configurations: https://json.nlohmann.me/api/macros/json_diagnostics/
#define JSON_DIAGNOSTICS 1
#else
#define JSON_DIAGNOSTICS 0
#endif
#include <nlohmann/json.hpp>