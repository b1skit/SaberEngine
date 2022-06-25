# Saber Engine

Project goals:
--------------
- Progressive gpu forward path tracer
	-> GPU implementation of PBRT
- Fork of Blaze Engine, but highly modernized
	-> Multi-threaded
	-> Supports animation, GPU instancing
- Modern API: DX12/Vulkan


Dependencies:
-------------

Assimp:
- Current version: assimp-5.2.4
- Unzip files to the <project root>\Dependencies\assimp\ folder
- Run "cmake CMakeLists.txt" from the assimp root folder to generate the visual studio solution
- Add "$(ProjectDir)..\Dependencies\assimp\include;" to "project properties -> C/C++ -> Additional Include Directories"
- Add "$(ProjectDir)..\Dependencies\assimp\lib\<CONFIG TYPE>\" to "project properties -> Linker -> General -> Additional Library Directories" for each build configuration (eg. Debug/Release etc)
- Add "assimp\lib\<CONFIG TYPE>\assimp-vc***-mt*.lib;" to "project properties -> Linker -> Input -> Additional Dependences" for each build configuration (eg. Debug/Release etc)
- Ensure the "project properties -> Build Events -> Post-Build Event" copies the relevant Debug/Release configuration of the assimp-vc***-mt.dll to the <Project Root>\SaberEngine\ directory
- Update the .gitignore if the assimp-vc***-mt.dll is renamed for any reason



Recommended Visual Studio extensions:
-------------------------------------
- Smart Command Line Arguments: https://marketplace.visualstudio.com/items?itemName=MBulli.SmartCommandlineArguments
- GLSL Language Integration: https://github.com/danielscherzer/GLSL

