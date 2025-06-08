# Debug Markers and External Tool Compatibility

This document describes fixes applied to address crashes when using external debugging tools (PIX, NSight Graphics) to replay frame captures in DX12 mode.

## Issues Addressed

### 1. PIX Marker Version Compatibility
**Problem**: The PIX v1 markers (default) can cause instability when external debugging tools attempt to replay frame captures.

**Solution**: Enabled PIX v2 markers by defining `PIX_USE_GPU_MARKERS_V2` in `Source/Core/ProfilingMarkers.h`. PIX v2 markers provide:
- Better stability during frame capture replay
- Improved integration with external debugging tools
- More robust error handling

### 2. Null Pointer Safety
**Problem**: Potential crashes if null pointers are passed to PIX marker functions.

**Solution**: Added null pointer checks in the `SEBeginGPUEvent` and `SEEndGPUEvent` macros to prevent crashes when invalid API objects are passed.

### 3. Emergency Fallback Option
**Problem**: If PIX markers continue to cause issues with specific external tools or configurations.

**Solution**: Added a compile-time option `DISABLE_PIX_MARKERS_FOR_EXTERNAL_TOOLS` that completely disables all PIX markers while preserving the API. This can be enabled by uncommenting the define in `ProfilingMarkers.h`.

## Debugging Steps

If external debugging tools still crash when replaying frame captures:

1. **First, try PIX v2 markers** (already enabled):
   - Test with PIX for Windows
   - Test with NSight Graphics
   - Check if frame captures replay without crashes

2. **If issues persist, disable PIX markers**:
   - Uncomment `#define DISABLE_PIX_MARKERS_FOR_EXTERNAL_TOOLS` in `Source/Core/ProfilingMarkers.h`
   - Rebuild the project
   - Test frame capture replay again

3. **Check D3D12 debug layer interaction**:
   - Try running with different debug levels (`-debuglevel 0`, `-debuglevel 1`, `-debuglevel 2`)
   - GPU-based validation (`-debuglevel 2`) can sometimes interfere with external tools
   - Consider using `-debuglevel 1` when working with external debugging tools

4. **Verify external tool versions**:
   - Ensure PIX for Windows is up to date
   - Ensure NSight Graphics is up to date
   - Check tool documentation for known compatibility issues

## Code Changes Made

### Source/Core/ProfilingMarkers.h
- Enabled `PIX_USE_GPU_MARKERS_V2` for better external tool compatibility
- Added null pointer safety checks in debug marker macros
- Added `DISABLE_PIX_MARKERS_FOR_EXTERNAL_TOOLS` emergency fallback option
- Improved macro consistency between debug and release modes

## Additional Notes

- All existing debug marker usage patterns remain unchanged
- The changes are backward compatible
- Performance impact is minimal (only adds null checks in debug builds)
- Release builds are unaffected by these changes

## Testing

After applying these changes:
1. Build the project in Debug configuration
2. Run with external debugging tools
3. Capture frames and verify replay works without crashes
4. If issues persist, try the emergency fallback option