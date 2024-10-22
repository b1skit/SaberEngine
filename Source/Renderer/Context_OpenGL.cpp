// © 2022 Adam Badke. All rights reserved.
#include "Context_OpenGL.h"
#include "Context.h"
#include "EnumTypes_OpenGL.h"
#include "MeshPrimitive.h"
#include "RenderManager.h"
#include "SysInfo_OpenGL.h"
#include "Window_Win32.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/HashUtils.h"

#include <GL/glew.h>
#include <GL/wglew.h> // Windows-specific GL functions and macros
#include <GL/GL.h> // Must follow glew.h


namespace opengl
{
	// The function used to get WGL extensions is an extension itself, thus it needs an OpenGL context. Thus, we create
	// a temp window and context, retrieve and store our function pointers, and then destroy the temp objects
	// More info: https://www.khronos.org/opengl/wiki/Creating_an_OpenGL_Context_(WGL)
	void Context::GetOpenGLExtensionProcessAddresses()
	{
		const wchar_t* const tempWindowID = L"SaberEngineOpenGLTempWindow";

		WNDCLASSEXW windowClass = {};

		windowClass.cbSize = sizeof(WNDCLASSEX);
		windowClass.style = CS_OWNDC;
		windowClass.lpfnWndProc = (WNDPROC)DefWindowProcA; // Window message handler function pointer
		windowClass.hInstance = GetModuleHandle(0); // Handle to the instance containing the window procedure
		windowClass.lpszClassName = tempWindowID; // Set the unique window identifier

		const ATOM registerResult = RegisterClassExW(&windowClass);
		SEAssert(registerResult, "Failed to register temp OpenGL window");

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
		SEAssert(tempWindow, "Failed to create dummy OpenGL window");

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
		SEAssert(pxFormat, "Failed to find a suitable pixel format");

		if (!::SetPixelFormat(tempDeviceContext, pxFormat, &pfd))
		{
			SEAssertF("Failed to set the pixel format");
		}

		HGLRC tempRenderContext = ::wglCreateContext(tempDeviceContext);
		SEAssert(tempRenderContext, "Failed to create a dummy OpenGL rendering context");

		if (!::wglMakeCurrent(tempDeviceContext, tempRenderContext))
		{
			SEAssertF("Failed to activate dummy OpenGL rendering context");
		}

		wglCreateContextAttribsARBFn =
			(opengl::Context::wglCreateContextAttribsARB_type*)::wglGetProcAddress("wglCreateContextAttribsARB");
		wglChoosePixelFormatARBFn =
			(opengl::Context::wglChoosePixelFormatARB_type*)::wglGetProcAddress("wglChoosePixelFormatARB");

		// Cleanup:
		::wglMakeCurrent(tempDeviceContext, 0);
		::wglDeleteContext(tempRenderContext);
		::ReleaseDC(tempWindow, tempDeviceContext);
		::DestroyWindow(tempWindow);
	}


