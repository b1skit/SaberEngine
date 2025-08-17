# SaberEngine: Technical Deep Dive and Implementation Details

*Advanced technical analysis focusing on implementation specifics, performance optimizations, and innovative solutions.*

## Table of Contents

1. [Compile-Time Hash System](#compile-time-hash-system)
2. [Scriptable Graphics System Implementation](#scriptable-graphics-system-implementation)
3. [Multi-API Strategy Pattern Implementation](#multi-api-strategy-pattern-implementation)
4. [Ray Tracing Integration](#ray-tracing-integration)
5. [Entity Component System Details](#entity-component-system-details)
6. [Thread-Safe Design Patterns](#thread-safe-design-patterns)
7. [Memory Management Strategies](#memory-management-strategies)
8. [Shader System and Droid Compiler](#shader-system-and-droid-compiler)
9. [Performance Optimization Techniques](#performance-optimization-techniques)
10. [Advanced Rendering Techniques](#advanced-rendering-techniques)

## Compile-Time Hash System

SaberEngine implements a sophisticated compile-time string hashing system using the FNV-1a algorithm:

```cpp
class CHashKey final
{
public:
    consteval CHashKey(char const* keyStr)
        : m_key(keyStr)
        , m_keyHash(Fnv1A(keyStr))
    {
    }

private:
    constexpr uint64_t Fnv1A(uint64_t hash, const char* keyStr)
    {
        constexpr uint64_t k_fnvPrime = 1099511628211ull;
        return (*keyStr == 0) ? hash : Fnv1A((hash ^ static_cast<uint64_t>(*keyStr)) * k_fnvPrime, keyStr + 1);
    }
};
```

### Technical Benefits

1. **Zero Runtime Cost**: String hashes computed at compile time
2. **Cache-Friendly Lookups**: 64-bit integer comparisons instead of string comparisons
3. **Type Safety**: Hash keys are strongly typed, preventing accidental misuse
4. **Debugging Support**: Original string retained in debug builds for inspection

### Usage Pattern

The system enables efficient string-based lookups throughout the engine:

```cpp
static constexpr util::CHashKey k_depthInput = "SceneDepth";
static constexpr util::CHashKey k_aoOutput = "RTAOTex";

// Fast hash-based lookup instead of string comparison
bool hasInput = graphicsSystem->HasRegisteredTextureInput(k_depthInput);
```

## Scriptable Graphics System Implementation

### Self-Registering Factory Pattern

Graphics systems automatically register themselves using template metaprogramming:

```cpp
template<typename T>
class IScriptableGraphicsSystem
{
public:
    IScriptableGraphicsSystem() { static_cast<void*>(&s_isRegistered); }

private:
    static bool s_isRegistered;
};

template<typename T>
bool IScriptableGraphicsSystem<T>::s_isRegistered =
    GraphicsSystem::RegisterGS(T::GetScriptName(), gr::GraphicsSystem::Create<T>);
```

### Runtime Function Binding

Each graphics system exposes runtime-bindable functions through a flexible interface:

```cpp
class GBufferGraphicsSystem : 
    public GraphicsSystem, 
    public IScriptableGraphicsSystem<GBufferGraphicsSystem>
{
public:
    static constexpr char const* GetScriptName() { return "GBuffer"; }
    
    RuntimeBindings GetRuntimeBindings() override
    {
        RETURN_RUNTIME_BINDINGS
        (
            INIT_PIPELINE(INIT_PIPELINE_FN(GBufferGraphicsSystem, InitPipeline))
            PRE_RENDER(PRE_RENDER_FN(GBufferGraphicsSystem, PreRender))
        );
    }
};
```

### Dependency Resolution Algorithm

The engine automatically resolves dependencies between graphics systems:

1. **Parse JSON Pipeline**: Extract graphics system dependencies from JSON configuration
2. **Build Dependency Graph**: Create directed acyclic graph of system dependencies
3. **Topological Sort**: Compute execution order ensuring dependencies are satisfied
4. **Parallel Grouping**: Identify systems that can execute in parallel
5. **Thread Assignment**: Distribute parallel groups across worker threads

### GBuffer Implementation Example

The GBuffer system demonstrates the pattern used throughout the engine:

```cpp
enum GBufferTexIdx : uint8_t
{
    GBufferAlbedo       = 0,  // Base color
    GBufferWNormal      = 1,  // World-space normals
    GBufferRMAO         = 2,  // Roughness, Metallic, AO
    GBufferEmissive     = 3,  // Emissive lighting
    GBufferMatProp0     = 4,  // Material properties
    GBufferMaterialID   = 5,  // Material identification
    GBufferDepth        = 6,  // Depth buffer
};
```

This deferred rendering setup enables:
- **Bandwidth Optimization**: Efficient use of render target bandwidth
- **Material Flexibility**: Support for various material types
- **Debugging Support**: Individual buffer visualization
- **Forward Compatibility**: Easy addition of new material properties

## Multi-API Strategy Pattern Implementation

### Function Pointer Binding

SaberEngine uses runtime function pointer binding for API abstraction:

```cpp
namespace platform
{
    bool RegisterPlatformFunctions()
    {
        switch (core::Config::GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey))
        {
            case RenderingAPI::DX12:
                platform::Texture::Create = &dx12::Texture::Create;
                platform::Buffer::Create = &dx12::Buffer::Create;
                platform::SwapChain::Create = &dx12::SwapChain::Create;
                // ... hundreds of function bindings
                break;
                
            case RenderingAPI::OpenGL:
                platform::Texture::Create = &opengl::Texture::Create;
                platform::Buffer::Create = &opengl::Buffer::Create;
                platform::SwapChain::Create = &opengl::SwapChain::Create;
                // ... corresponding OpenGL implementations
                break;
        }
        return true;
    }
}
```

### API-Specific Implementation Strategy

Each graphics API maintains its own implementation namespace:

- **dx12**: DirectX 12 specific implementations
- **opengl**: OpenGL 4.6 specific implementations
- **vulkan**: Future Vulkan implementations

### Benefits of This Approach

1. **Zero Runtime Overhead**: Direct function calls, no virtual function dispatch
2. **Compile-Time Safety**: All implementations must match platform interface signatures
3. **Runtime Flexibility**: Graphics API selection at program startup
4. **Code Isolation**: API-specific code completely separated
5. **Easy Extension**: New APIs added without modifying existing code

## Ray Tracing Integration

### Ray-Traced Ambient Occlusion (RTAO)

SaberEngine demonstrates modern hardware ray tracing integration:

```cpp
class RTAOGraphicsSystem : 
    public GraphicsSystem, 
    public IScriptableGraphicsSystem<RTAOGraphicsSystem>
{
private:
    std::shared_ptr<re::AccelerationStructure> const* m_sceneTLAS;
    
    // RTAO parameters
    glm::vec2 m_tMinMax;     // Ray interval distance
    uint32_t m_rayCount;     // Rays per pixel
    uint8_t m_geometryInstanceMask; // Geometry filtering
};
```

### Acceleration Structure Management

The engine automatically manages ray tracing acceleration structures:

1. **Bottom Level (BLAS)**: Per-mesh geometry structures
2. **Top Level (TLAS)**: Scene-level instance structures
3. **Automatic Updates**: Dynamic geometry and transform updates
4. **Memory Management**: Efficient GPU memory allocation for structures

### Ray Tracing Pipeline Integration

RT systems integrate seamlessly with the rasterization pipeline:

```cpp
// RTAO reads from rasterized G-Buffer
static constexpr util::CHashKey k_depthInput = "SceneDepth";
static constexpr util::CHashKey k_wNormalInput = "SceneWNormal";
static constexpr util::CHashKey k_sceneTLASInput = "SceneTLAS";

// Outputs AO for use in lighting calculations
static constexpr util::CHashKey k_aoOutput = "RTAOTex";
```

## Entity Component System Details

### Thread-Safe EnTT Integration

SaberEngine wraps EnTT with thread-safe interfaces:

```cpp
class EntityManager
{
private:
    entt::basic_registry<entt::entity> m_registry;
    mutable std::recursive_mutex m_registeryMutex;

public:
    template<typename T>
    T* TryGetComponent(entt::entity entity)
    {
        std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
        return m_registry.try_get<T>(entity);
    }
};
```

### Component System Organization

The ECS implements several system categories:

#### Transform Systems
- **Hierarchical Transforms**: Parent-child relationships with automatic propagation
- **Animation Integration**: Skeletal and keyframe animation application
- **Bounds Calculation**: Automatic bounding volume computation

#### Rendering Systems
- **Mesh Primitive Components**: Geometry and material binding
- **Material Instance Components**: Per-instance material parameter overrides
- **Render Data Components**: Cached rendering state and optimization data

#### Animation Systems
- **Skinning Components**: Skeletal animation with bone matrices
- **Morph Target Components**: Vertex-level shape animation
- **Animation Controllers**: State machines for animation control

### Performance Optimizations

#### Component Storage
EnTT provides optimal memory layout:
- **Array of Structures (AoS) to Structure of Arrays (SoA)**: Automatic conversion for cache efficiency
- **Component Packing**: Dense storage without holes
- **Type Erasure**: Efficient storage of heterogeneous component types

#### Query Optimization
```cpp
template<typename T, typename... Args, typename Callback>
auto EntityManager::QueryRegistry(Callback&& callback) const
{
    std::unique_lock<std::recursive_mutex> lock(m_registryMutex);
    auto view = m_registry.view<T, Args...>();
    return std::forward<Callback>(callback)(view);
}
```

## Thread-Safe Design Patterns

### Work-Stealing Thread Pool

SaberEngine implements a sophisticated thread pool with work stealing:

```cpp
class ThreadPool
{
    template<typename FunctionType>
    static std::future<typename std::invoke_result<FunctionType>::type> EnqueueJob(FunctionType job)
    {
        typedef typename std::invoke_result<FunctionType>::type resultType;
        
        std::packaged_task<resultType()> packagedTask(std::move(job));
        std::future<resultType> taskFuture(packagedTask.get_future());
        
        {
            std::unique_lock<std::mutex> waitingLock(s_jobQueueMutex);
            s_jobQueue.push(std::move(packagedTask));
        }
        
        s_jobQueueCV.notify_one();
        return taskFuture;
    }
};
```

### Barrier Synchronization

The main game loop uses barriers for frame synchronization:

```cpp
void EngineApp::Run()
{
    while (m_isRunning)
    {
        // Main thread: Update game state
        UpdateGameLogic();
        
        // Wait for render thread to complete previous frame
        m_syncBarrier->arrive_and_wait();
        
        // Start next frame
        ++m_frameNum;
    }
}
```

### Lock-Free Data Structures

Where possible, the engine uses lock-free designs:

#### Double-Buffered Containers
```cpp
template<typename Key, typename Value>
class DoubleBufferUnorderedMap
{
    // Allows lock-free reads while writes occur to alternate buffer
    std::array<std::unordered_map<Key, Value>, 2> m_buffers;
    std::atomic<uint8_t> m_readIndex;
};
```

#### Thread-Safe Vectors
```cpp
template<typename T>
class ThreadSafeVector
{
    // Optimized for frequent reads, infrequent writes
    std::vector<T> m_data;
    std::shared_mutex m_mutex;
};
```

## Memory Management Strategies

### Buffer Allocation System

SaberEngine implements sophisticated buffer management:

```cpp
class BufferAllocator
{
public:
    enum AllocationPool : uint8_t
    {
        Immutable,          // Static geometry, loaded once
        MutableTransient,   // Per-frame temporary data
        MutablePersistent,  // Long-lived dynamic data
        AllocationPool_Count
    };
    
private:
    // Ring buffer allocation for temporary data
    std::atomic<uint64_t> m_bufferBaseIndexes[AllocationPool_Count];
};
```

### Automatic Buffer Merging

The system automatically merges contiguous buffer updates:

```cpp
auto MergeContiguousCommits = [](std::vector<PlatformCommitMetadata>& dirtyBuffers)
{
    std::sort(dirtyBuffers.begin(), dirtyBuffers.end(),
        [](PlatformCommitMetadata const& a, PlatformCommitMetadata const& b)
        {
            if (a.m_buffer->GetUniqueID() == b.m_buffer->GetUniqueID())
                return a.m_baseOffset < b.m_baseOffset;
            return a.m_buffer->GetUniqueID() < b.m_buffer->GetUniqueID();
        });
    // Merge adjacent ranges...
};
```

### Reference Counting

Smart pointer usage throughout:

```cpp
// Automatic resource lifetime management
core::InvPtr<re::Texture> m_gBufferTextures[GBufferTexIdx_Count];
std::shared_ptr<re::Buffer> m_uniformBuffer;
std::unique_ptr<gr::Stage> m_renderStage;
```

## Shader System and Droid Compiler

### Effect Definition System

Shaders are defined through JSON effect files:

```json
{
    "Effect": {
        "Name": "PBRDeferred",
        "Techniques": [
            {
                "Name": "Opaque",
                "DrawStyles": ["Opaque", "Instanced"],
                "Shaders": {
                    "VertexShader": "PBRDeferred_VS",
                    "PixelShader": "PBRDeferred_PS"
                }
            }
        ]
    }
}
```

### Cross-API Shader Generation

Droid generates API-specific shaders from common definitions:

#### Input Processing
1. **Parse Effect JSON**: Extract shader requirements and techniques
2. **Generate Variants**: Create permutations based on material properties
3. **Cross-Compile**: Generate HLSL and GLSL from common source
4. **Optimize**: Platform-specific optimization passes

#### Code Generation
Droid generates C++ binding code:

```cpp
// Auto-generated effect binding
class Effect_PBRDeferred
{
public:
    enum Techniques { Opaque, Transparent, TechniqueCount };
    enum DrawStyles { Default, Instanced, DrawStyleCount };
    
    static EffectID GetEffectID();
    static TechniqueID GetTechnique(Techniques technique);
};
```

### Runtime Shader Resolution

The system dynamically selects appropriate shaders:

1. **Material Analysis**: Examine material properties and draw flags
2. **Technique Selection**: Choose appropriate rendering technique
3. **Variant Selection**: Select optimized shader variant
4. **Resource Binding**: Bind textures and buffers to shader

## Performance Optimization Techniques

### Instancing and Batching

SaberEngine automatically detects and optimizes draw calls:

#### Automatic Instance Detection
```cpp
class BatchManager
{
    // Automatically groups similar geometry into batches
    void ProcessDrawCalls()
    {
        // Group by: mesh, material, transform similarity
        // Generate instance data buffers
        // Combine into single draw calls
    }
};
```

#### Batch Pool Optimization
```cpp
class BatchPool
{
    // Reuses batch resources across frames
    std::vector<std::unique_ptr<Batch>> m_availableBatches;
    std::vector<std::unique_ptr<Batch>> m_usedBatches;
    
    // Minimizes allocation overhead
    std::unique_ptr<Batch> AcquireBatch();
    void ReleaseBatch(std::unique_ptr<Batch>);
};
```

### GPU Memory Management

#### Ring Buffer Allocation
```cpp
// Efficient temporary resource management
class RingBuffer
{
    uint64_t m_currentOffset;
    uint64_t m_bufferSize;
    
    BufferSlice AllocateTransient(uint64_t size);
    void AdvanceFrame(); // Recycle used portions
};
```

#### Resource State Tracking
```cpp
// Minimizes GPU state transitions
class ResourceStateTracker_DX12
{
    std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> m_resourceStates;
    
    void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES newState);
    void FlushBarriers(); // Batch state transitions
};
```

### CPU Performance Optimization

#### Cache-Friendly Data Layout
- **Structure of Arrays**: Components stored in contiguous arrays
- **Data Locality**: Related data stored together
- **Prefetching**: Strategic memory access patterns

#### Parallel Algorithm Implementation
```cpp
// Parallel frustum culling
void CullingSystem::UpdateCulling()
{
    auto cullJob = [this](auto& entities) {
        // Process entity subset on worker thread
        for (auto entity : entities) {
            // Perform view frustum test
            if (IsVisible(entity)) {
                AddToVisibleList(entity);
            }
        }
    };
    
    // Distribute work across threads
    DistributeWork(visibleEntities, cullJob);
}
```

## Advanced Rendering Techniques

### Physically-Based Lighting

SaberEngine implements industry-standard PBR based on Frostbite research:

#### Material Model
```cpp
struct PBRMaterial
{
    glm::vec3 baseColor;      // Diffuse albedo
    float metallic;           // Metallic factor [0,1]
    float roughness;          // Surface roughness [0,1]
    float ao;                 // Ambient occlusion [0,1]
    glm::vec3 emissive;       // Emissive color
    float emissiveStrength;   // Emissive intensity
};
```

#### BRDF Implementation
The engine implements the Cook-Torrance microfacet BRDF:
- **Fresnel**: Schlick approximation for dielectric/conductor interfaces
- **Distribution**: GGX/Trowbridge-Reitz normal distribution
- **Geometry**: Smith masking-shadowing function with height correlation

### Shadow Mapping Techniques

#### Percentage Closer Soft Shadows (PCSS)
```cpp
class ShadowSystem
{
    // Contact-hardening shadows with variable penumbra
    float PCSShadow(float3 shadowCoord, float3 worldPos);
    
    // Efficient sample pattern generation
    void GeneratePoissonSamples(uint32_t sampleCount);
};
```

#### Cascade Shadow Maps
- **Automatic Cascade Distribution**: Optimal cascade split calculation
- **Temporal Stability**: Stable shadow boundaries between frames
- **Memory Optimization**: Efficient shadow atlas packing

### Image-Based Lighting

#### Environment Map Processing
```cpp
class IBLSystem
{
    // Pre-filtered environment maps for real-time IBL
    void PrefilterEnvironmentMap(Texture* hdrEnvironment);
    
    // Split-sum approximation for environment BRDF
    void GenerateBRDF_LUT();
    
    // Importance sampling for diffuse irradiance
    void GenerateIrradianceMap(Texture* environmentMap);
};
```

#### HDR Tone Mapping
```cpp
// ACES filmic tone curve implementation
vec3 ACESFilmic(vec3 hdrColor)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    
    return (hdrColor * (a * hdrColor + b)) / (hdrColor * (c * hdrColor + d) + e);
}
```

### Ambient Occlusion Techniques

#### Intel XeGTAO Integration
SaberEngine integrates Intel's XeGTAO for high-quality screen-space AO:

- **Temporal Accumulation**: Multi-frame stability
- **Adaptive Quality**: Dynamic quality scaling based on performance
- **Bent Normals**: Directional occlusion for improved lighting

#### Ray-Traced Ambient Occlusion
For hardware supporting ray tracing:

```cpp
// Hardware-accelerated AO computation
[shader("raygeneration")]
void RTAORayGen()
{
    // Cast rays from G-Buffer positions
    // Accumulate occlusion samples
    // Apply spatial-temporal filtering
}
```

---

This technical deep dive demonstrates SaberEngine's sophisticated implementation of modern real-time rendering techniques, showcasing how research-level graphics programming can be successfully implemented in a production-quality engine architecture. The engine serves as an excellent reference for implementing advanced graphics features while maintaining clean, maintainable code organization.