// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"
#include "Sampler.h"
#include "Sampler_Platform.h"

#include "Core/Assert.h"


namespace re
{
	core::InvPtr<re::Sampler> Sampler::GetSampler(util::StringHash const& samplerNameHash)
	{
		return re::RenderManager::Get()->GetInventory()->Get<re::Sampler>(samplerNameHash, nullptr);
	}


	core::InvPtr<re::Sampler> Sampler::GetSampler(char const* samplerName)
	{
		return GetSampler(util::StringHash(samplerName));
	}


	core::InvPtr<re::Sampler> Sampler::GetSampler(std::string const& samplerName)
	{
		return GetSampler(samplerName.c_str());
	}


	core::InvPtr<re::Sampler> Sampler::Create(char const* name, SamplerDesc const& samplerDesc)
	{
		struct LoadContext final : public virtual core::ILoadContext<re::Sampler>
		{
			void OnLoadBegin(core::InvPtr<re::Sampler>& newSampler) override
			{
				LOG(std::format("Creating sampler \"{}\"", m_samplerName).c_str());

				re::RenderManager::Get()->RegisterForCreate(newSampler); // API-layer creation
			}

			std::unique_ptr<re::Sampler> Load(core::InvPtr<re::Sampler>&) override
			{
				return std::unique_ptr<re::Sampler>(new re::Sampler(m_samplerName.c_str(), m_samplerDesc));
			}

			std::string m_samplerName;
			SamplerDesc m_samplerDesc;
		};
		std::shared_ptr<LoadContext> samplerLoadContext = std::make_shared<LoadContext>();

		samplerLoadContext->m_samplerName = name;
		samplerLoadContext->m_samplerDesc = samplerDesc;
		samplerLoadContext->m_isPermanent = true;

		core::InvPtr<re::Sampler> const& newSampler = re::RenderManager::Get()->GetInventory()->Get(
				util::StringHash(name), 
				static_pointer_cast<core::ILoadContext<re::Sampler>>(samplerLoadContext));

		return newSampler;
	}


	core::InvPtr<re::Sampler> Sampler::Create(std::string const& name, SamplerDesc const& samplerDesc)
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
		SEAssert(m_platformParams == nullptr,
			"Sampler dtor called, but platform params is not null. Was Destroy() called?");
	}


	void Sampler::Destroy()
	{
		SEAssert(m_platformParams->m_isCreated, "Sampler has not been created");
		platform::Sampler::Destroy(*this);
		m_platformParams = nullptr;
	}
}