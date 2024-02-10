// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
#include "RenderManager.h"
#include "Sampler.h"
#include "Sampler_Platform.h"


namespace re
{
	// Static members:
	std::unique_ptr<std::unordered_map<std::string, std::shared_ptr<re::Sampler>>> Sampler::m_samplerLibrary = nullptr;
	std::recursive_mutex Sampler::m_samplerLibraryMutex;


	bool re::Sampler::SamplerDesc::operator==(SamplerDesc const& rhs) const
	{
		return m_filterMode == rhs.m_filterMode &&
			m_edgeModeU == rhs.m_edgeModeU &&
			m_edgeModeV == rhs.m_edgeModeV &&
			m_edgeModeW == rhs.m_edgeModeW &&
			m_mipLODBias == rhs.m_mipLODBias &&
			m_maxAnisotropy == rhs.m_maxAnisotropy &&
			m_comparisonFunc == rhs.m_comparisonFunc &&
			m_borderColor == rhs.m_borderColor &&
			m_minLOD == rhs.m_minLOD &&
			m_maxLOD == rhs.m_maxLOD;
	}


	std::shared_ptr<re::Sampler> const Sampler::GetSampler(std::string const& samplerName)
	{
		{
			std::unique_lock<std::recursive_mutex> samplerLock(m_samplerLibraryMutex);

			if (Sampler::m_samplerLibrary == nullptr)
			{
				Sampler::m_samplerLibrary = std::make_unique<std::unordered_map<std::string, std::shared_ptr<Sampler>>>();

				// Pre-add some samplers we know we need:

				constexpr re::Sampler::SamplerDesc k_wrapMinMagLinearMipPoint = re::Sampler::SamplerDesc
				{
					.m_filterMode = re::Sampler::FilterMode::MIN_MAG_LINEAR_MIP_POINT,
					.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
					.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
					.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
					.m_mipLODBias = 0.f,
					.m_maxAnisotropy = 16,
					.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
					.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
					.m_minLOD = 0,
					.m_maxLOD = std::numeric_limits<float>::max() // No limit
				};
				const std::string wrapMinMagLinearMipPointName = "WrapMinMagLinearMipPoint";
				Sampler::m_samplerLibrary->emplace(wrapMinMagLinearMipPointName,
					re::Sampler::Create(wrapMinMagLinearMipPointName, k_wrapMinMagLinearMipPoint));

				constexpr re::Sampler::SamplerDesc k_clampMinMagLinearMipPoint = re::Sampler::SamplerDesc
				{
					.m_filterMode = re::Sampler::FilterMode::MIN_MAG_LINEAR_MIP_POINT,
					.m_edgeModeU = re::Sampler::EdgeMode::Clamp,
					.m_edgeModeV = re::Sampler::EdgeMode::Clamp,
					.m_edgeModeW = re::Sampler::EdgeMode::Clamp,
					.m_mipLODBias = 0.f,
					.m_maxAnisotropy = 16,
					.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
					.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
					.m_minLOD = 0,
					.m_maxLOD = std::numeric_limits<float>::max() // No limit
				};
				const std::string clampMinMagLinearMipPointName = "ClampMinMagLinearMipPoint";
				Sampler::m_samplerLibrary->emplace(clampMinMagLinearMipPointName,
					re::Sampler::Create(clampMinMagLinearMipPointName, k_clampMinMagLinearMipPoint));

				constexpr re::Sampler::SamplerDesc k_clampMinMagMipPoint = re::Sampler::SamplerDesc
				{
					.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_POINT,
					.m_edgeModeU = re::Sampler::EdgeMode::Clamp,
					.m_edgeModeV = re::Sampler::EdgeMode::Clamp,
					.m_edgeModeW = re::Sampler::EdgeMode::Clamp,
					.m_mipLODBias = 0.f,
					.m_maxAnisotropy = 16,
					.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
					.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
					.m_minLOD = 0,
					.m_maxLOD = std::numeric_limits<float>::max() // No limit
				};
				const std::string clampMinMagMipPointName = "ClampMinMagMipPoint";
				Sampler::m_samplerLibrary->emplace(clampMinMagMipPointName,
					re::Sampler::Create(clampMinMagMipPointName, k_clampMinMagMipPoint));

				constexpr re::Sampler::SamplerDesc k_clampLinearMipMapLinearLinear = re::Sampler::SamplerDesc
				{
					.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_LINEAR,
					.m_edgeModeU = re::Sampler::EdgeMode::Clamp,
					.m_edgeModeV = re::Sampler::EdgeMode::Clamp,
					.m_edgeModeW = re::Sampler::EdgeMode::Clamp,
					.m_mipLODBias = 0.f,
					.m_maxAnisotropy = 16,
					.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
					.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
					.m_minLOD = 0,
					.m_maxLOD = std::numeric_limits<float>::max() // No limit
				};
				const std::string clampMinMagMipLinearName = "ClampMinMagMipLinear";
				Sampler::m_samplerLibrary->emplace(clampMinMagMipLinearName,
					re::Sampler::Create(clampMinMagMipLinearName, k_clampLinearMipMapLinearLinear));

				constexpr re::Sampler::SamplerDesc k_wrapLinearMipMapLinearLinear = re::Sampler::SamplerDesc
				{
					.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_LINEAR,
					.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
					.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
					.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
					.m_mipLODBias = 0.f,
					.m_maxAnisotropy = 16,
					.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
					.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
					.m_minLOD = 0,
					.m_maxLOD = std::numeric_limits<float>::max() // No limit
				};
				const std::string wrapMinMagMipLinearName = "WrapMinMagMipLinear";
				Sampler::m_samplerLibrary->emplace(wrapMinMagMipLinearName,
					re::Sampler::Create(wrapMinMagMipLinearName, k_wrapLinearMipMapLinearLinear));


				// PCF Samplers
				constexpr float k_maxLinearDepth = std::numeric_limits<float>::max();

				constexpr re::Sampler::SamplerDesc k_borderCmpMinMagLinearMipPoint = re::Sampler::SamplerDesc
				{
					.m_filterMode = re::Sampler::FilterMode::COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
					.m_edgeModeU = re::Sampler::EdgeMode::Border,
					.m_edgeModeV = re::Sampler::EdgeMode::Border,
					.m_edgeModeW = re::Sampler::EdgeMode::Border,
					.m_mipLODBias = 0.f,
					.m_maxAnisotropy = 16,
					.m_comparisonFunc = re::Sampler::ComparisonFunc::Less,
					.m_borderColor = re::Sampler::BorderColor::OpaqueWhite,
					.m_minLOD = 0,
					.m_maxLOD = std::numeric_limits<float>::max() // No limit
				};
				const std::string borderCmpMinMagLinearMipPointName = "BorderCmpMinMagLinearMipPoint";
				Sampler::m_samplerLibrary->emplace(borderCmpMinMagLinearMipPointName,
					re::Sampler::Create(borderCmpMinMagLinearMipPointName, k_borderCmpMinMagLinearMipPoint));

				constexpr re::Sampler::SamplerDesc k_wrapCmpMinMagLinearMipPoint = re::Sampler::SamplerDesc
				{
					.m_filterMode = re::Sampler::FilterMode::COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
					.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
					.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
					.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
					.m_mipLODBias = 0.f,
					.m_maxAnisotropy = 16,
					.m_comparisonFunc = re::Sampler::ComparisonFunc::Less,
					.m_borderColor = re::Sampler::BorderColor::OpaqueWhite,
					.m_minLOD = 0,
					.m_maxLOD = std::numeric_limits<float>::max() // No limit
				};
				const std::string wrapCmpMinMagLinearMipPointName = "WrapCmpMinMagLinearMipPoint";
				Sampler::m_samplerLibrary->emplace(wrapCmpMinMagLinearMipPointName,
					re::Sampler::Create(wrapCmpMinMagLinearMipPointName, k_wrapCmpMinMagLinearMipPoint));
			}

			SEAssert(Sampler::m_samplerLibrary->contains(samplerName), "Invalid sampler name");
			auto const& result = Sampler::m_samplerLibrary->find(samplerName);

			return result->second;
		}
	}


