// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/RenderManager.h"
#include "Private/Sampler.h"
#include "Private/Sampler_DX12.h"
#include "Private/Sampler_OpenGL.h"
#include "Private/Sampler_Platform.h"

#include "Core/Assert.h"


namespace platform
{
	void Sampler::CreatePlatformObject(re::Sampler& sampler)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			sampler.SetPlatformObject(std::make_unique<opengl::Sampler::PlatObj>());
		}
		break;
		case RenderingAPI::DX12:
		{
			sampler.SetPlatformObject(std::make_unique<dx12::Sampler::PlatObj>());
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