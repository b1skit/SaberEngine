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
		ENUM_TO_STR(WrapLinearLinear),
		ENUM_TO_STR(ClampLinearLinear),
		ENUM_TO_STR(ClampNearestNearest),
		ENUM_TO_STR(ClampLinearMipMapLinearLinear),
		ENUM_TO_STR(WrapLinearMipMapLinearLinear),
	};
	// Note: Sampler names must be unique

	// Static members:
	std::unique_ptr<std::unordered_map<Sampler::WrapAndFilterMode, std::shared_ptr<re::Sampler>>> Sampler::m_samplerLibrary = 
		nullptr;
	std::mutex Sampler::m_samplerLibraryMutex;


	std::shared_ptr<re::Sampler> const Sampler::GetSampler(std::string const& samplerTypeLibraryname)
	{
		static const unordered_map<std::string, Sampler::WrapAndFilterMode> k_nameToSamplerLibraryIdx = {
			{ENUM_TO_STR(WrapLinearLinear),				WrapAndFilterMode::WrapLinearLinear},
			{ENUM_TO_STR(ClampLinearLinear),			WrapAndFilterMode::ClampLinearLinear},
			{ENUM_TO_STR(ClampNearestNearest),			WrapAndFilterMode::ClampNearestNearest},
			{ENUM_TO_STR(ClampLinearMipMapLinearLinear),WrapAndFilterMode::ClampLinearMipMapLinearLinear},
			{ENUM_TO_STR(WrapLinearMipMapLinearLinear), WrapAndFilterMode::WrapLinearMipMapLinearLinear},
		};
		SEAssert("Array size mismatch", re::Sampler::SamplerTypeLibraryNames.size() == k_nameToSamplerLibraryIdx.size());

		return GetSampler(k_nameToSamplerLibraryIdx.at(samplerTypeLibraryname));
	}

	std::shared_ptr<re::Sampler> const Sampler::GetSampler(Sampler::WrapAndFilterMode type)
	{
		std::unique_lock<std::mutex> samplerLock(m_samplerLibraryMutex);
		// TODO: Rewrite this to be lockless once m_samplerLibrary has been created

		if (Sampler::m_samplerLibrary == nullptr)
		{
			SEAssert("Size of sampler type enum and sampler type library names mismatch",
				SamplerTypeLibraryNames.size() == (size_t)Sampler::WrapAndFilterMode::SamplerType_Count);

			Sampler::m_samplerLibrary = make_unique<unordered_map<Sampler::WrapAndFilterMode, shared_ptr<Sampler>>>();
			
			// WrapWrapLinear: Reading/writing to the GBuffer
			const Sampler::SamplerParams WrapLinearLinearParams = {
				.m_addressMode = Sampler::AddressMode::Wrap,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::Linear,
				.m_texMaxMode = Sampler::MaxFilter::Linear,
				.m_mipLODBias = 0.f,
				.m_maxAnisotropy = 16
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::WrapLinearLinear,
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::WrapLinearLinear],
					WrapLinearLinearParams));

			// ClampLinearLinear: Depth maps
			const Sampler::SamplerParams ClampLinearLinearParams = {
				.m_addressMode = Sampler::AddressMode::Clamp,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::Linear,
				.m_texMaxMode = Sampler::MaxFilter::Linear,
				.m_mipLODBias = 0.f,
				.m_maxAnisotropy = 16
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::ClampLinearLinear,
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::ClampLinearLinear],
					ClampLinearLinearParams));

			// ClampNearestNearest: BRDF pre-integration map
			const Sampler::SamplerParams ClampNearestNearestParams = {
				.m_addressMode = Sampler::AddressMode::Clamp,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::Nearest,
				.m_texMaxMode = Sampler::MaxFilter::Nearest,
				.m_mipLODBias = 0.f,
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::ClampNearestNearest, 
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::ClampNearestNearest],
					ClampNearestNearestParams));

			// Clamp, LinearMipMapLinear, Linear: HDR input images for IBL
			const Sampler::SamplerParams ClampLinearMipMapLinearLinearParams = {
				.m_addressMode = Sampler::AddressMode::Clamp,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::LinearMipMapLinear,
				.m_texMaxMode = Sampler::MaxFilter::Linear,
				.m_mipLODBias = 0.f,
				.m_maxAnisotropy = 16
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear,
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear],
					ClampLinearMipMapLinearLinearParams));

			// Wrap, LinearMipMapLinear, Linear: Skybox/IBL cubemaps
			const Sampler::SamplerParams WrapLinearMipMapLinearLinearParams = {
				.m_addressMode = Sampler::AddressMode::Wrap,
				.m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f),
				.m_texMinMode = Sampler::MinFilter::LinearMipMapLinear,
				.m_texMaxMode = Sampler::MaxFilter::Linear,
				.m_mipLODBias = 0.f,
				.m_maxAnisotropy = 16
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::WrapLinearMipMapLinearLinear,  
				re::Sampler::Create(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::WrapLinearMipMapLinearLinear],
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
		// -> Nested unordered maps of AddressMode/MinFilter/MaxFilter

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
		#pragma message("TODO: Reeanble the assert in re::Sampler::Destroy")
		//SEAssert("Sampler has not been created", m_platformParams->m_isCreated);
		platform::Sampler::Destroy(*this);
		m_platformParams = nullptr;
		m_samplerParams = {};
	}
}