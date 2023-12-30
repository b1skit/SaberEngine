// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <GL/glew.h>

#include "Context.h"
#include "Context_Platform.h"


namespace re
{
	class VertexStream;
}

namespace opengl
{
	class Context final : public virtual re::Context
	{
	public:
		[[nodiscard]] void Create(uint64_t currentFrame) override;
		~Context() override = default;

		// Platform implementations:
		static void Destroy(re::Context& context);

		// Context interface:
		void Present() override;

		// OpenGL-specific interface:
		void SetPipelineState(re::PipelineState const& pipelineState);

		static uint64_t ComputeVAOHash(re::VertexStream const* const*, uint8_t count, re::VertexStream const* indexStream);

		GLuint GetCreateVAO(re::VertexStream const* const*, uint8_t count, re::VertexStream const* indexStream);
		

	protected:
		Context();
		friend class re::Context;

	private:
		void SetCullingMode(re::PipelineState::FaceCullingMode mode);
		void SetDepthTestMode(re::PipelineState::DepthTestMode mode);
		void SetFillMode(re::PipelineState const&);

		void GetOpenGLExtensionProcessAddresses();


	private:
		HGLRC m_glRenderContext;
		HDC m_hDeviceContext;

		typedef HGLRC WINAPI wglCreateContextAttribsARB_type(HDC hdc, HGLRC hShareContext, const int* attribList);
		wglCreateContextAttribsARB_type* wglCreateContextAttribsARBFn;

		typedef BOOL WINAPI wglChoosePixelFormatARB_type(
			HDC hdc, 
			const int* piAttribIList, 
			const FLOAT* pfAttribFList, 
			UINT nMaxFormats, 
			int* piFormats, 
			UINT* nNumFormats);
		wglChoosePixelFormatARB_type* wglChoosePixelFormatARBFn;


	private:
		std::unordered_map<uint64_t, GLuint> m_VAOLibrary; // Maps bitmasks of enabled vertex attribute indexes to VAO name
		std::mutex m_VAOLibraryMutex;
	};
}