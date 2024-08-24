// © 2024 Adam Badke. All rights reserved.
#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <unordered_map>
#include <vector>


// nlohmann-json:
#if defined(_DEBUG)
// Enable extended diagnostics in debug configurations: https://json.nlohmann.me/api/macros/json_diagnostics/
#define JSON_DIAGNOSTICS 1
#else
#define JSON_DIAGNOSTICS 0
#endif
#include <nlohmann/json.hpp>