// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"
#include "Sampler.h"
#include "Sampler_DX12.h"
#include "Sampler_OpenGL.h"
#include "Sampler_Platform.h"

#include "Core/Assert.h"


namespace platform
{
	void Sampler::CreatePlatformParams(re::Sampler& sampler)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			sampler.SetPlatformParams(std::make_unique<opengl::Sampler::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			sampler.SetPlatformParams(std::make_unique<dx12::Sampler::PlatformParams>());
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