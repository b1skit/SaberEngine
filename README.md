# Saber Engine

"Ideally, a Jedi took many months to construct a single perfect weapon that he or she would keep and use for a lifetime. Once you build it, the lightsaber will become your constant companion, your tool, and a ready means of defense." - Luke Skywalker

-----------------
Project Overview:
-----------------
Saber Engine is a multi-API, multi-threaded, real-time rendering R&D framework, with the architecture of a game engine. © 2022 Adam Badke. All rights reserved.


---------
Features:
---------
Saber Engine is constantly improving. Its current features include:
- Rendering API agnostic. Supports:
	- DirectX 12 (Agility SDK 1.611.2) (Default)  
	- OpenGL 4.6  
	- Upcoming Vulkan support  
- Asynchronous copy/graphics/compute pipelines  
- Multi-threaded architecture programmed in C++ 20  
- EnTT Entity Component System (ECS)  
- GLTF 2.0 scene format support  
- HDR physically-based lighting model (As per EA’s Frostbite, Lagarde et al.)  
- Image-based indirect lighting  
- Directional, point and spot punctual lights  
- PCF & PCSS soft shadows  
- Radiometrically-correct screen-space ambient occlusion (Intel XeGTAO)  
- ACES filmic response tone mapping  
- Physically-based camera and exposure settings  
- Physically-based emissive and bloom  
- Camera frustum culling  
- Automatic GPU instancing  
- ImGui menus and a robust suite of debugging tools  
- Support for RenderDoc and PIX programmatic capture APIs  


-----------------------
Command line arguments:
-----------------------
Most of the keys described in `ConfigKeys.h` can be set/overridden via key/value command line arguments using a `-keyname value` pattern.  If `value` is omitted, it will be stored as a boolean true value.  The most important command line arguments are described here:

Scene loading: `-scene Folder\Name\filename.extension`  
* Path is relative to the `<project root>\Scenes\` directory  
* Supports GLTF 2.0 files

Display log messages in a system console window: `-console`  

Select the backend rendering API: `-platform API`. If no API is specified, DirectX 12 is used. Supported API values are:
* dx12
* opengl

Enable strict shader binding checks: `-strictshaderbinding`  
* Enables (helpful, but occasionally annoying) asserts if parameters aren't found when parsing reflected shader metadata  

Enable graphics API debugging: `-debuglevel [0, 2]`. Each level increases log verbosity, and API-specific validation  
* 0: Default (disabled)  
* 1: Basic debug output (OpenGL, DX12)  
* 2: Level 1 + GPU-based validation (DX12 only)  

Enable DRED debugging: `-enabledred` (DX12 only)  

Enable PIX programmatic capture (DX12 only): `-enablepixgpucapture`, `-enablepixcpucapture`  
* This is only required for programmatic captures. It is not required for PIX markers  
* Captures can be triggered via the render debug menu  
* More info on PIX programmatic captures here: https://devblogs.microsoft.com/pix/programmatic-capture/  

Enable RenderDoc programmatic capture: `-renderdoc`  
* This is only required for programmatic captures. RenderDoc can still launch/capture without this  

Enable CPU-side normalization of vertex streams when requested: `-cpunormalizevertexstreams`  
* This is provided for strict GLTF 2.0 compatibility but only rarely required if a vertex stream requires normalization, but is not received in a format compatible with GPU normalization  


--------------  
Menu and logs:  
--------------  
Press the ` (tilde/grave) key to show/hide the ImGui overlay  
* Logs are also output to the `.\Logs\` directory  


----------------------
Runtime Configuration:
----------------------
Settings are loaded from the `<project root>\config\config.cfg` file  
* Commands for system configuration ("set"), input ("bind"), etc  
* See the existing file for examples  

Default controls:  
* Camera movement: WASD + mouse look  
* Down: Shift  
* Up: Space  
* Toggle VSync: v  


------------
Conventions:
------------
- Right-handed coordinate system  
- UV (0,0) = Top-left  
- Depth near/far = [0,1]  
- CPU-side matrices are stored in column-major order (GLM default)  
- GLSL matrices constructed/consumed in column-major order (GLSL default)  
- HLSL uniform matrices arrive in column-major order. Matrices declared in shader body are constructed in row-major order (HLSL defaults)  


---------------------
Image-based Lighting:
---------------------
* A per-scene IBL is loaded from `<project root>\Scenes\SceneFolderName\IBL\ibl.hdr`, if it exists  
  * A default IBL (`<project root>\Assets\DefaultIBL\default.hdr`) is used as a fallback if no scene IBL is found  
  * New IBLs can be loaded at runtime via the ImGui menus  


--------
Shaders:
--------
For simplicity, naming patterns are used to associate Shaders.  
- OpenGL: Shaders sharing common names (with .vert/.geom/.frag suffixes) will be concatenated and compiled at runtime  
- DX12: Compiled Shader Objects (.cso) sharing a common name prefix are differentiated by a `<ShaderName>_<?>Shader.hlsl` suffix (e.g. Some_VShader.hlsl, Some_GShader.hlsl, Some_PShader.hlsl), and associated at runtime  


------------------
PIX Configuration:
------------------
Microsoft PIX requires the `[HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\Appx]` key to exist in the local Windows registry. This can be enabled by executing the following command from a command prompt launched with administrator priviledges:  
```reg add HKLM\SOFTWARE\Policies\Microsoft\Windows\Appx```  


----------------
DX12 Shader PDBs
----------------
Shader PDBs are generated when SaberEngine's Debug build configuration is compiled. PDBs are output to .\Build\x64\Debug\  

* __PIX configuration__: Set the shader PDB path in the "Settings -> Symbol / PDB Options" menu  
* __RenderDoc configuration__: Set the shader PDB path in the "Tools -> Settings -> Core Shader debug search path" menu  


--------------
Initial setup:
--------------
* Clone the repository: `git clone https://github.com/b1skit/SaberEngine.git`
* Run the `.\InitialSetup.bat` script
  * This will initialize and update the git submodule dependencies, and configure vcpkg 
