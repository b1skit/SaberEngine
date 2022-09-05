#pragma once

#include "DebugConfiguration.h"
#include "Sampler.h"
#include "Sampler_Platform.h"

using std::string;
using std::shared_ptr;
using std::make_shared;
using std::make_unique;
using std::unordered_map;


namespace gr
{
	const std::vector<std::string> Sampler::SamplerTypeLibraryNames
	{
		"WrapLinearLinear",
		"ClampLinearLinear",
		"ClampNearestNearest",
		"ClampLinearMipMapLinearLinear",
		"WrapLinearMipMapLinearLinear"
	};
	// Elements must correspond with enum class SamplerType in Sampler.h


	std::unique_ptr<std::unordered_map<Sampler::SamplerType, std::shared_ptr<gr::Sampler>>> Sampler::m_samplerLibrary = 
		nullptr;

	std::shared_ptr<gr::Sampler const> const Sampler::GetSampler(Sampler::SamplerType type)
	{
		if (Sampler::m_samplerLibrary == nullptr)
		{
			SEAssert("Size of sampler type enum and sampler type library names mismatch",
				SamplerTypeLibraryNames.size() == (size_t)Sampler::SamplerType::SamplerType_Count);

			Sampler::m_samplerLibrary = make_unique<unordered_map<Sampler::SamplerType, shared_ptr<Sampler>>>();
			
			// WrapWrapLinear: Reading/writing to the GBuffer
			Sampler::SamplerParams WrapLinearLinearParams = {
				Sampler::TextureSamplerMode::Wrap,
				Sampler::TextureMinFilter::Linear,
				Sampler::TextureMaxFilter::Linear
			};
			shared_ptr<gr::Sampler> WrapLinearLinear = make_shared<gr::Sampler>(
				SamplerTypeLibraryNames[(size_t)Sampler::SamplerType::WrapLinearLinear], 
				WrapLinearLinearParams);
			WrapLinearLinear->Create();
			Sampler::m_samplerLibrary->insert({Sampler::SamplerType::WrapLinearLinear, WrapLinearLinear});

			// ClampLinearLinear: Depth maps
			Sampler::SamplerParams ClampLinearLinearParams = {
				Sampler::TextureSamplerMode::Clamp ,
				Sampler::TextureMinFilter::Linear,
				Sampler::TextureMaxFilter::Linear
			};
			shared_ptr<gr::Sampler> ClampLinearLinear = make_shared<gr::Sampler>(
				SamplerTypeLibraryNames[(size_t)Sampler::SamplerType::ClampLinearLinear],
				ClampLinearLinearParams);
			ClampLinearLinear->Create();
			Sampler::m_samplerLibrary->insert({ Sampler::SamplerType::ClampLinearLinear, ClampLinearLinear });

			// ClampNearestNearest: BRDF pre-integration map
			Sampler::SamplerParams ClampNearestNearestParams = {
				Sampler::TextureSamplerMode::Clamp,
				Sampler::TextureMinFilter::Nearest,
				Sampler::TextureMaxFilter::Nearest
			};
			shared_ptr<gr::Sampler> ClampNearestNearest = make_shared<gr::Sampler>(
				SamplerTypeLibraryNames[(size_t)Sampler::SamplerType::ClampNearestNearest],
				ClampNearestNearestParams);
			ClampNearestNearest->Create();
			Sampler::m_samplerLibrary->insert({ Sampler::SamplerType::ClampNearestNearest, ClampNearestNearest });

			// Clamp, LinearMipMapLinear, Linear: HDR input images for IBL
			Sampler::SamplerParams ClampLinearMipMapLinearLinearParams = {
				Sampler::TextureSamplerMode::Clamp ,
				Sampler::TextureMinFilter::LinearMipMapLinear,
				Sampler::TextureMaxFilter::Linear
			};
			shared_ptr<gr::Sampler> ClampLinearMipMapLinearLinear = make_shared<gr::Sampler>(
				SamplerTypeLibraryNames[(size_t)Sampler::SamplerType::ClampLinearMipMapLinearLinear],
				ClampLinearMipMapLinearLinearParams);
			ClampLinearMipMapLinearLinear->Create();
			Sampler::m_samplerLibrary->insert({ Sampler::SamplerType::ClampLinearMipMapLinearLinear, ClampLinearMipMapLinearLinear });

			// Wrap, LinearMipMapLinear, Linear: Skybox/IBL cubemaps
			Sampler::SamplerParams WrapLinearMipMapLinearLinearParams = {
				Sampler::TextureSamplerMode::Wrap,
				Sampler::TextureMinFilter::LinearMipMapLinear,
				Sampler::TextureMaxFilter::Linear
			};
			shared_ptr<gr::Sampler> WrapLinearMipMapLinearLinear = make_shared<gr::Sampler>(
				SamplerTypeLibraryNames[(size_t)Sampler::SamplerType::WrapLinearMipMapLinearLinear],
				WrapLinearMipMapLinearLinearParams);
			WrapLinearMipMapLinearLinear->Create();
			Sampler::m_samplerLibrary->insert({ Sampler::SamplerType::WrapLinearMipMapLinearLinear, ClampLinearMipMapLinearLinear });
		}

		auto result = Sampler::m_samplerLibrary->find(type);

		SEAssert("Invalid sampler name", result != Sampler::m_samplerLibrary->end());

		return result->second;
	}

	Sampler::Sampler(string const& name, SamplerParams params) :
		m_samplerParams{params},
		m_name{name}
	{
		platform::Sampler::PlatformParams::CreatePlatformParams(*this);
	}


	void Sampler::Create()
	{
		platform::Sampler::Create(*this);
	}


	void Sampler::Bind(uint32_t textureUnit, bool doBind) const
	{
		platform::Sampler::Bind(*this, textureUnit, doBind);
	}


	void Sampler::Destroy()
	{
		m_name += "_DESTROYED";
		m_platformParams = nullptr;
		m_samplerParams = {};
	}
}