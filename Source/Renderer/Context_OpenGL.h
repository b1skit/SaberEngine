// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Context.h"
#include "BatchHandle.h"


namespace gr
{
}

namespace re
{
	class BindlessResourceManager;
	class VertexBufferInput;
	class RasterizationState;
	class VertexStream;
}

namespace opengl
{
	class Context final : public virtual re::Context
	{
	public:
		~Context() override = default;

		// Context interface:
		void Create_Platform() override;
		void Update_Platform() override;
		void EndFrame_Platform() override;
		void Destroy_Platform() override;


	public:
		void Present() override;

		re::BindlessResourceManager* GetBindlessResourceManager() override;


	private:
		void CreateAPIResources_Platform() override;


	public: // OpenGL-specific interface:
		void SetRasterizationState(re::RasterizationState const*);

		GLuint GetCreateVAO(gr::StageBatchHandle const&, re::VertexBufferInput const& indexStream);


	protected:
		Context(platform::RenderingAPI api, uint8_t numFramesInFlight, host::Window*);
		friend class re::Context;


	private:
		void SetRasterizerState(re::RasterizationState const*);
		void SetDepthStencilState(re::RasterizationState const*);
		void SetBlendState(re::RasterizationState const*);

		static uint64_t ComputeVAOHash(
			gr::StageBatchHandle::ResolvedVertexBuffers const&,
			re::VertexBufferInput const& indexStream);

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