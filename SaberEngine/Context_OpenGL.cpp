// © 2022 Adam Badke. All rights reserved.
#include <SDL.h>
#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h

#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"

#include "Context_OpenGL.h"
#include "Context.h"

#include "Config.h"
#include "DebugConfiguration.h"

using en::Config;
using std::string;
using std::to_string;


namespace opengl
{
	// OpenGL error message helper function: (Enable/disable via BuildConfiguration.h)
#if defined(DEBUG_LOG_OPENGL)
	void GLAPIENTRY GLMessageCallback
	(
			GLenum source,
			GLenum type,
			GLuint id,
			GLenum severity,
			GLsizei length,
			const GLchar* message,
			const void* userParam
	)
	{
		string srcMsg;
		switch (source)
		{
		case GL_DEBUG_SOURCE_API:
			srcMsg = "GL_DEBUG_SOURCE_API";
			break;
		case GL_DEBUG_SOURCE_APPLICATION: 
			srcMsg = "GL_DEBUG_SOURCE_APPLICATION";
				break;

		case GL_DEBUG_SOURCE_THIRD_PARTY:
			srcMsg = "GL_DEBUG_SOURCE_THIRD_PARTY";
				break;
		default:
			srcMsg = "CURRENTLY UNRECOGNIZED ENUM VALUE: " + to_string(source) + " (Todo: Convert to hex!)";
			// If we ever hit this, we should add the enum as a new string
		}
		
		string typeMsg;
		switch (type)
		{
		case GL_DEBUG_TYPE_ERROR:
			typeMsg = "GL_DEBUG_TYPE_ERROR";
			break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			typeMsg = "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR";
			break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			typeMsg = "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR";
			break;
		case GL_DEBUG_TYPE_PORTABILITY:
			typeMsg = "GL_DEBUG_TYPE_PORTABILITY";
			break;
		case GL_DEBUG_TYPE_PERFORMANCE:
			typeMsg = "GL_DEBUG_TYPE_PERFORMANCE";
			break;
		case GL_DEBUG_TYPE_OTHER:
			typeMsg = "GL_DEBUG_TYPE_OTHER";
			break;
		default:
			typeMsg = "UNKNOWN";
		}

		string severityMsg;
		switch (severity)
		{
		#if defined(DEBUG_LOG_OPENGL_NOTIFICATIONS)
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			severityMsg = "NOTIFICATION";
			break;
		#else
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			return; // DO NOTHING
		#endif
		case GL_DEBUG_SEVERITY_LOW :
			severityMsg = "GL_DEBUG_SEVERITY_LOW";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			severityMsg = "GL_DEBUG_SEVERITY_MEDIUM";
			break;
		case GL_DEBUG_SEVERITY_HIGH:
			severityMsg = "GL_DEBUG_SEVERITY_HIGH";
			break;
		default:
			severityMsg = "UNKNOWN";
		}
		
		switch(severity)
		{
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			LOG("\nOpenGL Error Callback:\nSource: %s\nType: %s\nid: %d\nSeverity: %s\nMessage: %s\n",
				srcMsg.c_str(), typeMsg.c_str(), id, severityMsg.c_str(), message);
			break;
		default:
			LOG_ERROR("\nOpenGL Error Callback:\nSource: %s\nType: %s\nid: %d\nSeverity: %s\nMessage: %s\n",
				srcMsg.c_str(), typeMsg.c_str(), id, severityMsg.c_str(), message);
		}

		if (severity == GL_DEBUG_SEVERITY_HIGH)
		{
			SEAssertF("High severity GL error!");
		}		
	}
#endif


