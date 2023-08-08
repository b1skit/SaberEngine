// © 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>
#include <GL/wglew.h> // Windows-specific GL functions and macros
#include <GL/GL.h> // Must follow glew.h

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"

#include "Context_OpenGL.h"
#include "Context.h"

#include "Window_Win32.h"

#include "Config.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"


namespace
{
	using en::Config;
	using std::string;
	using std::to_string;


	// The function used to get WGL extensions is an extension itself, thus it needs an OpenGL context. Thus, we create
	// a temp window and context, retrieve and store our function pointers, and then destroy the temp objects
	// More info: https://www.khronos.org/opengl/wiki/Creating_an_OpenGL_Context_(WGL)
	void GetOpenGLExtensionProcessAddresses(re::Context& context)
	{
		const wchar_t* const tempWindowID = L"SaberEngineOpenGLTempWindow";

		WNDCLASSEXW windowClass = {};

		windowClass.cbSize = sizeof(WNDCLASSEX);
		windowClass.style = CS_OWNDC;
		windowClass.lpfnWndProc = (WNDPROC)DefWindowProcA; // Window message handler function pointer
		windowClass.hInstance = GetModuleHandle(0); // Handle to the instance containing the window procedure
		windowClass.lpszClassName = tempWindowID; // Set the unique window identifier

		const ATOM registerResult = RegisterClassExW(&windowClass);
		SEAssert("Failed to register temp OpenGL window", registerResult);

		const wchar_t* const tempWindowTitle = L"Saber Engine Temp OpenGL Window";

		HWND tempWindow = ::CreateWindowExW(
			0,
			windowClass.lpszClassName,
			tempWindowTitle,
			0,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			windowClass.hInstance,
			0);
		SEAssert("Failed to create dummy OpenGL window", tempWindow);

		// These don't matter, we set actual values later via the wgl extension functions
		PIXELFORMATDESCRIPTOR pfd;
		pfd.nSize = sizeof(pfd);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cAlphaBits = 8;
		pfd.iLayerType = PFD_MAIN_PLANE;
		pfd.cDepthBits = 24;
		pfd.cStencilBits = 8;

		HDC tempDeviceContext = ::GetDC(tempWindow); // Get the device context

		int pxFormat = ::ChoosePixelFormat(tempDeviceContext, &pfd);
		SEAssert("Failed to find a suitable pixel format", pxFormat);

		if (!::SetPixelFormat(tempDeviceContext, pxFormat, &pfd))
		{
			SEAssertF("Failed to set the pixel format");
		}

		HGLRC tempRenderContext = ::wglCreateContext(tempDeviceContext);
		SEAssert("Failed to create a dummy OpenGL rendering context", tempRenderContext);

		if (!::wglMakeCurrent(tempDeviceContext, tempRenderContext))
		{
			SEAssertF("Failed to activate dummy OpenGL rendering context");
		}

		opengl::Context::PlatformParams* platformParams = 
			context.GetPlatformParams()->As<opengl::Context::PlatformParams*>();


		platformParams->wglCreateContextAttribsARBFn =
			(opengl::Context::PlatformParams::wglCreateContextAttribsARB_type*)::wglGetProcAddress("wglCreateContextAttribsARB");
		platformParams->wglChoosePixelFormatARBFn =
			(opengl::Context::PlatformParams::wglChoosePixelFormatARB_type*)::wglGetProcAddress("wglChoosePixelFormatARB");

		// Cleanup:
		::wglMakeCurrent(tempDeviceContext, 0);
		::wglDeleteContext(tempRenderContext);
		::ReleaseDC(tempWindow, tempDeviceContext);
		::DestroyWindow(tempWindow);
	}
}


namespace opengl
{
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
			srcMsg = "Unknown ENUM: " + std::format("{:x}", source);
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


