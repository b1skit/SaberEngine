# Saber Engine

"Ideally, a Jedi took many months to construct a single perfect weapon that he or she would keep and use for a lifetime. Once you build it, the lightsaber will become your constant companion, your tool, and a ready means of defense." - Luke Skywalker

--------------
Project goals:
--------------
- Progressive gpu forward path tracer
	-> GPU implementation of PBRT
- Fork of Blaze Engine, but highly modernized
	-> Multi-threaded
	-> Supports animation, GPU instancing
- Modern API: DX12/Vulkan


--------
Details:
--------
- Supported graphics APIs:
	- OpenGL 4.6
- Renders GLTF 2.0 scenes/assets
- Implemented with C++ 20


-----------------------
Command line arguments:
-----------------------
Scene loading: -scene Folder\Name\filename.extension
	- Path is relative to the "<project root>\Scenes\" directory
	- Supports GLTF 2.0 files


--------------
Configuration:
--------------
Loaded from the "<project root>\config\config.cfg"
	- Commands for system configuration ("set"), input ("bind"), etc


---------------------
Image-based Lighting:
---------------------
A per-scene IBL is loaded from ""<project root>\Scenes\SceneFolderName\IBL\ibl.hdr", if it exists
	- A default IBL ("<project root>\Assets\DefaultIBL\ibl.hdr") is used as a fallback if no scene IBL is found


-------------
Dependencies:
-------------

Imgui: https://github.com/ocornut/imgui/
-----------------------------------------
- Current version: v1.88
- Unzip to <project root>\Dependencies\imgui\
- Add "$(ProjectDir)..\Dependencies\imgui\" to "Project properties -> C/C++ -> Additional Include Directories"
- Add the \Dependencies\imgui\.h/.cpp files to the Visual Studio project (i.e. under the "imgui" filter in the project view)
- Add the \Dependencies\imgui\backends\imgui_impl_ .h/.cpp files for dx12, opengl3, sdl to the Visual Studio project (i.e. under the "imgui\backends" filter in the project view)


CGLTF: https://github.com/jkuhlmann/cgltf
-----------------------------------------
- Current version: cgltf-1.13
- Unzip to <project root>\Dependencies\cgltf\
- Add "$(ProjectDir)..\Dependencies\cgltf\" to "Project properties -> C/C++ -> Additional Include Directories"
- Sample assets: https://github.com/KhronosGroup/glTF-Sample-Models


MikkTSpace: https://github.com/mmikk/MikkTSpace
-----------------------------------------------
- Current version: "Github last modified Mar 25, 2020""
- Unzip to <project root>\Dependencies\MikkTSpace
- Add "$(ProjectDir)..\Dependencies\MikkTSpace\" to "Project properties -> C/C++ -> Additional Include Directories"


MikkTSpace Welder: https://github.com/mmikk/Welder
--------------------------------------------------
- Current version: "Github last modified Mar 25, 2020""
- Unzip to <project root>\Dependencies\Welder
- Add "$(ProjectDir)..\Dependencies\Welder\" to "Project properties -> C/C++ -> Additional Include Directories"


Glew: https://github.com/nigels-com/glew/releases
-------------------------------------------------
- Current version: glew-2.2.0-win32 (pre-compiled)
- Unzip to <project root>\Dependencies\glew\
- Add "$(ProjectDir)..\Dependencies\glew\include" to "Project properties -> C/C++ -> Additional Include Directories" for all configurations
- Add "$(ProjectDir)..\Dependencies\glew\lib\x64\" to "project properties -> Linker -> General -> Additional Library Directories" for all build configurations
- Ensure the "Project properties -> Build Events -> Post-Build Event" copies glew\bin\Release\x64\glew32.dll to the <Project Root>\SaberEngine\ directory


GLM: https://github.com/g-truc/glm/releases
-------------------------------------------
- Current version: GLM 0.9.9.8
- Unzip to <project root>\Dependencies\glm\
- Add "$(ProjectDir)..\Dependencies\glm\" to "Project properties -> C/C++ -> Additional Include Directories" for all configurations
- Add "$(ProjectDir)..\Dependencies\glm\" to "project properties -> Linker -> General -> Additional Library Directories"


SDL2: https://www.libsdl.org/index.php
--------------------------------------
- Current version: SDL2-2.0.22-win32-x64.zip (stable)
- Unzip to <project root>\Dependencies\SDL2\
- Open the .\SDL2\VisualC\SDL.sln in Visual Studio, and compile the 3 SDL2 projects (SDL2, SDL2main, SDL2test) for both the Debug and Release configurations
- Add "$(ProjectDir)..\Dependencies\SDL2\include\" to "Project properties -> C/C++ -> Additional Include Directories" for all configurations
- Add "$(ProjectDir)..\Dependencies\SDL2\VisualC\x64\Release\" to "project properties -> Linker -> General -> Additional Library Directories" for all build configurations
- Add "SDL2.lib" to "Project properties -> Linker -> Input -> Additional Dependences"
- Ensure the "Project properties -> Build Events -> Post-Build Event" copies the relevant Debug/Release configuration of the SDL2.dll to the <Project Root>\SaberEngine\ directory


stb_image.h: https://github.com/nothings/stb/blob/master/stb_image.h
--------------------------------------------------------------------
- Current version: v2.27 
- Add stb_image.h to <project root>\Dependencies\stb\
- Add "$(ProjectDir)..\Dependencies\stb\" to "Project properties -> C/C++ -> Additional Include Directories" for all configurations


-------------------------------------
Recommended Visual Studio extensions:
-------------------------------------
- Smart Command Line Arguments
- Editor Guidelines
- GLSL Language Integration: https://github.com/danielscherzer/GLSL
- Beyond Compare
