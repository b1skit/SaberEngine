// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Context_Platform.h"


namespace re
{
	class Context;
}


namespace opengl
{
	class Context
	{
	public:
		struct PlatformParams final : public re::Context::PlatformParams
		{
			HGLRC m_glRenderContext;
			HDC m_hDeviceContext;

			typedef HGLRC WINAPI wglCreateContextAttribsARB_type(HDC hdc, HGLRC hShareContext, const int* attribList);
			wglCreateContextAttribsARB_type* wglCreateContextAttribsARBFn = nullptr;

			typedef BOOL WINAPI wglChoosePixelFormatARB_type(HDC hdc, const int* piAttribIList,
				const FLOAT* pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);
			wglChoosePixelFormatARB_type* wglChoosePixelFormatARBFn = nullptr;
		};


	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void Present(re::Context const& context);
		static void SetPipelineState(re::Context const& context, gr::PipelineState const& pipelineState);
		static uint8_t GetMaxTextureInputs();
		static uint8_t GetMaxColorTargets();

		// OpenGL-specific interface:
		static void SetCullingMode(gr::PipelineState::FaceCullingMode const& mode);
		static void ClearTargets(gr::PipelineState::ClearTarget const& clearTarget);
		static void SetBlendMode(gr::PipelineState::BlendMode const& src, gr::PipelineState::BlendMode const& dst);
		static void SetDepthTestMode(gr::PipelineState::DepthTestMode const& mode);
		static void SetDepthWriteMode(gr::PipelineState::DepthWriteMode const& mode);
		static void SetColorWriteMode(gr::PipelineState::ColorWriteMode const& channelModes);
	};
}