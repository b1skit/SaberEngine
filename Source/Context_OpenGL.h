// � 2022 Adam Badke. All rights reserved.
#pragma once

#include "Context.h"
#include "Context_Platform.h"




namespace opengl
{
	class Context final : public virtual re::Context
	{
	public:
		void Create() override;
		~Context() override = default;

		// Platform implementations:
		static void Destroy(re::Context& context);

		// Context interface:
		void Present() override;

		// OpenGL-specific interface:
		void SetPipelineState(re::PipelineState const& pipelineState);


	protected:
		Context();
		friend class re::Context;

	private:
		void SetCullingMode(re::PipelineState::FaceCullingMode const& mode);
		void SetDepthTestMode(re::PipelineState::DepthTestMode const& mode);

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
	};
}