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
		struct PlatformParams final : public virtual platform::Context::PlatformParams
		{
			PlatformParams() = default;
			~PlatformParams() override = default;

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
		static void SetVSyncMode(re::Context const& window, bool enabled);
		static void SetCullingMode(platform::Context::FaceCullingMode const& mode);
		static void ClearTargets(platform::Context::ClearTarget const& clearTarget);
		static void SetBlendMode(platform::Context::BlendMode const& src, platform::Context::BlendMode const& dst);
		static void SetDepthTestMode(platform::Context::DepthTestMode const& mode);
		static void SetDepthWriteMode(platform::Context::DepthWriteMode const& mode);
		static void SetColorWriteMode(platform::Context::ColorWriteMode const& channelModes);
		static uint32_t GetMaxTextureInputs();
	};
}