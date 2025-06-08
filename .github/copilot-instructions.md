Any newly created source or header files must include the standard copyright and license information as a comment on the first line.
- e.g. `// © 2025 Adam Badke. All rights reserved.`
- Ensure the comment contains the correct current year.

Do not modify copyright messages in existing files unless there is a specific and documented reason to do so.

Preserve and adhere to existing code style and conventions: The existing project style, patterns, layout, naming conventions, indentation, comment tone, and maximum line width must be maintained.

If the pull request introduces changes that affect the usage, structure, or behavior of the project, the `README.md` file must be updated accordingly to reflect those changes.

New small utility functions (e.g. getters/setters) must be marked as `inline` to avoid unnecessary function call overhead and improve compilation performance.

Only add standard library headers to the pch.h files, never to project source files.

Prefer forward declarations over #includes in header files when possible.

Use dependency injection when possible.

Make the minimal changes necessary.

Be very careful.

The SaberEngine solution has 4 build configurations:
- Debug: Provides the maximum amount of validation and debug information.
- DebugRelease: Provides a balance between a useable level of performance, while still providing debug information and validation. Most development is done in this configuration.
- Profile: Intended for performance benchmarking and profiling. All debug and validation is disabled, except for CPU and GPU markers used by external CPU and GPU performance profling tools.
- Release: Provides the maxiumum performance possible. All debug information, validation, and CPU/GPU markers are disabled.

Keep the .vcxproj, .filters, .props, and .gitignore files, and source file #include directives up to date when changes that might affect them are made (e.g. if any file path, directories, or settings are changed).
- If all projects share the same settings, prefer they be stored in a common .props file if this is valid and allowed to do so.
- Verify these files are correct and valid after modification

SaberEngine has multiple dependencies on other libraries. Most are listed in the README.md file.
- The solution uses vcpkg, NuGet, and Git subtrees to manage dependencies. These configurations must be updated if a change is made that might affect them.
- The ./Source/Dependencies/ directory contains some (but not all) dependency sources.
- Code from a dependency source must never be modified. These must be maintained in the same way they are distributed.

SaberEngine is a realtime rendering engine where high performance is critical:
- The README.md gives a high level description of key features and usage.
- The solution consists of multiple projects: Core (static library), DroidShaderBurner (.exe), Presentation (static library), Renderer (static library), and SaberEngine (.exe).
- It uses a layered architecture, where projects are only allowed to depend on/include/use the layers below them:
	- DroidShaderBurner -> Core.
	- SaberEngine -> Presentation -> Renderer -> Core.
- This pattern is continued within each project, where namespaces are used to further communicate/enforce layering.
	
The application is multi-threaded. Most asyncronous work is handled using the core::ThreadPool.

SaberEngine uses .json files to store configuration used at runtime:
- Shaders, Effects, Techniques, Drawstyles and various other rendering-related configuration
- Scriptable rendering pipelines

SaberEngine supports multiple graphics APIs: OpenGL, D3D12
- GLSL and HLSL shaders are maintained for each API, and are kept as similar in style and functionality as possible.
- Only 1 API is active at once, and is specified at runtime via command line arguments.
- The Renderer project uses a layered architecture to help encapsulate and abstract graphics functionality:
	- gr -> re -> platform -> opengl
	- gr -> re -> platform -> dx12
- Static function pointers in the platform layer are bound to the active graphics API at program startup.

The DroidShaderBurner project creates temporary "_generated" code directories at compile time as a post build step. These are temporary files not committed to the git repository. 

SaberEngine uses C++ 20.