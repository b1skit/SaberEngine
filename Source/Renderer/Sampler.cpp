// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"
#include "Sampler.h"
#include "Sampler_Platform.h"

#include "Core/Assert.h"


namespace re
{
	std::shared_ptr<re::Sampler> Sampler::GetSampler(util::StringHash const& samplerNameHash)
	{
		re::SceneData* sceneData = re::RenderManager::GetSceneData();
		return sceneData->GetSampler(samplerNameHash);
	}


	std::shared_ptr<re::Sampler> Sampler::GetSampler(char const* samplerName)
	{
		return GetSampler(util::StringHash(samplerName));
	}


	std::shared_ptr<re::Sampler> Sampler::GetSampler(std::string const& samplerName)
	{
		return GetSampler(samplerName.c_str());
	}


	std::shared_ptr<re::Sampler> Sampler::Create(char const* name, SamplerDesc const& samplerDesc)
	{
		re::SceneData* sceneData = re::RenderManager::GetSceneData();
		if (sceneData->SamplerExists(name))
		{
			return sceneData->GetSampler(name);
		}

		std::shared_ptr<re::Sampler> newSampler;
		newSampler.reset(new re::Sampler(name, samplerDesc));

		// Register the Shader with the SceneData object for lifetime management:
		if (sceneData->AddUniqueSampler(newSampler))
		{
			// Register the Shader with the RenderManager (once only), so its API-level object can be created before use
			re::RenderManager::Get()->RegisterForCreate(newSampler);
		}

		return newSampler;
	}


	std::shared_ptr<re::Sampler> Sampler::Create(std::string const& name, SamplerDesc const& samplerDesc)
	{
		return Create(name.c_str(), samplerDesc);
	}


	Sampler::Sampler(char const* name, SamplerDesc const& samplerDesc)
		: core::INamedObject(name)
		, m_samplerDesc{ samplerDesc }
	{
		platform::Sampler::CreatePlatformParams(*this);
	}


	Sampler::~Sampler()
	{
		SEAssert(m_platformParams->m_isCreated, "Sampler has not been created");
		platform::Sampler::Destroy(*this);
		m_platformParams = nullptr;
	}
}