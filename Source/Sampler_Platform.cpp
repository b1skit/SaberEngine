// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Config.h"
#include "DebugConfiguration.h"
#include "Sampler.h"
#include "Sampler_DX12.h"
#include "Sampler_OpenGL.h"
#include "Sampler_Platform.h"

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
			sampler.m_platformParams = std::make_unique<dx12::Sampler::PlatformParams>(sampler.GetSamplerParams());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}

	void (*Sampler::Create)(re::Sampler&) = nullptr;
	void (*Sampler::Destroy)(re::Sampler&) = nullptr;

}