	void Context::Create(re::Context& context)
	{
		GetOpenGLExtensionProcessAddresses(context);

		en::Window* window = en::CoreEngine::Get()->GetWindow();
		SEAssert("Window pointer cannot be null", window);

		win32::Window::PlatformParams* windowPlatParams = 
			window->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		opengl::Context::PlatformParams* contextPlatParams =
			context.GetPlatformParams()->As<opengl::Context::PlatformParams*>();

		// Get the Device Context Handle
		contextPlatParams->m_hDeviceContext = GetDC(windowPlatParams->m_hWindow); 

		// Now we can choose a pixel format using wglChoosePixelFormatARB:
		int pixel_format_attribs[] = {
			WGL_DRAW_TO_WINDOW_ARB,     GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB,     GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB,      GL_TRUE,
			WGL_ACCELERATION_ARB,       WGL_FULL_ACCELERATION_ARB,
			WGL_PIXEL_TYPE_ARB,         WGL_TYPE_RGBA_ARB,
			WGL_COLOR_BITS_ARB,         32,
			WGL_DEPTH_BITS_ARB,         24,
			WGL_STENCIL_BITS_ARB,       8,
			0
		};

		int pixel_format;
		UINT num_formats;
		contextPlatParams->wglChoosePixelFormatARBFn(
			contextPlatParams->m_hDeviceContext, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
		if (!num_formats)
		{
			SEAssertF("Failed to set the OpenGL pixel format");
		}

		PIXELFORMATDESCRIPTOR pfd;
		DescribePixelFormat(contextPlatParams->m_hDeviceContext, pixel_format, sizeof(pfd), &pfd);
		if (!SetPixelFormat(contextPlatParams->m_hDeviceContext, pixel_format, &pfd))
		{
			SEAssertF("Failed to set the OpenGL pixel format");
		}

		// Specify our OpenGL core profile context version
		const int glMajorVersion = 4;
		const int glMinorVersion = 6;
		int glAttribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, glMajorVersion,
			WGL_CONTEXT_MINOR_VERSION_ARB, glMinorVersion,
			WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0,
		};

		contextPlatParams->m_glRenderContext = 
			contextPlatParams->wglCreateContextAttribsARBFn(contextPlatParams->m_hDeviceContext, 0, glAttribs);
		SEAssert("Failed to create OpenGL context", contextPlatParams->m_glRenderContext);

		if (!wglMakeCurrent(contextPlatParams->m_hDeviceContext, contextPlatParams->m_glRenderContext))
		{
			SEAssertF("Failed to activate OpenGL rendering context");
		}

		// Verify the context version:
		int glMajorVersionCheck = 0;
		int glMinorVersionCheck = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersionCheck);
		glGetIntegerv(GL_MINOR_VERSION, &glMinorVersionCheck);

		SEAssert("Reported OpenGL version does not match the version set",
			glMajorVersion == glMajorVersionCheck && glMinorVersion == glMinorVersionCheck);

		LOG("Using OpenGL version %d.%d", glMajorVersionCheck, glMinorVersionCheck);

		context.GetSwapChain().SetVSyncMode(Config::Get()->GetValue<bool>("vsync"));

		// Create the (implied) swap chain
		context.GetSwapChain().Create();
		

		// Initialize glew:
		::glewExperimental = GL_TRUE; // Expose OpenGL 3.x+ interfaces
		const GLenum glStatus = glewInit();
		SEAssert("glewInit failed", glStatus == GLEW_OK);

