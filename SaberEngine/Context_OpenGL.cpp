#include <memory>
#include <string>

#include <SDL.h>
#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h

#include "Context_OpenGL.h"
#include "Context.h"

#include "CoreEngine.h"
#include "DebugConfiguration.h"

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
		string output = "\nOpenGL Error Callback:\nSource: ";

		switch (source)
		{
		case GL_DEBUG_SOURCE_API:
			output += "GL_DEBUG_SOURCE_API\n";
			break;
		case GL_DEBUG_SOURCE_APPLICATION: 
			output += "GL_DEBUG_SOURCE_APPLICATION\n";
				break;

		case GL_DEBUG_SOURCE_THIRD_PARTY:
			output += "GL_DEBUG_SOURCE_THIRD_PARTY\n";
				break;
		default:
			output += "CURRENTLY UNRECOGNIZED ENUM VALUE: " + to_string(source) + " (Todo: Convert to hex!)\n"; // If we ever hit this, we should add the enum as a new string
		}

		output += "Type: ";

		switch (type)
		{
		case GL_DEBUG_TYPE_ERROR:
			output += "GL_DEBUG_TYPE_ERROR\n";
			break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			output += "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR\n";
			break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			output += "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR\n";
			break;
		case GL_DEBUG_TYPE_PORTABILITY:
			output += "GL_DEBUG_TYPE_PORTABILITY\n";
			break;
		case GL_DEBUG_TYPE_PERFORMANCE:
			output += "GL_DEBUG_TYPE_PERFORMANCE\n";
			break;
		case GL_DEBUG_TYPE_OTHER:
			output += "GL_DEBUG_TYPE_OTHER\n";
			break;
		default:
			output += "\n";
		}

		output += "id: " + to_string(id) + "\n";

		output += "Severity: ";
		switch (severity)
		{
		#if defined(DEBUG_LOG_OPENGL_NOTIFICATIONS)
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			output += "NOTIFICATION\n";
			break;
		#else
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			return; // DO NOTHING
		#endif
		case GL_DEBUG_SEVERITY_LOW :
				output += "GL_DEBUG_SEVERITY_LOW\n";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			output += "GL_DEBUG_SEVERITY_MEDIUM\n";
			break;
		case GL_DEBUG_SEVERITY_HIGH:
			output += "GL_DEBUG_SEVERITY_HIGH\n";
			break;
		default:
			output += "\n";
		}
		
		output += "Message: " + string(message);

		switch(severity)
		{
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			LOG(output);
			break;
		default:
			LOG_ERROR(output);
		}
		
		if (severity == GL_DEBUG_SEVERITY_HIGH)
		{
			SEAssert("High severity GL error!: " + output, false);
		}		
	}
#endif


	void Context::Create(re::Context& context)
	{
		opengl::Context::PlatformParams* const platformParams =
			dynamic_cast<opengl::Context::PlatformParams*>(context.GetPlatformParams());

		// Video automatically inits events, but included here as a reminder
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
		SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		//SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32); // Crashes if uncommented???

		SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
		SDL_SetRelativeMouseMode(SDL_TRUE);	// Lock the mouse to the window

		//// Make our buffer swap syncronized with the monitor's vertical refresh:
		//SDL_GL_SetSwapInterval(1);

		// Create a window:
		const string windowTitle = 
			SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("windowTitle");
		const int xRes = SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes");
		const int yRes = SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes");
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

		SEAssert(
			"Failed to make OpenGL context current", 
			SDL_GL_MakeCurrent(platformParams->m_glWindow, platformParams->m_glContext) >= 0);
		
		// Verify the context version:
		int glMajorVersionCheck = 0;
		int glMinorVersionCheck = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersionCheck);
		glGetIntegerv(GL_MINOR_VERSION, &glMinorVersionCheck);
		
		SEAssert("Reported OpenGL version does not match the version set", 
			glMajorVersion == glMajorVersionCheck && glMinorVersion == glMinorVersionCheck);

		LOG("Using OpenGL version " + to_string(glMajorVersionCheck) + "." + to_string(glMinorVersionCheck));

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

		// Initialize other OpenGL settings:
		glFrontFace(GL_CCW);				// Counter-clockwise vertex winding (OpenGL default)
		glEnable(GL_DEPTH_TEST);			// Start with Z depth testing enabled
		glDepthFunc(GL_LESS);				// Default is less
		glEnable(GL_CULL_FACE);				// Start with face culling enabled
		glCullFace(GL_BACK);				// Default is backface culling

		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

		// Set inital buffer clear values:
		glClearColor(
			GLclampf(platformParams->m_windowClearColor.r),
			GLclampf(platformParams->m_windowClearColor.g),
			GLclampf(platformParams->m_windowClearColor.b),
			GLclampf(platformParams->m_windowClearColor.a));
		glClearDepth((GLdouble)platformParams->m_depthClearColor);
		
		// Clear both buffers:
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		SDL_GL_SwapWindow(platformParams->m_glWindow);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}


	void Context::Destroy(re::Context& context)
	{
		opengl::Context::PlatformParams* const platformParams =
			dynamic_cast<opengl::Context::PlatformParams*>(context.GetPlatformParams());
		
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
			SEAssert("Invalid face culling mode", false);
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
		default:
			SEAssert("Invalid face clear target",false);
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
				SEAssert("Invalid blend mode", false);
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


	void Context::SetDepthMode(platform::Context::DepthMode const& mode)
	{
		if (mode == platform::Context::DepthMode::Always)
		{
			glDisable(GL_DEPTH_TEST);
			return;
		}

		glEnable(GL_DEPTH_TEST);
		
		GLenum depthMode = GL_LESS;
		switch (mode)
		{
		case platform::Context::DepthMode::Default:
		case platform::Context::DepthMode::Less:
		{
			depthMode = GL_LESS;
		}
		break;
		case platform::Context::DepthMode::Equal:
		{
			depthMode = GL_EQUAL;
		}
		break;
		case platform::Context::DepthMode::LEqual:
		{
			depthMode = GL_LEQUAL;
		}
		break;
		case platform::Context::DepthMode::Greater:
		{
			depthMode = GL_GREATER;
		}
		break;
		case platform::Context::DepthMode::NotEqual:
		{
			depthMode = GL_NOTEQUAL;
		}
		break;
		case platform::Context::DepthMode::GEqual:
		{
			depthMode = GL_GEQUAL;
		}
		break;
		default:
		{
			SEAssert("Invalid depth mode", false);
		}
		}

		glDepthFunc(depthMode);
	}


	uint32_t opengl::Context::GetMaxTextureInputs()
	{
		int maxTexInputs;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTexInputs);
		SEAssert("GL_MAX_TEXTURE_IMAGE_UNITS query failed", maxTexInputs > 0);
		return (uint32_t)maxTexInputs;
	}
}