	void Context::Create(re::Context& context)
	{
		opengl::Context::PlatformParams* const platformParams =
			dynamic_cast<opengl::Context::PlatformParams*>(context.GetPlatformParams());

		// SDL_INIT_VIDEO automatically inits events, but SDL_INIT_EVENTS included here as a reminder
		SEAssert(
			SDL_GetError(), 
			SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO) == 0);

		
		// Configure SDL before creating a window:
		const int glMajorVersion = 4;
		const int glMinorVersion = 6;
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glMajorVersion);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glMinorVersion);

		SEAssert(
			SDL_GetError(), 
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) >= 0);		

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

		// Specify relative mouse mode
		// https://wiki.libsdl.org/SDL_HINT_MOUSE_RELATIVE_MODE_WARP
		SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0", SDL_HINT_OVERRIDE);
		SDL_SetRelativeMouseMode(SDL_TRUE);	// Lock the mouse to the window
		
		// Create a window:
		const string windowTitle = Config::Get()->GetValue<string>("windowTitle") + " " + 
			Config::Get()->GetValue<string>("commandLineArgs");
		const int xRes = Config::Get()->GetValue<int>("windowXRes");
		const int yRes = Config::Get()->GetValue<int>("windowYRes");
		platformParams->m_glWindow = SDL_CreateWindow
		(
			windowTitle.c_str(),
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			xRes,
			yRes,
			SDL_WINDOW_OPENGL
		);
		SEAssert("Could not create window", platformParams->m_glWindow != NULL);

		// Create an OpenGL context and make it current:
		platformParams->m_glContext = SDL_GL_CreateContext(platformParams->m_glWindow);
		SEAssert("Could not create OpenGL context", platformParams->m_glContext != NULL);

		SEAssert("Failed to make OpenGL context current", 
			SDL_GL_MakeCurrent(platformParams->m_glWindow, platformParams->m_glContext) >= 0);

		// Synchronize buffer swapping with the monitor's vertical refresh (VSync):
		const bool vsyncEnabled = Config::Get()->GetValue<bool>("vsync");
		SDL_GL_SetSwapInterval(static_cast<int>(vsyncEnabled));
		
		// Verify the context version:
		int glMajorVersionCheck = 0;
		int glMinorVersionCheck = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersionCheck);
		glGetIntegerv(GL_MINOR_VERSION, &glMinorVersionCheck);
		
		SEAssert("Reported OpenGL version does not match the version set", 
			glMajorVersion == glMajorVersionCheck && glMinorVersion == glMinorVersionCheck);

		LOG("Using OpenGL version %d.%d", glMajorVersionCheck, glMinorVersionCheck);

		// Initialize glew:
		glewExperimental = GL_TRUE; // Expose OpenGL 3.x+ interfaces
		GLenum glStatus = glewInit();
		SEAssert("glStatus not ok!", glStatus == GLEW_OK);

		// Configure OpenGL logging:
#if defined(DEBUG_LOG_OPENGL)		// Defined in BuildConfiguration.h
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);	// Make the error callback immediately
		glDebugMessageCallback(GLMessageCallback, 0);