	void GLAPIENTRY GLMessageCallback(
		GLenum source,
		GLenum type,
		GLuint id,
		GLenum severity,
		GLsizei length,
		const GLchar* message,
		const void* userParam)
	{
		std::string srcMsg = "Unknown ENUM: " + std::format("{:x}", source);
		switch (source)
		{
		case GL_DEBUG_SOURCE_API:			srcMsg = "GL_DEBUG_SOURCE_API"; break;
		case GL_DEBUG_SOURCE_APPLICATION:	srcMsg = "GL_DEBUG_SOURCE_APPLICATION"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:	srcMsg = "GL_DEBUG_SOURCE_THIRD_PARTY"; break;
		default: break;			
		}
		
		std::string typeMsg = "UNKNOWN";
		switch (type)
		{
		case GL_DEBUG_TYPE_ERROR:				typeMsg = "GL_DEBUG_TYPE_ERROR"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeMsg = "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:	typeMsg = "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR"; break;
		case GL_DEBUG_TYPE_PORTABILITY:			typeMsg = "GL_DEBUG_TYPE_PORTABILITY"; break;
		case GL_DEBUG_TYPE_PERFORMANCE:			typeMsg = "GL_DEBUG_TYPE_PERFORMANCE"; break;
		case GL_DEBUG_TYPE_OTHER:				typeMsg = "GL_DEBUG_TYPE_OTHER"; break;
		default: break;
		}

		std::string severityMsg = "UNKNOWN";
		switch (severity)
		{
		case GL_DEBUG_SEVERITY_NOTIFICATION:	severityMsg = "NOTIFICATION"; break;
		case GL_DEBUG_SEVERITY_LOW :			severityMsg = "GL_DEBUG_SEVERITY_LOW"; break;
		case GL_DEBUG_SEVERITY_MEDIUM:			severityMsg = "GL_DEBUG_SEVERITY_MEDIUM"; break;
		case GL_DEBUG_SEVERITY_HIGH:			severityMsg = "GL_DEBUG_SEVERITY_HIGH"; break;
		default: break;
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


	Context::Context()
		: m_glRenderContext(nullptr)
		, m_hDeviceContext(nullptr)
		, wglCreateContextAttribsARBFn(nullptr)
		, wglChoosePixelFormatARBFn(nullptr)
	{
	}


	void Context::Create(uint64_t currentFrame)
	{		
		GetOpenGLExtensionProcessAddresses();

		app::Window* window = GetWindow();
		SEAssert(window, "Window pointer cannot be null");

		win32::Window::PlatformParams* windowPlatParams = 
			window->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		// Get the Device Context Handle
		m_hDeviceContext = GetDC(windowPlatParams->m_hWindow);

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
		wglChoosePixelFormatARBFn(m_hDeviceContext, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
		if (!num_formats)
		{
			SEAssertF("Failed to set the OpenGL pixel format");
		}

		PIXELFORMATDESCRIPTOR pfd;
		DescribePixelFormat(m_hDeviceContext, pixel_format, sizeof(pfd), &pfd);
		if (!SetPixelFormat(m_hDeviceContext, pixel_format, &pfd))
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

		m_glRenderContext = wglCreateContextAttribsARBFn(m_hDeviceContext, 0, glAttribs);
		SEAssert(m_glRenderContext, "Failed to create OpenGL context");

		if (!wglMakeCurrent(m_hDeviceContext, m_glRenderContext))
		{
			SEAssertF("Failed to activate OpenGL rendering context");
		}

		// Verify the context version:
		int glMajorVersionCheck = 0;
		int glMinorVersionCheck = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &glMajorVersionCheck);
		glGetIntegerv(GL_MINOR_VERSION, &glMinorVersionCheck);

		SEAssert(glMajorVersion == glMajorVersionCheck && glMinorVersion == glMinorVersionCheck,
			"Reported OpenGL version does not match the version set");

		LOG("Using OpenGL version %d.%d", glMajorVersionCheck, glMinorVersionCheck);

		GetSwapChain().SetVSyncMode(core::Config::Get()->GetValue<bool>(core::configkeys::k_vsyncEnabledKey));

		// Create the (implied) swap chain
		GetSwapChain().Create();
		

		// Initialize glew:
		::glewExperimental = GL_TRUE; // Expose OpenGL 3.x+ interfaces
		const GLenum glStatus = glewInit();
		SEAssert(glStatus == GLEW_OK, "glewInit failed");

		// Disable all debug messages to prevent spam. We'll selectively re-enable them if/when needed
		glDebugMessageControl(
			GL_DONT_CARE,	// Source
			GL_DONT_CARE,	// Type
			GL_DONT_CARE,	// Severity
			0,				// Number of message ids being enabled/disabled
			nullptr,		// Ptr to an array of id integers to enable/disable. nullptr = all
			false);			// Enable/disable state

		// Debugging:
		const int debugLevel = core::Config::Get()->GetValue<int>(core::configkeys::k_debugLevelCmdLineArg);
		if (debugLevel >= 1)
		{
			// All debug levels get all high severity messages
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, NULL, true);

			LOG("Debug level %d: Enabled OpenGL high severity messages", debugLevel);

			// Debug levels 2+ get medium severity messages
			if (debugLevel >= 2)
			{
				glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM, 0, NULL, true);				

				LOG("Debug level %d: Enabled OpenGL medium severity messages", debugLevel);
			}

			// Debug levels 3+ get low and notification severity messages
			if (debugLevel >= 3)
			{
				glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, NULL, true);

				// We omit the GL_DEBUG_TYPE_PUSH_GROUP/GL_DEBUG_TYPE_POP_GROUP because they're very spammy.
				glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, true);
				glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, true);
				glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, true);
				glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PORTABILITY, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, true);
				glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PERFORMANCE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, true);
				glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_MARKER, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, true);
				glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, true);

				LOG("Debug level %d: Enabled OpenGL low & notification severity messages", debugLevel);
			}

			// Configure OpenGL logging:
			::glEnable(GL_DEBUG_OUTPUT);
			::glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);	// Make the error callback immediately
			::glDebugMessageCallback(GLMessageCallback, 0);
		}

		// Global OpenGL settings:
		::glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		::glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
		::glEnable(GL_SCISSOR_TEST);

		// Call our opengl::SysInfo members while we're on the main thread to cache their values, before any systems
		// that might use them
		opengl::SysInfo::GetMaxRenderTargets();
		opengl::SysInfo::GetMaxTextureBindPoints();
		opengl::SysInfo::GetMaxVertexAttributes();

		// OpenGL-specific:
		opengl::SysInfo::GetUniformBufferOffsetAlignment();
		opengl::SysInfo::GetShaderStorageBufferOffsetAlignment();
		opengl::SysInfo::GetMaxAnisotropy();

		// Buffer Allocator:
		m_bufferAllocator = re::BufferAllocator::Create();
		m_bufferAllocator->Initialize(currentFrame);
	}


	void Context::Destroy(re::Context& reContext)
	{
		opengl::Context& context = dynamic_cast<opengl::Context&>(reContext);
	
		::wglMakeCurrent(NULL, NULL); // Make the rendering context not current  

		win32::Window::PlatformParams* windowPlatformParams = 
			reContext.GetWindow()->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		::ReleaseDC(windowPlatformParams->m_hWindow, context.m_hDeviceContext); // Release device context
		::wglDeleteContext(context.m_glRenderContext); // Delete the rendering context

		// NOTE: We must destroy anything that holds a buffer before the BufferAllocator is destroyed, 
		// as buffers call the BufferAllocator in their destructor
		context.GetBufferAllocator()->Destroy();

		// Destroy VAO library:
		{
			std::lock_guard<std::mutex> lock(context.m_VAOLibraryMutex);

			for (auto& vao : context.m_VAOLibrary)
			{
				glDeleteVertexArrays(1, &vao.second);
				vao.second = 0;
			}
			context.m_VAOLibrary.clear();
		}
	}


	void Context::Present()
	{
		::SwapBuffers(m_hDeviceContext);
	}


	void Context::SetPipelineState(re::PipelineState const* pipelineState)
	{
		if (pipelineState)
		{
			// Rasterizer state:
			SetFillMode(pipelineState);
			SetCullingMode(pipelineState->GetFaceCullingMode());
			SEAssert(pipelineState->GetMultiSampleEnabled() == false, "TODO: Handle this");
			SEAssert(pipelineState->GetForcedSampleCount() == 0, "TODO: Handle this");
			SEAssert(pipelineState->GetConservativeRaster() == false , "TODO: Handle this");

			// Depth stencil state:
			SetDepthTestEnable(pipelineState->GetDepthTestEnabled());
			SetDepthWriteMask(pipelineState->GetDepthWriteMask());
			SetDepthComparison(pipelineState->GetDepthComparison());
			SetDepthBias(pipelineState);
			SetDepthClip(pipelineState->GetDepthClipEnabled());
			SetStencilEnable(pipelineState->GetStencilEnabled());
			SetStencilOps(pipelineState);
		}
	}


	void Context::SetFillMode(re::PipelineState const* pipelineState)
	{
		GLenum fillMode = GL_FILL;
		switch (pipelineState->GetFillMode())
		{
		case re::PipelineState::FillMode::Solid:
		{
			fillMode = GL_FILL;
		}
		break;
		case re::PipelineState::FillMode::Wireframe:
		{
			fillMode = GL_LINE;
		}
		break;
		default: SEAssertF("Invalid fill mode");
		}

		glPolygonMode(GL_FRONT_AND_BACK, fillMode);
	}


	void Context::SetCullingMode(re::PipelineState::FaceCullingMode mode)
	{
		if (mode != re::PipelineState::FaceCullingMode::Disabled)
		{
			glEnable(GL_CULL_FACE);
		}

		switch (mode)
		{
		case re::PipelineState::FaceCullingMode::Disabled:
		{
			glDisable(GL_CULL_FACE);
		}
		break;
		case re::PipelineState::FaceCullingMode::Front:
		{
			glCullFace(GL_FRONT);
		}
		break;
		case re::PipelineState::FaceCullingMode::Back:
		{
			glCullFace(GL_BACK);
		}
		break;
		default:
			SEAssertF("Invalid face culling mode");
		}
	}


	void Context::SetDepthTestEnable(bool isEnabled)
	{
		if (isEnabled)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}
	}


	void Context::SetDepthWriteMask(re::PipelineState::DepthWriteMask depthWriteMask)
	{
		switch (depthWriteMask)
		{
		case re::PipelineState::DepthWriteMask::Zero:
		{
			glDepthMask(GL_FALSE);
		}
		break;
		case re::PipelineState::DepthWriteMask::All:
		{
			glDepthMask(GL_TRUE);
		}
		break;
		default: SEAssertF("Invalid depth write mask");
		}
	}


	void Context::SetDepthBias(re::PipelineState const* pipelineState)
	{
		const re::PipelineState::PrimitiveTopologyType topologyType = pipelineState->GetPrimitiveTopologyType();
		const int depthBias = pipelineState->GetDepthBias();

		if (depthBias == 0)
		{
			switch (topologyType)
			{
			case re::PipelineState::PrimitiveTopologyType::Triangle:
			{
				glDisable(GL_POLYGON_OFFSET_FILL);
			}
			break;
			case re::PipelineState::PrimitiveTopologyType::Point:
			{
				glDisable(GL_POLYGON_OFFSET_POINT);
			}
			break;
			case re::PipelineState::PrimitiveTopologyType::Line:
			{
				glDisable(GL_POLYGON_OFFSET_LINE);
			}
			break;
			case re::PipelineState::PrimitiveTopologyType::Patch:
			default: SEAssertF("Invalid topology type");
			}
		}
		else
		{
			switch (topologyType)
			{
			case re::PipelineState::PrimitiveTopologyType::Triangle:
			{
				glEnable(GL_POLYGON_OFFSET_FILL);
			}
			break;
			case re::PipelineState::PrimitiveTopologyType::Point:
			{
				glEnable(GL_POLYGON_OFFSET_POINT);
			}
			break;
			case re::PipelineState::PrimitiveTopologyType::Line:
			{
				glEnable(GL_POLYGON_OFFSET_LINE);
			}
			break;
			case re::PipelineState::PrimitiveTopologyType::Patch:
			default: SEAssertF("Invalid topology type");
			}

			SEAssertF("TODO: If you hit this, this is the first time this code has been tested - test that it works!");

			const GLfloat factor = pipelineState->GetSlopeScaledDepthBias();
			const GLfloat units = depthBias / static_cast<float>(glm::pow(2.f, 24.f)); // TODO: This should depend on the depth buffer format?
			glPolygonOffset(factor, units);
		}		
	}

	
	void Context::SetDepthClip(bool doEnable)
	{
		// Somewhat counter-intuitively, enabling depth clamping disables depth clipping
		// https://www.khronos.org/opengl/wiki/Vertex_Post-Processing
		if (doEnable)
		{
			glDisable(GL_DEPTH_CLAMP);			
		}
		else
		{
			SEAssertF("TODO: If you hit this, this is the first time this code has been tested - test that it works!");
			glEnable(GL_DEPTH_CLAMP);
		}
	}


	void Context::SetStencilEnable(bool doEnable)
	{
		if (doEnable)
		{
			SEAssertF("TODO: If you hit this, this is the first time this code has been tested - test that it works!");
			glEnable(GL_STENCIL_TEST);
		}
		else
		{
			glDisable(GL_STENCIL_TEST);
		}
	}


	constexpr GLenum ComparisonFuncToGLEnum(re::PipelineState::ComparisonFunc comparisonFunc)
	{
		switch (comparisonFunc)
		{
		case re::PipelineState::ComparisonFunc::Less: return GL_LESS;
		case re::PipelineState::ComparisonFunc::Never: return GL_NEVER;
		case re::PipelineState::ComparisonFunc::Equal: return GL_EQUAL;
		case re::PipelineState::ComparisonFunc::LEqual: return GL_LEQUAL;
		case re::PipelineState::ComparisonFunc::Greater: return GL_GREATER;
		case re::PipelineState::ComparisonFunc::NotEqual: return GL_NOTEQUAL;
		case re::PipelineState::ComparisonFunc::GEqual: return GL_GEQUAL;
		case re::PipelineState::ComparisonFunc::Always: return GL_ALWAYS;
		}
		return GL_ALWAYS; // This should never happen
	}


	constexpr GLenum StencilOpToGLEnum(re::PipelineState::StencilOp stencilOp)
	{
		switch (stencilOp)
		{
		case re::PipelineState::StencilOp::Keep: return GL_KEEP;
		case re::PipelineState::StencilOp::Zero: return GL_ZERO;
		case re::PipelineState::StencilOp::Replace: return GL_REPLACE;
		case re::PipelineState::StencilOp::IncrementSaturate: return GL_INCR;
		case re::PipelineState::StencilOp::DecrementSaturate: return GL_DECR;
		case re::PipelineState::StencilOp::Invert: return GL_INVERT;
		case re::PipelineState::StencilOp::Increment: return GL_INCR_WRAP;
		case re::PipelineState::StencilOp::Decrement: return GL_DECR_WRAP;
		}
		return GL_KEEP; // This should never happen
	}


	void Context::SetStencilOps(re::PipelineState const* pipelineState)
	{
		re::PipelineState::StencilOpDesc const& frontDesc = pipelineState->GetFrontFaceStencilOpDesc();
		re::PipelineState::StencilOpDesc const& backDesc = pipelineState->GetBackFaceStencilOpDesc();

		glStencilOpSeparate(GL_FRONT,
			StencilOpToGLEnum(frontDesc.m_failOp),
			StencilOpToGLEnum(frontDesc.m_depthFailOp),
			StencilOpToGLEnum(frontDesc.m_passOp));

		glStencilOpSeparate(GL_BACK,
			StencilOpToGLEnum(backDesc.m_failOp),
			StencilOpToGLEnum(backDesc.m_depthFailOp),
			StencilOpToGLEnum(backDesc.m_passOp));
	}

	
	void Context::SetDepthComparison(re::PipelineState::ComparisonFunc mode)
	{
		glDepthFunc(ComparisonFuncToGLEnum(mode));
	}


	uint64_t Context::ComputeVAOHash(
		std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> const& vertexBuffers,
		re::VertexBufferInput const& indexBuffer)
	{
		uint64_t vaoHash = 0;

		uint32_t bitmask = 0; // Likely only needs to be 16 bits wide, max
		for (uint8_t streamIdx = 0; streamIdx < gr::VertexStream::k_maxVertexStreams; streamIdx++)
		{
			// We assume vertex streams will be tightly packed, with streams of the same type stored consecutively
			re::Buffer const* vertexBuffer = vertexBuffers[streamIdx].m_buffer;
			if (vertexBuffer == nullptr)
			{
				SEAssert(streamIdx > 0, "Failed to find a valid vertex stream");
				break;
			}

			bitmask |= (1 << vertexBuffers[streamIdx].m_bindSlot);		

			util::AddDataToHash(
				vaoHash, static_cast<uint64_t>(vertexBuffers[streamIdx].m_view.m_stream.m_dataType));
			util::AddDataToHash(vaoHash, vertexBuffers[streamIdx].m_view.m_stream.m_isNormalized);

			// Note: We assume all vertex streams have a relative offset of 0, so we don't (currently) include it in
			// the hash here
		}

		if (indexBuffer.m_buffer)
		{			
			util::AddDataToHash(vaoHash, static_cast<uint64_t>(indexBuffer.m_view.m_stream.m_dataType));
			util::AddDataToHash(vaoHash, indexBuffer.m_view.m_stream.m_isNormalized);
		}

		util::AddDataToHash(vaoHash, bitmask);

		return vaoHash;
	}


	GLuint Context::GetCreateVAO(
		std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> const& vertexBuffers,
		re::VertexBufferInput const& indexStream)
	{
		const uint64_t vaoHash = ComputeVAOHash(vertexBuffers, indexStream);

		{
			std::lock_guard<std::mutex> lock(m_VAOLibraryMutex);

			if (!m_VAOLibrary.contains(vaoHash))
			{
				GLuint newVAO = 0;
				glGenVertexArrays(1, &newVAO);
				SEAssert(newVAO != 0, "Failed to create VAO");

				m_VAOLibrary.emplace(vaoHash, newVAO);

				glBindVertexArray(newVAO);

				std::string objectLabel; // Debug name to visually identify our VAOs

				for (uint8_t streamIdx = 0; streamIdx < gr::VertexStream::k_maxVertexStreams; streamIdx++)
				{
					// We assume vertex streams will be tightly packed, with streams of the same type stored consecutively
					if (vertexBuffers[streamIdx].m_buffer == nullptr)
					{
						SEAssert(streamIdx > 0, "Failed to find a valid vertex stream");
						break;
					}

					const uint8_t slotIdx = vertexBuffers[streamIdx].m_bindSlot;

					glEnableVertexArrayAttrib(newVAO, slotIdx);

					// Associate the vertex attribute and binding indexes for the VAO
					glVertexArrayAttribBinding(
						newVAO,
						slotIdx,	// Attribute index [0, GL_MAX_VERTEX_ATTRIBS - 1] to associated w/a vertex buffer binding
						slotIdx);	// Binding index [0, GL_MAX_VERTEX_ATTRIB_BINDINGS - 1] to associate w/a vertex attribute

					// Relative offset specifies the distance btween elements within the buffer.
					// Note: If this is ever != 0, update opengl::Context::ComputeVAOHash to include the offset
					constexpr uint32_t k_relativeOffset = 0;
					
					// Define our vertex layout:
					glVertexAttribFormat(
						slotIdx,																		// Attribute index
						DataTypeToNumComponents(vertexBuffers[streamIdx].m_view.m_stream.m_dataType),	// size: 1/2/3/4 						
						DataTypeToGLDataType(vertexBuffers[streamIdx].m_view.m_stream.m_dataType),		// Data type
						vertexBuffers[streamIdx].m_view.m_stream.m_isNormalized,						// Normalize data?
						k_relativeOffset);							// relativeOffset: Distance between buffer elements

					objectLabel = std::format("{} {}", objectLabel, slotIdx);
				}

				// Renderdoc name for the VAO
				glObjectLabel(
					GL_VERTEX_ARRAY,
					newVAO,
					-1,
					std::format("VAO {}, Slots: {}, hash: {}", newVAO, objectLabel, vaoHash).c_str());

				glBindVertexArray(0); // Cleanup
			}
		}
		return m_VAOLibrary.at(vaoHash);
	}
}