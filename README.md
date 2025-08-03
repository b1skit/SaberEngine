# Saber Engine

"Ideally, a Jedi took many months to construct a single perfect weapon that he or she would keep and use for a lifetime. Once you build it, the lightsaber will become your constant companion, your tool, and a ready means of defense." - Luke Skywalker

## Project Overview
Saber Engine is a multi-API, multi-threaded, real-time rendering research & development framework with the architecture of a game engine. © 2022 Adam Badke. All rights reserved.


## Features
#### Core Architecture:
- **Multi-threaded architecture** implemented in C++20
- **Scriptable rendering pipeline**:
  - Graphics systems are implemented using a high-level, API-agnostic abstraction layer & combined through input/output dependencies defined in `.json`
  - Dynamically generates an optimized, thread-safe render graph at runtime
- **Droid**: A custom offline shader compiler & C++ code generation tool
  - Effects/Techniques/DrawStyles are described via `.json` for dynamic runtime shader resolution
- **GLTF 2.0** format support (cgltf):
  - Supports the `KHR_lights_punctual` extension with:
    - Directional lights
    - Point lights
    - Spot lights
  - Supports the `KHR_materials_emissive_strength` extension, allowing configurable emissive intensity
  - Supports `KHR_materials_unlit` extension for unlit shading
  - Supports `EXT_mesh_gpu_instancing` extension for GPU instancing

#### Rendering Functionality:
- **Rendering API-agnostic**: Supports:
  - **DirectX 12** (Agility SDK 1.611.2) *(default)*
  - **OpenGL 4.6**
  - *Upcoming Vulkan support*
