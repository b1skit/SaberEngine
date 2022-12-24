#pragma once

#include "Config.h"
#include "Sampler.h"
#include "Sampler_Platform.h"
#include "Sampler_OpenGL.h"
#include "DebugConfiguration.h"

using en::Config;


namespace platform
{
	// Parameter struct object factory:
	void Sampler::CreatePlatformParams(re::Sampler& sampler)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

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
	}

	void (*Sampler::Create)(re::Sampler&);
	void (*Sampler::Bind)(re::Sampler&, uint32_t textureUnit);
	void (*Sampler::Destroy)(re::Sampler&);

}