// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/Context.h"
#include "Private/VertexStream.h"


#include <GL/glew.h>

namespace gr
{
	class VertexStream;
}

namespace re
{
	class BindlessResourceManager;
	class VertexBufferInput;
	class RasterizationState;
}

namespace opengl
{
	class Context final : public virtual re::Context
	{
	public:
		~Context() override = default;

		// Context interface:
		void CreateInternal(uint64_t currentFrame) override;
		void UpdateInternal(uint64_t currentFrame) override;
		void DestroyInternal() override;

		void Present() override;

		re::BindlessResourceManager* GetBindlessResourceManager() override;


	public: // OpenGL-specific interface:
		void SetRasterizationState(re::RasterizationState const*);

		static uint64_t ComputeVAOHash(
			std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> const&,
			re::VertexBufferInput const& indexStream);

		GLuint GetCreateVAO(
			std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> const&, 
			re::VertexBufferInput const& indexStream);
		

	protected:
		Context(platform::RenderingAPI api, uint8_t numFramesInFlight, host::Window*);
		friend class re::Context;

	private:
		void SetRasterizerState(re::RasterizationState const*);
		void SetDepthStencilState(re::RasterizationState const*);
		void SetBlendState(re::RasterizationState const*);


	private:
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


	inline re::BindlessResourceManager* Context::GetBindlessResourceManager()
	{
		// OpenGL does not currently support bindless resources
		return nullptr;
	}
}