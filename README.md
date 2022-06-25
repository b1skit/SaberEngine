# Saber Engine

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
- Implemented with C++ 20


-------------
Dependencies:
-------------

Assimp: https://github.com/assimp/assimp/releases
-------------------------------------------------
- Current version: assimp-5.2.4
- Unzip files to the <project root>\Dependencies\assimp\ folder
- Run "cmake CMakeLists.txt" from the assimp root folder to generate the visual studio solution
- Add "$(ProjectDir)..\Dependencies\assimp\include" to "Project properties -> C/C++ -> Additional Include Directories" for all configurations
- Add "$(ProjectDir)..\Dependencies\assimp\lib\<CONFIG TYPE>\" to "project properties -> Linker -> General -> Additional Library Directories" for each build configuration (eg. Debug/Release etc)
- Add "assimp-vc***-mt*.lib" to "Project properties -> Linker -> Input -> Additional Dependences" for each build configuration (eg. Debug/Release etc)
- Ensure the "Project properties -> Build Events -> Post-Build Event" copies the relevant Debug/Release configuration of the assimp-vc***-mt.dll to the <Project Root>\SaberEngine\ directory


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
