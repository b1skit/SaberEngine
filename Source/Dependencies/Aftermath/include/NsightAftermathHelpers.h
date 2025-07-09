//*********************************************************
//
// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
// 
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
// 
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
// 
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//
//*********************************************************

#pragma once

#include "GFSDK_Aftermath.h"
#include "GFSDK_Aftermath_GpuCrashDump.h"
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"

//*********************************************************
// Some std::to_string overloads for some Nsight Aftermath
// API types.
//

namespace std
{
    template<typename T>
    inline std::string to_hex_string(T n)
    {
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(2 * sizeof(T)) << std::hex << n;
        return stream.str();
    }

    inline std::string to_string(GFSDK_Aftermath_Result result)
    {
        return std::string("0x") + to_hex_string(static_cast<UINT>(result));
    }

    inline std::string to_string(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier)
    {
        return to_hex_string(identifier.id[0]) + "-" + to_hex_string(identifier.id[1]);
    }

    inline std::string to_string(const GFSDK_Aftermath_ShaderBinaryHash& hash)
    {
        return to_hex_string(hash.hash);
    }
} // namespace std

//*********************************************************
// Helper for comparing shader hashes and debug info identifier.
//

// Helper for comparing GFSDK_Aftermath_ShaderDebugInfoIdentifier.
inline bool operator<(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& lhs, const GFSDK_Aftermath_ShaderDebugInfoIdentifier& rhs)
{
    if (lhs.id[0] == rhs.id[0])
    {
        return lhs.id[1] < rhs.id[1];
    }
    return lhs.id[0] < rhs.id[0];
}

// Helper for comparing GFSDK_Aftermath_ShaderBinaryHash.
inline bool operator<(const GFSDK_Aftermath_ShaderBinaryHash& lhs, const GFSDK_Aftermath_ShaderBinaryHash& rhs)
{
    return lhs.hash < rhs.hash;
}

// Helper for comparing GFSDK_Aftermath_ShaderDebugName.
inline bool operator<(const GFSDK_Aftermath_ShaderDebugName& lhs, const GFSDK_Aftermath_ShaderDebugName& rhs)
{
    return strncmp(lhs.name, rhs.name, sizeof(lhs.name)) < 0;
}

//*********************************************************
// Helper for checking Nsight Aftermath failures.
//

// Exception for Nsight Aftermath failures
class AftermathException : public std::runtime_error
{
public:
    AftermathException(GFSDK_Aftermath_Result result)
        : std::runtime_error(GetErrorMessage(result))
        , m_result(result)
    {
    }

    AftermathException Error() const
    {
        return m_result;
    }

    static std::string  GetErrorMessage(GFSDK_Aftermath_Result result)
    {
        switch (result)
        {
        case GFSDK_Aftermath_Result_FAIL_VersionMismatch:
            return "Aftermath version mismatch between caller and library.";
        case GFSDK_Aftermath_Result_FAIL_NotInitialized:
            return "Aftermath library has not been initialized. Call GFSDK_Aftermath_DX*_Initialize first.";
        case GFSDK_Aftermath_Result_FAIL_InvalidAdapter:
            return "Invalid GPU adapter - Aftermath supports only NVIDIA GPUs.";
        case GFSDK_Aftermath_Result_FAIL_InvalidParameter:
            return "Invalid parameter passed to Aftermath - likely a null pointer or bad handle.";
        case GFSDK_Aftermath_Result_FAIL_Unknown:
            return "Unknown failure occurred inside Aftermath.";
        case GFSDK_Aftermath_Result_FAIL_ApiError:
            return "Graphics API call failed within Aftermath.";
        case GFSDK_Aftermath_Result_FAIL_NvApiIncompatible:
            return "Incompatible or outdated NvAPI DLL. Please update it.";
        case GFSDK_Aftermath_Result_FAIL_GettingContextDataWithNewCommandList:
            return "Attempted to get Aftermath context data before using event markers on the command list.";
        case GFSDK_Aftermath_Result_FAIL_AlreadyInitialized:
            return "Aftermath has already been initialized.";
        case GFSDK_Aftermath_Result_FAIL_D3DDebugLayerNotCompatible:
            return "A debug layer not compatible with Aftermath has been detected.";
        case GFSDK_Aftermath_Result_FAIL_DriverInitFailed:
            return "Aftermath failed to initialize in the graphics driver.";
        case GFSDK_Aftermath_Result_FAIL_DriverVersionNotSupported:
            return "Unsupported driver version - requires an NVIDIA R495 display driver or newer.";
        case GFSDK_Aftermath_Result_FAIL_OutOfMemory:
            return "System ran out of memory during Aftermath operation.";
        case GFSDK_Aftermath_Result_FAIL_GetDataOnBundle:
            return "Cannot get Aftermath data on bundles. Use the command list instead.";
        case GFSDK_Aftermath_Result_FAIL_GetDataOnDeferredContext:
            return "Cannot get Aftermath data on deferred contexts. Use the immediate context instead.";
        case GFSDK_Aftermath_Result_FAIL_FeatureNotEnabled:
            return "This Aftermath feature was not enabled during initialization. Check GFSDK_Aftermath_FeatureFlags.";
        case GFSDK_Aftermath_Result_FAIL_NoResourcesRegistered:
            return "No resources have been registered with Aftermath.";
        case GFSDK_Aftermath_Result_FAIL_ThisResourceNeverRegistered:
            return "The specified resource was never registered with Aftermath.";
        case GFSDK_Aftermath_Result_FAIL_NotSupportedInUWP:
            return "Aftermath functionality is not supported in UWP applications.";
        case GFSDK_Aftermath_Result_FAIL_D3dDllNotSupported:
            return "D3D DLL version is not compatible with Aftermath.";
        case GFSDK_Aftermath_Result_FAIL_D3dDllInterceptionNotSupported:
            return "Aftermath is incompatible with D3D API interception, such as PIX or Nsight Graphics.";
        case GFSDK_Aftermath_Result_FAIL_Disabled:
            return "Aftermath is disabled by system policy. Check registry or environment settings.";
        case GFSDK_Aftermath_Result_FAIL_NotSupportedOnContext:
            return "Markers cannot be set on queue or device contexts.";
        default:
            return "Aftermath Error 0x" + std::to_hex_string(result);
        }
    }

private:
    const GFSDK_Aftermath_Result m_result;
};

// Helper macro for checking Nsight Aftermath results and throwing exception
// in case of a failure.
#define AFTERMATH_CHECK_ERROR(FC)                                                                       \
[&]() {                                                                                                 \
    GFSDK_Aftermath_Result _result = FC;                                                                \
    if (!GFSDK_Aftermath_SUCCEED(_result))                                                              \
    {                                                                                                   \
        MessageBoxA(0, AftermathException::GetErrorMessage(_result).c_str(), "Aftermath Error", MB_OK); \
        throw AftermathException(_result);                                                              \
    }                                                                                                   \
}()
