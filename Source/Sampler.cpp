// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "DebugConfiguration.h"
#include "RenderManager.h"
#include "Sampler.h"
#include "Sampler_Platform.h"

using std::string;
using std::shared_ptr;
using std::make_shared;
using std::make_unique;
using std::unordered_map;


namespace re
{
	const std::vector<std::string> Sampler::SamplerTypeLibraryNames
	{
		ENUM_TO_STR(Wrap_Linear_Linear),
		ENUM_TO_STR(Clamp_Linear_Linear),
		ENUM_TO_STR(Clamp_Nearest_Nearest),
		ENUM_TO_STR(Clamp_LinearMipMapLinear_Linear),
		ENUM_TO_STR(Wrap_LinearMipMapLinear_Linear),
	};
	// Note: Sampler names must be unique

	// Static members:
	std::unique_ptr<std::unordered_map<Sampler::WrapAndFilterMode, std::shared_ptr<re::Sampler>>> Sampler::m_samplerLibrary = 
		nullptr;
	std::mutex Sampler::m_samplerLibraryMutex;


	std::shared_ptr<re::Sampler> const Sampler::GetSampler(std::string const& samplerTypeLibraryName)
	{
		static const unordered_map<std::string, Sampler::WrapAndFilterMode> k_nameToSamplerLibraryIdx = {
			{ENUM_TO_STR(Wrap_Linear_Linear),				WrapAndFilterMode::Wrap_Linear_Linear},
			{ENUM_TO_STR(Clamp_Linear_Linear),			WrapAndFilterMode::Clamp_Linear_Linear},
			{ENUM_TO_STR(Clamp_Nearest_Nearest),			WrapAndFilterMode::Clamp_Nearest_Nearest},
			{ENUM_TO_STR(Clamp_LinearMipMapLinear_Linear),WrapAndFilterMode::Clamp_LinearMipMapLinear_Linear},
			{ENUM_TO_STR(Wrap_LinearMipMapLinear_Linear), WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear},
		};
		SEAssert("Array size mismatch", re::Sampler::SamplerTypeLibraryNames.size() == k_nameToSamplerLibraryIdx.size());

		return GetSampler(k_nameToSamplerLibraryIdx.at(samplerTypeLibraryName));
	}


	std::shared_ptr<re::Sampler> const Sampler::GetSampler(Sampler::WrapAndFilterMode type)
	{
		std::unique_lock<std::mutex> samplerLock(m_samplerLibraryMutex);
		// TODO: Rewrite this to be lockless once m_samplerLibrary has been created

		if (Sampler::m_samplerLibrary == nullptr)
		{
			SEAssert("Size of sampler type enum and sampler type library names mismatch",
				SamplerTypeLibraryNames.size() == (size_t)Sampler::WrapAndFilterMode::WrapAndFilterMode_Count);

			Sampler::m_samplerLibrary = make_unique<unordered_map<Sampler::WrapAndFilterMode, shared_ptr<Sampler>>>();
			
			// WrapWrapLinear: Reading/writing to the GBuffer
			const Sampler::SamplerParams WrapLinearLinearParams = {
				.m_addressMode = Sampler::AddressMode::Wrap,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::Linear,
				.m_texMagMode = Sampler::MagFilter::Linear,
				.m_mipLODBias = 0.f,
				.m_maxAnisotropy = 16
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::Wrap_Linear_Linear,
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::Wrap_Linear_Linear],
					WrapLinearLinearParams));

			// Clamp_Linear_Linear: Depth maps
			const Sampler::SamplerParams ClampLinearLinearParams = {
				.m_addressMode = Sampler::AddressMode::Clamp,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::Linear,
				.m_texMagMode = Sampler::MagFilter::Linear,
				.m_mipLODBias = 0.f,
				.m_maxAnisotropy = 16
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::Clamp_Linear_Linear,
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::Clamp_Linear_Linear],
					ClampLinearLinearParams));

			// Clamp_Nearest_Nearest: BRDF pre-integration map
			const Sampler::SamplerParams ClampNearestNearestParams = {
				.m_addressMode = Sampler::AddressMode::Clamp,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::Nearest,
				.m_texMagMode = Sampler::MagFilter::Nearest,
				.m_mipLODBias = 0.f,
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::Clamp_Nearest_Nearest, 
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::Clamp_Nearest_Nearest],
					ClampNearestNearestParams));

			// Clamp, LinearMipMapLinear, Linear: HDR input images for IBL
			const Sampler::SamplerParams ClampLinearMipMapLinearLinearParams = {
				.m_addressMode = Sampler::AddressMode::Clamp,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::LinearMipMapLinear,
				.m_texMagMode = Sampler::MagFilter::Linear,
				.m_mipLODBias = 0.f,
				.m_maxAnisotropy = 16
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::Clamp_LinearMipMapLinear_Linear,
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::Clamp_LinearMipMapLinear_Linear],
					ClampLinearMipMapLinearLinearParams));

			// Wrap, LinearMipMapLinear, Linear: Skybox/IBL cubemaps
			const Sampler::SamplerParams WrapLinearMipMapLinearLinearParams = {
				.m_addressMode = Sampler::AddressMode::Wrap,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::LinearMipMapLinear,
				.m_texMagMode = Sampler::MagFilter::Linear,
				.m_mipLODBias = 0.f,
				.m_maxAnisotropy = 16
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear,  
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear],
					WrapLinearMipMapLinearLinearParams));
		}

		auto const& result = Sampler::m_samplerLibrary->find(type);

		SEAssert("Invalid sampler name", result != Sampler::m_samplerLibrary->end());

		return result->second;
	}


	void Sampler::DestroySamplerLibrary()
	{
		std::unique_lock<std::mutex> samplerLock(m_samplerLibraryMutex);
		m_samplerLibrary = nullptr;
	}


	std::shared_ptr<re::Sampler> Sampler::Create(std::string const& name, SamplerParams params)
	{
		// TODO: Get rid of the sampler library, and create samplers on demand as they're requested
		// -> Nested unordered maps of AddressMode/MinFilter/MagFilter

		std::shared_ptr<re::Sampler> newSampler = nullptr;
		newSampler.reset(new re::Sampler(name, params));

		// Register new Samplers with the RenderManager, so their API-level objects are created before use
		re::RenderManager::Get()->RegisterForCreate(newSampler);

		return newSampler;
	}


	Sampler::Sampler(string const& name, SamplerParams params) : en::NamedObject(name),
		m_samplerParams{params}
	{
		platform::Sampler::CreatePlatformParams(*this);
	}


	void Sampler::Destroy()
	{
		SEAssert("Sampler has not been created", m_platformParams->m_isCreated);
		platform::Sampler::Destroy(*this);
		m_platformParams = nullptr;
		m_samplerParams = {};
	}
}