#pragma once

#include "CoreEngine.h"
#include "Sampler.h"
#include "Sampler_Platform.h"
#include "Sampler_OpenGL.h"
#include "DebugConfiguration.h"


namespace platform
{
	// Parameter struct object factory:
	void Sampler::PlatformParams::CreatePlatformParams(gr::Sampler& sampler)
	{
		const platform::RenderingAPI& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			sampler.m_platformParams = std::make_unique<opengl::Sampler::PlatformParams>(sampler.GetSamplerParams());
		}
		break;
		case RenderingAPI::DX12:
		{
			SEAssertF("DX12 is not yet supported");
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}

		return;
	}

	void (*Sampler::Create)(gr::Sampler&);
	void (*Sampler::Bind)(gr::Sampler const&, uint32_t textureUnit, bool doBind);
	void (*Sampler::Destroy)(gr::Sampler&);

}