		// Debugging:
		if (en::Config::Get()->GetValue<int>(en::Config::k_debugLevelCmdLineArg) >= 1)
		{
			// Configure OpenGL logging:
			::glEnable(GL_DEBUG_OUTPUT);
			::glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);	// Make the error callback immediately
			::glDebugMessageCallback(GLMessageCallback, 0);
		}

		// Global OpenGL settings:
		::glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		::glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
		::glEnable(GL_SCISSOR_TEST);

		// Setup our ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = re::k_imguiIniPath;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		::ImGui_ImplWin32_Init(windowPlatParams->m_hWindow);

		const string imguiGLSLVersionString = "#version 130";
		::ImGui_ImplOpenGL3_Init(imguiGLSLVersionString.c_str());
	}


	void Context::Destroy(re::Context& context)
	{
		opengl::Context::PlatformParams* contextPlatformParams =
			context.GetPlatformParams()->As<opengl::Context::PlatformParams*>();
		if (!contextPlatformParams)
		{
			return;
		}

		// Imgui cleanup
		::ImGui_ImplOpenGL3_Shutdown();
		::ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		
		::wglMakeCurrent(NULL, NULL); // Make the rendering context not current  

		win32::Window::PlatformParams* windowPlatformParams = 
			en::CoreEngine::Get()->GetWindow()->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		::ReleaseDC(windowPlatformParams->m_hWindow, contextPlatformParams->m_hDeviceContext); // Release device context
		::wglDeleteContext(contextPlatformParams->m_glRenderContext); // Delete the rendering context

		// NOTE: We must destroy anything that holds a parameter block before the ParameterBlockAllocator is destroyed, 
		// as parameter blocks call the ParameterBlockAllocator in their destructor
		context.GetParameterBlockAllocator().Destroy();
	}


	void Context::Present(re::Context const& context)
	{
		opengl::Context::PlatformParams* platformParams =
			context.GetPlatformParams()->As<opengl::Context::PlatformParams*>();

		::SwapBuffers(platformParams->m_hDeviceContext);
	}


	void Context::SetPipelineState(re::Context const& context, gr::PipelineState const& pipelineState)
	{
		opengl::Context::SetCullingMode(pipelineState.GetFaceCullingMode());
		opengl::Context::SetBlendMode(pipelineState.GetSrcBlendMode(), pipelineState.GetDstBlendMode());
		opengl::Context::SetDepthTestMode(pipelineState.GetDepthTestMode());
		opengl::Context::SetDepthWriteMode(pipelineState.GetDepthWriteMode());
		opengl::Context::SetColorWriteMode(pipelineState.GetColorWriteMode());
		opengl::Context::ClearTargets(pipelineState.GetClearTarget()); // Clear AFTER setting color/depth modes
	}


	void Context::SetCullingMode(gr::PipelineState::FaceCullingMode const& mode)
	{
		if (mode != gr::PipelineState::FaceCullingMode::Disabled)
		{
			glEnable(GL_CULL_FACE);
		}

		switch (mode)
		{
		case gr::PipelineState::FaceCullingMode::Disabled:
		{
			glDisable(GL_CULL_FACE);
		}
		break;
		case gr::PipelineState::FaceCullingMode::Front:
		{
			glCullFace(GL_FRONT);
		}
		break;
		case gr::PipelineState::FaceCullingMode::Back:
		{
			glCullFace(GL_BACK);
		}
		break;
		default:
			SEAssertF("Invalid face culling mode");
		}
	}


	void Context::ClearTargets(gr::PipelineState::ClearTarget const& clearTarget)
	{
		switch (clearTarget)
		{
		case gr::PipelineState::ClearTarget::Color:
		{
			glClear(GL_COLOR_BUFFER_BIT);
		}
		break;
		case gr::PipelineState::ClearTarget::Depth:
		{
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		break;
		case gr::PipelineState::ClearTarget::ColorDepth:
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
		break;
		case gr::PipelineState::ClearTarget::None:
		{
			return;
		}
		break;
		default:
			SEAssertF("Invalid face clear target");
		}
	}
	

	void Context::SetBlendMode(gr::PipelineState::BlendMode const& src, gr::PipelineState::BlendMode const& dst)
	{
		if (src == gr::PipelineState::BlendMode::Disabled)
		{
			SEAssert("Must disable blending for both source and destination", src == dst);

			glDisable(GL_BLEND);
			return;
		}

		glEnable(GL_BLEND);

		GLenum sFactor = GL_ONE;
		GLenum dFactor = GL_ZERO;

		auto SetGLBlendFactor = [](
			gr::PipelineState::BlendMode const& platformBlendMode,
			GLenum& blendFactor,
			bool isSrc
			)
		{
			switch (platformBlendMode)
			{
			case gr::PipelineState::BlendMode::Zero:
			{
				blendFactor = GL_ZERO;
			}
			break;
			case gr::PipelineState::BlendMode::One:
			{
				blendFactor = GL_ONE;
			}
			break;
			case gr::PipelineState::BlendMode::SrcColor:
			{
				blendFactor = GL_SRC_COLOR;
			}
			break;
			case gr::PipelineState::BlendMode::OneMinusSrcColor:
			{
				blendFactor = GL_ONE_MINUS_SRC_COLOR;
			}
			break;
			case gr::PipelineState::BlendMode::DstColor:
			{
				blendFactor = GL_DST_COLOR;
			}
			break;
			case gr::PipelineState::BlendMode::OneMinusDstColor:
			{
				blendFactor = GL_ONE_MINUS_DST_COLOR;
			}
			case gr::PipelineState::BlendMode::SrcAlpha:
			{
				blendFactor = GL_SRC_ALPHA;
			}
			case gr::PipelineState::BlendMode::OneMinusSrcAlpha:
			{
				blendFactor = GL_ONE_MINUS_SRC_ALPHA;
			}
			case gr::PipelineState::BlendMode::DstAlpha:
			{
				blendFactor = GL_DST_ALPHA;
			}
			case gr::PipelineState::BlendMode::OneMinusDstAlpha:
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

		if (src != gr::PipelineState::BlendMode::Default)
		{
			SetGLBlendFactor(src, sFactor, true);
		}

		if (dst != gr::PipelineState::BlendMode::Default)
		{
			SetGLBlendFactor(dst, dFactor, false);
		}

		glBlendFunc(sFactor, dFactor);
	}


	void Context::SetDepthTestMode(gr::PipelineState::DepthTestMode const& mode)
	{
		if (mode == gr::PipelineState::DepthTestMode::Always)
		{
			glDisable(GL_DEPTH_TEST);
			return;
		}

		glEnable(GL_DEPTH_TEST);
		
		GLenum depthMode = GL_LESS;
		switch (mode)
		{
		case gr::PipelineState::DepthTestMode::Default:
		case gr::PipelineState::DepthTestMode::Less:
		{
			depthMode = GL_LESS;
		}
		break;
		case gr::PipelineState::DepthTestMode::Equal:
		{
			depthMode = GL_EQUAL;
		}
		break;
		case gr::PipelineState::DepthTestMode::LEqual:
		{
			depthMode = GL_LEQUAL;
		}
		break;
		case gr::PipelineState::DepthTestMode::Greater:
		{
			depthMode = GL_GREATER;
		}
		break;
		case gr::PipelineState::DepthTestMode::NotEqual:
		{
			depthMode = GL_NOTEQUAL;
		}
		break;
		case gr::PipelineState::DepthTestMode::GEqual:
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


	void opengl::Context::SetDepthWriteMode(gr::PipelineState::DepthWriteMode const& mode)
	{
		switch (mode)
		{
		case gr::PipelineState::DepthWriteMode::Enabled:
		{
			glDepthMask(GL_TRUE);
		}
		break;
		case gr::PipelineState::DepthWriteMode::Disabled:
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


	void opengl::Context::SetColorWriteMode(gr::PipelineState::ColorWriteMode const& channelModes)
	{
		GLboolean r = channelModes.R == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ? GL_TRUE : GL_FALSE;
		GLboolean g = channelModes.G == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ? GL_TRUE : GL_FALSE;
		GLboolean b = channelModes.B == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ? GL_TRUE : GL_FALSE;
		GLboolean a = channelModes.A == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ? GL_TRUE : GL_FALSE;

		glColorMask(r, g, b, a);
	}


	uint8_t opengl::Context::GetMaxTextureInputs()
	{
		int maxTexInputs;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTexInputs);
		SEAssert("GL_MAX_TEXTURE_IMAGE_UNITS query failed", maxTexInputs > 0);
		return (uint32_t)maxTexInputs;
	}
}