#endif

		// Global OpenGL settings:
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);


		// Setup our ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = re::k_imguiIniPath;
		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForOpenGL(platformParams->m_glWindow, platformParams->m_glContext);

		const string imguiGLSLVersionString = "#version 130";
		ImGui_ImplOpenGL3_Init(imguiGLSLVersionString.c_str());
	}


	void Context::Destroy(re::Context& context)
	{
		opengl::Context::PlatformParams* const platformParams =
			dynamic_cast<opengl::Context::PlatformParams*>(context.GetPlatformParams());

		// Imgui cleanup
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		
		SDL_GL_DeleteContext(platformParams->m_glContext);
		SDL_DestroyWindow(platformParams->m_glWindow);
		SDL_Quit(); // Force a shutdown, instead of calling SDL_QuitSubSystem() for each subsystem
	}


	void Context::SwapWindow(re::Context const& context)
	{
		opengl::Context::PlatformParams const* const platformParams =
			dynamic_cast<opengl::Context::PlatformParams const*>(context.GetPlatformParams());

		SDL_GL_SwapWindow(platformParams->m_glWindow);
	}


	void Context::SetCullingMode(platform::Context::FaceCullingMode const& mode)
	{
		if (mode != platform::Context::FaceCullingMode::Disabled)
		{
			glEnable(GL_CULL_FACE);
		}

		switch (mode)
		{
		case platform::Context::FaceCullingMode::Disabled:
		{
			glDisable(GL_CULL_FACE);
		}
		break;
		case platform::Context::FaceCullingMode::Front:
		{
			glCullFace(GL_FRONT);
		}
		break;
		case platform::Context::FaceCullingMode::Back:
		{
			glCullFace(GL_BACK);
		}
		break;
		case platform::Context::FaceCullingMode::FrontBack:
		{
			glCullFace(GL_FRONT_AND_BACK);
		}
		break;
		default:
			SEAssertF("Invalid face culling mode");
		}
	}


	void Context::ClearTargets(platform::Context::ClearTarget const& clearTarget)
	{
		switch (clearTarget)
		{
		case platform::Context::ClearTarget::Color:
		{
			glClear(GL_COLOR_BUFFER_BIT);
		}
		break;
		case platform::Context::ClearTarget::Depth:
		{
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		break;
		case platform::Context::ClearTarget::ColorDepth:
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
		break;
		case platform::Context::ClearTarget::None:
		{
			return;
		}
		break;
		default:
			SEAssertF("Invalid face clear target");
		}
	}
	

	void Context::SetBlendMode(platform::Context::BlendMode const& src, platform::Context::BlendMode const& dst)
	{
		if (src == platform::Context::BlendMode::Disabled)
		{
			SEAssert("Must disable blending for both source and destination", src == dst);

			glDisable(GL_BLEND);
			return;
		}

		glEnable(GL_BLEND);

		GLenum sFactor = GL_ONE;
		GLenum dFactor = GL_ZERO;

		auto SetGLBlendFactor = [](
			platform::Context::BlendMode const& platformBlendMode, 
			GLenum& blendFactor,
			bool isSrc
			)
		{
			switch (platformBlendMode)
			{
			case platform::Context::BlendMode::Zero:
			{
				blendFactor = GL_ZERO;
			}
			break;
			case platform::Context::BlendMode::One:
			{
				blendFactor = GL_ONE;
			}
			break;
			case platform::Context::BlendMode::SrcColor:
			{
				blendFactor = GL_SRC_COLOR;
			}
			break;
			case platform::Context::BlendMode::OneMinusSrcColor:
			{
				blendFactor = GL_ONE_MINUS_SRC_COLOR;
			}
			break;
			case platform::Context::BlendMode::DstColor:
			{
				blendFactor = GL_DST_COLOR;
			}
			break;
			case platform::Context::BlendMode::OneMinusDstColor:
			{
				blendFactor = GL_ONE_MINUS_DST_COLOR;
			}
			case platform::Context::BlendMode::SrcAlpha:
			{
				blendFactor = GL_SRC_ALPHA;
			}
			case platform::Context::BlendMode::OneMinusSrcAlpha:
			{
				blendFactor = GL_ONE_MINUS_SRC_ALPHA;
			}
			case platform::Context::BlendMode::DstAlpha:
			{
				blendFactor = GL_DST_ALPHA;
			}
			case platform::Context::BlendMode::OneMinusDstAlpha:
			{
				blendFactor = GL_ONE_MINUS_DST_ALPHA;
			}
			break;
			default:
			{
				SEAssertF("Invalid blend mode");
			}
			}
		};

		if (src != platform::Context::BlendMode::Default)
		{
			SetGLBlendFactor(src, sFactor, true);
		}

		if (dst != platform::Context::BlendMode::Default)
		{
			SetGLBlendFactor(dst, dFactor, false);
		}

		glBlendFunc(sFactor, dFactor);
	}


	void Context::SetDepthTestMode(platform::Context::DepthTestMode const& mode)
	{
		if (mode == platform::Context::DepthTestMode::Always)
		{
			glDisable(GL_DEPTH_TEST);
			return;
		}

		glEnable(GL_DEPTH_TEST);
		
		GLenum depthMode = GL_LESS;
		switch (mode)
		{
		case platform::Context::DepthTestMode::Default:
		case platform::Context::DepthTestMode::Less:
		{
			depthMode = GL_LESS;
		}
		break;
		case platform::Context::DepthTestMode::Equal:
		{
			depthMode = GL_EQUAL;
		}
		break;
		case platform::Context::DepthTestMode::LEqual:
		{
			depthMode = GL_LEQUAL;
		}
		break;
		case platform::Context::DepthTestMode::Greater:
		{
			depthMode = GL_GREATER;
		}
		break;
		case platform::Context::DepthTestMode::NotEqual:
		{
			depthMode = GL_NOTEQUAL;
		}
		break;
		case platform::Context::DepthTestMode::GEqual:
		{
			depthMode = GL_GEQUAL;
		}
		break;
		default:
		{
			SEAssertF("Invalid depth test mode");
		}
		}

		glDepthFunc(depthMode);
	}


	void opengl::Context::SetDepthWriteMode(platform::Context::DepthWriteMode const& mode)
	{
		switch (mode)
		{
		case platform::Context::DepthWriteMode::Enabled:
		{
			glDepthMask(GL_TRUE);
		}
		break;
		case platform::Context::DepthWriteMode::Disabled:
		{
			glDepthMask(GL_FALSE);
		}
		break;
		default:
		{
			SEAssertF("Invalid depth write mode");
		}
		}
	}


	void opengl::Context::SetColorWriteMode(platform::Context::ColorWriteMode const& channelModes)
	{
		GLboolean r = channelModes.R == platform::Context::ColorWriteMode::ChannelMode::Enabled ? GL_TRUE : GL_FALSE;
		GLboolean g = channelModes.G == platform::Context::ColorWriteMode::ChannelMode::Enabled ? GL_TRUE : GL_FALSE;
		GLboolean b = channelModes.B == platform::Context::ColorWriteMode::ChannelMode::Enabled ? GL_TRUE : GL_FALSE;
		GLboolean a = channelModes.A == platform::Context::ColorWriteMode::ChannelMode::Enabled ? GL_TRUE : GL_FALSE;

		glColorMask(r, g, b, a);
	}


	uint32_t opengl::Context::GetMaxTextureInputs()
	{
		int maxTexInputs;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTexInputs);
		SEAssert("GL_MAX_TEXTURE_IMAGE_UNITS query failed", maxTexInputs > 0);
		return (uint32_t)maxTexInputs;
	}
}