- **GPU-accelerated ray tracing** (DXR)
- **Asynchronous** copy/graphics/compute pipelines
- Supports both **bindless** and **slot-based** resource binding models
- **Advanced graphics features**:
  - **Animation**: Skinning, morph targets, & keyframe node/transform animations
  - **HDR Physically-Based Lighting Model** (based on EA's Frostbite, Lagarde et al.)
    - Image-based indirect lighting
  - **Soft shadows**: PCF & PCSS
  - **Inline ray traced** shadows
  - **Radiometrically-correct screen-space ambient occlusion** (Intel XeGTAO)
  - **Ray traced ambient occlusion** (RTAO)
  - **ACES filmic response** and Reinhard tone mapping
  - Physically-based **camera** & exposure settings
  - Physically-based **emissive** lighting & bloom
  - **Camera frustum culling**
  - **GPU instancing**
    - Automatically detects & combines instanceable batches into single draw calls

#### Resource Management:
- **Asynchronous, reference-counted resource loading** system:
  - Supports work stealing
- **Indexed buffer management system** for automatic instancing and buffer indirection LUT generation
- **Batch pool** minimizes draw-call setup costs by reusing draw-call resources

#### Additional Features:
- **Entity Component System** (EnTT)
- **Interactive UI** (ImGui):
  - Supports drag-&-drop loading of `.gltf` & `.hdr` files
- **Comprehensive debugging tools**:
  - Real-time CPU/GPU frame timers
  - Support for **RenderDoc** & **PIX** programmatic capture APIs
  - **NVIDIA Aftermath** integration for automatic GPU crash dump generation

Saber Engine is continuously evolving. Additional features are always in development.


## Initial setup
* Clone the repository: `git clone https://github.com/b1skit/SaberEngine.git`
* Run the `.\InitialSetup.bat` script (requires git to be installed on the system)
  * This initializes & updates the git submodule dependencies & configures vcpkg
* Set your working directory for all build configurations:
  * Project -> Properties -> Configuration Properties -> Debugging -> Working Directory -> $(SolutionDir)SaberEngine
* Build the project

## Command line arguments
Most of the keys described in `ConfigKeys.h` can be set or overridden via key/value command line arguments using a `-keyname value` pattern. If `value` is omitted, it will be stored as a Boolean true value. The most important command line arguments are described here:

File loading: `-import Directory\Path\filename.extension`
* Supports GLTF 2.0 files
* Alternatively supports drag-&-drop loading of GLTF files

Display log messages in a system console window: `-console`

Select the backend rendering API: `-platform API`. If no API is specified, DirectX 12 is used. Currently supported API values:
* dx12
* opengl

Select a rendering pipeline: `-renderpipeline pipelineName.json`
* Rendering pipelines are described by json files located in the `<project root>\Assets\Pipelines\` directory
* If no rendering pipeline file is specified, the `DeferredRasterization.json` is used

Enable strict shader binding checks: `-strictshaderbinding`
* Enables helpful (but overly strict) asserts if runtime parameters don't match exactly with reflected shader metadata

Enable graphics API debugging: `-debuglevel [0, 2]`. Each level increases log verbosity & API-specific validation
* 0: Default (disabled)
* 1: Basic debug output (OpenGL, DX12)
* 2: Level 1 + GPU-based validation (DX12 only)

Enable DRED debugging: `-enabledred` (DX12 only)

Enable NVIDIA Aftermath support for debugging GPU crashes or hangs: `-aftermath`
* As recommended by NVIDIA, Aftermath is enabled only if the binary is compiled with `#define USE_NSIGHT_AFTERMATH` uncommented in Debug_DX12.h
* Requires the NVIDIA Aftermath SDK to be installed with the Aftermath Crash Monitor running on the local system

Enable PIX programmatic capture (DX12 only): `-enablepixgpucapture`, `-enablepixcpucapture`
* This is required only for programmatic captures & is not needed for PIX markers
* Captures can be triggered via the render debug menu or by attaching PIX to the SaberEngine process
* More info on PIX programmatic captures here: https://devblogs.microsoft.com/pix/programmatic-capture/

Enable RenderDoc captures: `-renderdoc`

Enable CPU-side normalization of vertex streams when requested: `-cpunormalizevertexstreams`
* This option is provided for strict GLTF 2.0 compatibility but is rarely required. It is useful when a vertex stream requires normalization yet is received in a format incompatible with GPU normalization.


## Runtime Configuration
Settings are loaded from the `<project root>\config\config.cfg` file
* Commands for system configuration ("set"), input ("bind"), etc
* See the existing file for examples

Default controls:
* Camera movement: WASD + mouse look
* Down: Shift
* Up: Space
* Toggle VSync: v


## Menu & logs
* Press the ` (tilde/grave) key to switch between camera movement or UI interactions
* Press the F11 key to show/hide the ImGui overlay
* Logs are also output to the `.\Logs\` directory


## Image-based Lighting
* A default HDR is included for IBL at `<project root>\Assets\DefaultIBL\default.hdr`
  * GLTF files can override this default by placing a `default.hdr` file in an `IBL` folder alongside the GLTF file
    * E.g. When loading `Some\Folder\Example.gltf`, a HDR at `Some\Folder\IBL\default.hdr` will be loaded at the same time as the GLTF file
  * Additional HDRs can also be imported from any location at runtime via the ImGui menus or by drag-&-drop loading of .hdr files


## Shaders, Effects, Techniques, Draw Styles
Shaders are dynamically resolved at runtime by matching draw style flags set via materials & render stages to sets of draw style rules defined by Effects that map to Techniques. See the JSON Effect definition files located in the `Assets\Effects` directory for example usage.

For simplicity shader names are expected to be identical between all APIs, with the exception of their file extensions
- OpenGL: Shaders have .glsl/.glsli extensions. These files are loaded/processed at runtime
- DX12: Shaders are compiled with the application from source .hlsli/.hlsl files. The resulting Compiled Shader Objects (.cso) with the same (extensionless) filename are loaded at runtime


## Droid shader compilation & code generation
Droid is automatically compiled & executed as part of the SaberEngine solution build process. By default, it parses the contents of the `<project root>\SaberEngine\Assets\Effects\` directory & converts Effect definitions into compilable C++/HLSL/GLSL code.

Its execution can be optionally modified via command line arguments:

Clean output directories: `-clean`
* Erases all generated C++ & shader code, & shader compilation artifacts.


## Conventions
- Right-handed coordinate system
- UV (0,0) = Top-left
- Depth near/far = [0,1]
- CPU-side matrices are stored in column-major order (GLM default)
- GLSL matrices constructed/consumed in column-major order (GLSL default)
- HLSL uniform matrices arrive in column-major order. Matrices declared in shader body are constructed in row-major order (HLSL defaults)


## Dependencies
* SaberEngine uses vcpkg, NuGet, & Git subtrees to manage dependencies. Source details are included for each dependency below.
* Git subtree dependencies are preconfigured. Installation & configuration details are included below for completeness.


### [CGLTF](https://github.com/jkuhlmann/cgltf)

- Included as a dependency via `vcpkg`. See the `Initial setup` section & `.\vcpkg.json` for more info
- Sample GLTF assets compatible with Saber Engine can be found here: https://github.com/KhronosGroup/glTF-Sample-Models


### [EnTT](https://github.com/skypjack/entt)

- Included as a dependency via `vcpkg`. See the `Initial setup` section & `.\vcpkg.json` for more info


### [Glew](https://github.com/nigels-com/glew/releases)

- Included as a git subtree: `<project root>\Source\Dependencies\glew\`
- Current version: glew-2.2.0-win32 (Note: The pre-compiled library is used, the Glew dependency is not added via a Subtree)
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\glew\include"
- "Project properties -> Linker -> General -> Additional Library Directories" -> "$(ProjectDir)Dependencies\glew\lib\x64\"
- Ensure the "Project properties -> Build Events -> Post-Build Event" copies glew\bin\Release\x64\glew32.dll to the <Project Root>\SaberEngine\ directory


### [GLM](https://github.com/g-truc/glm/releases)

- Included as a dependency via `vcpkg`. See the `Initial setup` section & `.\vcpkg.json` for more info


### [Imgui](https://github.com/ocornut/imgui/)

- Included as a git submodule: `<project root>\Source\Dependencies\imgui\`
- Current version: v1.91.5 (Docking branch)
- Compiled as a static library project: `<project root>\Source\ImGui\`
- The ImGui static library includes all core ImGui files & backend implementations for DX12, OpenGL3, & Win32
- Projects that use ImGui link against the ImGui static library & include imgui headers via `$(SolutionDir)Source\Dependencies\imgui\` in their include directories


### [Intel XeGTAO](https://github.com/GameTechDev/XeGTAO)

- Included as a git subtree: `<project root>\Source\Dependencies\XeGTAO\`
- Current version: Dec 2021
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\XeGTAO\"


### [Microsoft DirectX 12 Agility SDK](https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/)

- Included via a NuGet package


### [Microsoft vcpkg](https://github.com/microsoft/vcpkg/tree/master)

- Included as a git submodule: `<project root>\Source\Dependencies\cpkg\`
- See the `Initial setup` section for configuration instructions
- Manifest mode is enabled: Add dependencies to the `.\vcpkg.json` file


### [MikkTSpace](https://github.com/mmikk/MikkTSpace)

- Included as a git subtree: `<project root>\Source\Dependencies\MikkTSpace\`
- Current version: "Mar 25, 2020"
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\MikkTSpace\"


### [MikkTSpace Welder](https://github.com/mmikk/Welder)

- Included as a git subtree: `<project root>\Source\Dependencies\Welder\`
- Current version: "Mar 25, 2020"
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\Welder\"


### [JSON for Modern C++](https://github.com/nlohmann/json)

- Included as a dependency via `vcpkg`. See the `Initial setup` section & `.\vcpkg.json` for more info


### [stb (stb_image.h)](https://github.com/nothings/stb/blob/master/)

- Included as a git subtree: `<project root>\Source\Dependencies\stb\`
- Current version: Jan 29, 2023
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\stb\"


### [WinPixEventRuntime](https://devblogs.microsoft.com/pix/winpixeventruntime/)

- Included via a NuGet package


## Recommended Visual Studio extensions & Software
- Smart Command Line Arguments
- Editor Guidelines
- GLSL Language Integration: https://github.com/danielscherzer/GLSL
- HLSL Tools for Visual Studio: https://github.com/tgjones/HlslTools
  - Configuration guide (shadertoolsconfig.json): https://github.com/tgjones/HlslTools#custom-preprocessor-definitions-&-additional-include-directories
- License Header Manager
- Beyond Compare


© 2022 Adam Badke. All rights reserved.
