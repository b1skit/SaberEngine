# DX12 Memory Leak Fix - Test Plan

## Overview
This document describes the fix for the DX12 memory leak issue where descriptor caches grew indefinitely.

## Root Cause
The `DescriptorCache` in `DescriptorCache_DX12.cpp` was only freeing descriptors at shutdown, causing unbounded growth during runtime when many unique TextureView/BufferView combinations were used.

## Fix Summary
Added age-based periodic cleanup to descriptor caches:

1. **Age Tracking**: Each cache entry tracks when it was last used (`m_lastUsedFrame`)
2. **Periodic Cleanup**: Every 300 frames (~5s), unused descriptors older than 1800 frames (~30s) are freed
3. **Automatic**: Cleanup happens during normal `GetCreateDescriptor()` calls
4. **Performance Friendly**: Frequently used descriptors are retained, cleanup is infrequent

## Testing the Fix

### Before Fix (Expected Issues)
- Process memory usage would grow continuously in DX12 mode
- Memory usage would be stable in OpenGL mode
- Descriptor caches would accumulate thousands of entries over time

### After Fix (Expected Behavior) 
- Memory usage should stabilize in DX12 mode after initial warmup
- Old unused descriptors should be periodically freed
- Frequently used descriptors should remain cached for performance

### Test Steps
1. Run SaberEngine in DX12 mode: `SaberEngine.exe -platform dx12`
2. Load/reload scenes with many different textures/materials
3. Monitor process memory usage over time (should stabilize)
4. Compare with OpenGL mode: `SaberEngine.exe -platform opengl` 
5. Verify DX12 and OpenGL memory patterns are similar

### Verification Points
- [ ] Memory usage stabilizes after initial scene loading
- [ ] No continuous memory growth during scene changes
- [ ] Performance remains good (descriptor reuse works)
- [ ] No crashes or rendering issues
- [ ] Debug logs show periodic cleanup (in debug builds)

## Implementation Details

### Files Changed
- `DescriptorCache_DX12.h`: Added cleanup infrastructure
- `DescriptorCache_DX12.cpp`: Implemented cleanup logic

### Key Parameters
```cpp
static constexpr uint64_t k_cacheCleanupFrameInterval = 300; // ~5s at 60fps
static constexpr uint64_t k_maxUnusedFrames = 1800;         // ~30s at 60fps
```

These can be adjusted if needed for different cleanup behavior.

### Thread Safety
All operations remain thread-safe using the existing `m_descriptorCacheMutex`.

## Expected Impact
- **Memory leak fixed**: Prevents unbounded descriptor cache growth
- **Performance maintained**: Frequently used descriptors stay cached
- **Minimal overhead**: Cleanup only every 300 frames
- **No breaking changes**: External APIs unchanged