* Set your working directory for all build configurations:
  * Project -> Properties -> Configuration Properties -> Debugging -> Working Directory -> $(SolutionDir)SaberEngine  
* Build the project
  * It's recommended you build the Release configuration first, as this will copy all files required for distribution to the `.\SaberEngine\` output/working directory  

Notes:
* The remaining dependencies are automatically included via Git Subtrees. Installation/configuration details are included below for posterity.  


CGLTF: https://github.com/jkuhlmann/cgltf
-----------------------------------------
- Current version: cgltf-1.13  
- `<project root>\Source\Dependencies\cgltf\`  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)..\Dependencies\cgltf\"  
- Sample GLTF assets compatible with Saber Engine can be found here: https://github.com/KhronosGroup/glTF-Sample-Models  


EnTT: https://github.com/skypjack/entt
--------------------------------------
- Included as a dependency via `vcpkg`. See the `Dependencies` section for more info  


Glew: https://github.com/nigels-com/glew/releases
-------------------------------------------------
- Current version: glew-2.2.0-win32 (Note: The pre-compiled library is used, the Glew dependency is not added via a Subtree)
- `<project root>\Source\Dependencies\glew\`
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\glew\include"  
- "Project properties -> Linker -> General -> Additional Library Directories" -> "$(ProjectDir)Dependencies\glew\lib\x64\"  
- Ensure the "Project properties -> Build Events -> Post-Build Event" copies glew\bin\Release\x64\glew32.dll to the <Project Root>\SaberEngine\ directory  


GLM: https://github.com/g-truc/glm/releases
-------------------------------------------
- Current version: GLM 0.9.9.8  
- `<project root>\Source\Dependencies\glm\`  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\glm\"  
- "Project properties -> Linker -> General -> Additional Library Directories" -> "$(ProjectDir)..\Dependencies\glm\"  


Imgui: https://github.com/ocornut/imgui/
-----------------------------------------
- Current version: v1.89.9  
- `<project root>\Source\Dependencies\imgui\`  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\imgui\"  
- All of the .h and .cpp files in the .\Source\Dependencies\imgui\ directory are added to the Visual Studio project (under the "imgui" filter)  
- The dx12, opengl3, and win32 imgui_impl_* .h and .cpp files in the .\Source\Dependencies\imgui\backends\are added to the Visual Studio project (under the "imgui\backends" filter)  


Intel XeGTAO: https://github.com/GameTechDev/XeGTAO
---------------------------------------------------
- Current version: Dec 2021  
- `<project root>\Source\Dependencies\XeGTAO\`  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\XeGTAO\"  


MikkTSpace: https://github.com/mmikk/MikkTSpace
-----------------------------------------------
- Current version: "Mar 25, 2020"  
- `<project root>\Source\Dependencies\MikkTSpace\`  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\MikkTSpace\"  


MikkTSpace Welder: https://github.com/mmikk/Welder
--------------------------------------------------
- Current version: "Mar 25, 2020""  
- `<project root>\Source\Dependencies\Welder\`  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\Welder\"  


stb (stb_image.h): https://github.com/nothings/stb/blob/master/
--------------------------------------------------------------------
- Current version: Jan 29, 2023  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\stb\"  


Microsoft vcpkg: https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-msbuild
----------------------------------------------------------------------------------------
- Installed as a submodule  
- Manifest mode is enabled: Add dependencies to the `.\vckpg.json` file  


---------------
NuGet Packages:
---------------
The following packages are automatically included in the SaberEngine solution  
- WinPixEventRuntime  
- Microsoft DirectX 12 Agility SDK  


--------------------------------------------------
Recommended Visual Studio extensions and Software:
--------------------------------------------------
- Smart Command Line Arguments  
- Editor Guidelines  
- GLSL Language Integration: https://github.com/danielscherzer/GLSL  
- HLSL Tools for Visual Studio: https://github.com/tgjones/HlslTools
  - Configuration guide (shadertoolsconfig.json): https://github.com/tgjones/HlslTools#custom-preprocessor-definitions-and-additional-include-directories
- License Header Manager  
- Beyond Compare  


© 2022 Adam Badke. All rights reserved.
