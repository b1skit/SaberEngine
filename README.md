# Saber Engine

"Ideally, a Jedi took many months to construct a single perfect weapon that he or she would keep and use for a lifetime. Once you build it, the lightsaber will become your constant companion, your tool, and a ready means of defense." - Luke Skywalker

-----------------
Project Overview:
-----------------
Saber Engine is a multi-API, multi-threaded, real-time rendering R&D framework, with the architecture of a game engine.


--------
Details:
--------
- Currently supported graphics APIs:
	- OpenGL 4.6
	- DirectX 12
- Renders GLTF 2.0 scenes/assets
- Implemented with C++ 20


----------------------------------
User-Specific Visual Studio Setup:
----------------------------------
"Project -> Properties -> Configuration Properties -> Debugging -> Working Directory" -> "$(SolutionDir)SaberEngine"  


---------------------------
Enabling PIX Functionality:
---------------------------
Microsoft PIX requires the "[HKEY_LOCAL_MACHINE\SOFTWARE\Policies\Microsoft\Windows\Appx]" key to exist in the local Windows registry. This can be enabled by executing the following command from a command prompt launched with administrator priviledges:  
reg add HKLM\SOFTWARE\Policies\Microsoft\Windows\Appx  


-----------------------
Command line arguments:
-----------------------
Scene loading: -scene Folder\Name\filename.extension  
	- Path is relative to the "<project root>\Scenes\" directory  
	- Supports GLTF 2.0 files

Display log messages in a system console window: -console  

Disable strict shader binding checks: -relaxedshaderbinding  
	- Skip (helpful, but occasionally annoying) asserts if textures/parameters aren't found in a shader

Enable graphics API debugging: -debuglevel [0, 2]  
	- 0: Default (disabled)  
	- 1: Basic debug output (OpenGL, DX12)  
	- 2: Level 1 + GPU-based validation (DX12 only)  
	- 3: Level 2 + DRED breadcrumbs (DX12 only)  


----------------------
Runtime Configuration:
----------------------
Settings are loaded from the "<project root>\config\config.cfg" file  
	- Commands for system configuration ("set"), input ("bind"), etc  
	- See the existing file for examples  
Default controls:  
	- Movement: WASD + mouse look  
	- Down: Shift  
	- Up: Space  
	- Toggle VSync: v  


------------
Conventions:
------------
- Right-handed coordinate system  
- UV (0,0) = Top-left  
- Depth near/far = [0,1]  


---------------------
Image-based Lighting:
---------------------
A per-scene IBL is loaded from ""<project root>\Scenes\SceneFolderName\IBL\ibl.hdr", if it exists  
	- A default IBL ("<project root>\Assets\DefaultIBL\ibl.hdr") is used as a fallback if no scene IBL is found  

--------
Shaders:
--------
For simplicity, naming patterns are used to associate Shaders.  
- OpenGL: Shaders sharing common names (with .vert/.geom/.frag suffixes) will be concatenated and compiled at runtime  
- DX12: Compiled Shader Objects (.cso) sharing a common name prefix are differentiated by a "<ShaderName>_<?>Shader.hlsl" suffix (e.g. Some_VShader.hlsl, Some_GShader.hlsl, Some_PShader.hlsl), and associated at runtime  


-------------
Dependencies:
-------------
Dependencies are included via Git Subtrees. Configuration details are included below for reference.  


Imgui: https://github.com/ocornut/imgui/
-----------------------------------------
- Current version: v1.89.9  
- <project root>\Source\Dependencies\imgui\  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\imgui\"  
- All of the .h and .cpp files in the .\Source\Dependencies\imgui\ directory are added to the Visual Studio project (under the "imgui" filter)  
- The dx12, opengl3, and win32 imgui_impl_* .h and .cpp files in the .\Source\Dependencies\imgui\backends\are added to the Visual Studio project (under the "imgui\backends" filter)  


CGLTF: https://github.com/jkuhlmann/cgltf
-----------------------------------------
- Current version: cgltf-1.13  
- <project root>\Source\Dependencies\cgltf\  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)..\Dependencies\cgltf\"  
- Sample GLTF assets compatible with Saber Engine can be found here: https://github.com/KhronosGroup/glTF-Sample-Models  


MikkTSpace: https://github.com/mmikk/MikkTSpace
-----------------------------------------------
- Current version: "Mar 25, 2020"  
- <project root>\Source\Dependencies\MikkTSpace\  
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\MikkTSpace\"  


MikkTSpace Welder: https://github.com/mmikk/Welder
--------------------------------------------------
- Current version: "Mar 25, 2020""
- <project root>\Source\Dependencies\Welder\
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\Welder\"  


Direct-X Headers: https://github.com/microsoft/DirectX-Headers
--------------------------------------------------------------
- Current version: 1.610.0
- "Project properties -> C/C++ -> General -> Additional Include Directories" -> "$(ProjectDir)Dependencies\DirectX-Headers\include\"  
- Add "$(WindowsSDK_LibraryPath)\x64\d3d12.lib" and "$(WindowsSDK_LibraryPath)\x64\dxgi.lib" to "Project properties -> Linker -> Input -> Additional Dependencies"  


Glew: https://github.com/nigels-com/glew/releases
-------------------------------------------------
- Current version: glew-2.2.0-win32 (Note: The pre-compiled library is used, the Glew dependency is not added via a Subtree)
- <project root>\Source\Dependencies\glew\
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\glew\include"  
- "Project properties -> Linker -> General -> Additional Library Directories" -> "$(ProjectDir)Dependencies\glew\lib\x64\"  
- Ensure the "Project properties -> Build Events -> Post-Build Event" copies glew\bin\Release\x64\glew32.dll to the <Project Root>\SaberEngine\ directory


GLM: https://github.com/g-truc/glm/releases
-------------------------------------------
- Current version: GLM 0.9.9.8
- <project root>\Source\Dependencies\glm\
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\glm\"  
- "Project properties -> Linker -> General -> Additional Library Directories" -> "$(ProjectDir)..\Dependencies\glm\"  


stb (stb_image.h): https://github.com/nothings/stb/blob/master/
--------------------------------------------------------------------
- Current version: Jan 29, 2023
- "Project properties -> C/C++ -> Additional Include Directories" -> "$(ProjectDir)Dependencies\stb\"  


--------------------------------------------------
Recommended Visual Studio extensions and Software:
--------------------------------------------------
- Smart Command Line Arguments
- Editor Guidelines
- GLSL Language Integration: https://github.com/danielscherzer/GLSL
- License Header Manager
- Beyond Compare

© 2022 Adam Badke. All rights reserved.
