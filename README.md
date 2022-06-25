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
- Add "$(ProjectDir)..\Dependencies\assimp\include;" to "Project properties -> C/C++ -> Additional Include Directories"
- Add "$(ProjectDir)..\Dependencies\assimp\lib\<CONFIG TYPE>\" to "project properties -> Linker -> General -> Additional Library Directories" for each build configuration (eg. Debug/Release etc)
- Add "assimp-vc***-mt*.lib;" to "Project properties -> Linker -> Input -> Additional Dependences" for each build configuration (eg. Debug/Release etc)
- Ensure the "Project properties -> Build Events -> Post-Build Event" copies the relevant Debug/Release configuration of the assimp-vc***-mt.dll to the <Project Root>\SaberEngine\ directory


Glew: https://github.com/nigels-com/glew/releases
-------------------------------------------------
- Current version: glew-2.2.0-win32 (pre-compiled)
- Unzip to <project root>\Dependencies\glew\
- Add "$(ProjectDir)..\Dependencies\glew\include;" to "Project properties -> C/C++ -> Additional Include Directories"
- Add "$(ProjectDir)..\Dependencies\glew\lib\x64\" to "project properties -> Linker -> General -> Additional Library Directories" for all build configurations
- Add "glew32.lib;" to "Project properties -> Linker -> Input -> Additional Dependences"
- Ensure the "Project properties -> Build Events -> Post-Build Event" copies glew\bin\Release\x64\glew32.dll to the <Project Root>\SaberEngine\ directory


-------------------------------------
Recommended Visual Studio extensions:
-------------------------------------
- Smart Command Line Arguments
- Editor Guidelines
- GLSL Language Integration: https://github.com/danielscherzer/GLSL
- Beyond Compare



copy "$(ProjectDir)..\Dependencies\" "$(ProjectDir)"