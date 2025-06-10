// � 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"
#include "Sampler.h"
#include "Sampler_Platform.h"

#include "Core/Assert.h"
#include "Core/Logger.h"


namespace re
{
	core::InvPtr<re::Sampler> Sampler::GetSampler(util::HashKey const& samplerNameHash)
	{
		return re::RenderManager::Get()->GetInventory()->Get<re::Sampler>(samplerNameHash, nullptr);
	}


	core::InvPtr<re::Sampler> Sampler::GetSampler(std::string_view samplerName)
	{
		SEAssert(samplerName.data()[samplerName.size()] == '\0', "std::string_view must be null-terminated for Sampler usage");
		return GetSampler(util::HashKey(samplerName.data()));
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

		samplerLoadContext->m_retentionPolicy = core::RetentionPolicy::Permanent;

		samplerLoadContext->m_samplerName = name;
		samplerLoadContext->m_samplerDesc = samplerDesc;

		return re::RenderManager::Get()->GetInventory()->Get(
				util::HashKey(name), 
				static_pointer_cast<core::ILoadContext<re::Sampler>>(samplerLoadContext));
	}


	core::InvPtr<re::Sampler> Sampler::Create(std::string const& name, SamplerDesc const& samplerDesc)
	{
		return Create(name.c_str(), samplerDesc);
	}


	Sampler::Sampler(char const* name, SamplerDesc const& samplerDesc)
		: core::INamedObject(name)
		, m_samplerDesc{ samplerDesc }
	{
		platform::Sampler::CreatePlatformObject(*this);
	}


	Sampler::~Sampler()
	{
		SEAssert(m_platObj == nullptr,
			"Sampler dtor called, but platform object is not null. Was Destroy() called?");
	}


	void Sampler::Destroy()
	{
		SEAssert(m_platObj->m_isCreated, "Sampler has not been created");
		platform::Sampler::Destroy(*this);
		m_platObj = nullptr;
	}
}