	void Sampler::DestroySamplerLibrary()
	{
		{
			std::unique_lock<std::recursive_mutex> samplerLock(m_samplerLibraryMutex);
			m_samplerLibrary = nullptr;
		}
	}


	// -----------------------------------------------------------------------------------------------------------------


	std::shared_ptr<re::Sampler> Sampler::Create(std::string const& name, SamplerDesc const& samplerDesc)
	{
		std::shared_ptr<re::Sampler> newSampler = nullptr;
		{
			std::unique_lock<std::recursive_mutex> samplerLock(m_samplerLibraryMutex);

			if (!Sampler::m_samplerLibrary->contains(name))
			{
				newSampler.reset(new re::Sampler(name, samplerDesc));
				Sampler::m_samplerLibrary->emplace(name, newSampler);

				LOG("Creating sampler \"%s\"", name.c_str());

				// Register new Samplers with the RenderManager, so their API-level objects are created before use
				re::RenderManager::Get()->RegisterForCreate(newSampler);
			}
			else
			{
				newSampler = Sampler::m_samplerLibrary->at(name);

				SEAssert(newSampler->GetSamplerDesc() == samplerDesc,
					"Requested sampler does not match the description of the existing sampler with the same name");
			}
		}

		return newSampler;
	}


	Sampler::Sampler(std::string const& name, SamplerDesc const& samplerDesc)
		: en::NamedObject(name)
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