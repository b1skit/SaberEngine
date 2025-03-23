// © 2024 Adam Badke. All rights reserved.
#pragma once

// std library:
#include <algorithm>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <future>
#include <queue>
#include <regex>
#include <set>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

// Win32 API:
#define WIN32_LEAN_AND_MEAN // Limit the number of header files included via Windows.h
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <processthreadsapi.h>
// TODO: Move these OS-specific out of the PCH and into platform-specific files that require them

// nlohmann-json:
#if defined(_DEBUG)
// Enable extended diagnostics in debug configurations: https://json.nlohmann.me/api/macros/json_diagnostics/
#define JSON_DIAGNOSTICS 1
#else
#define JSON_DIAGNOSTICS 0
#endif
#include <nlohmann/json.hpp>