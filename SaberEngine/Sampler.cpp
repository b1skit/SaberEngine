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
	// Elements must correspond with enum class WrapAndFilterMode in Sampler.h


	std::unique_ptr<std::unordered_map<Sampler::WrapAndFilterMode, std::shared_ptr<gr::Sampler>>> Sampler::m_samplerLibrary = 
		nullptr;

	std::shared_ptr<gr::Sampler> const Sampler::GetSampler(Sampler::WrapAndFilterMode type)
	{
		if (Sampler::m_samplerLibrary == nullptr)
		{
			SEAssert("Size of sampler type enum and sampler type library names mismatch",
				SamplerTypeLibraryNames.size() == (size_t)Sampler::WrapAndFilterMode::SamplerType_Count);

			Sampler::m_samplerLibrary = make_unique<unordered_map<Sampler::WrapAndFilterMode, shared_ptr<Sampler>>>();
			
			// WrapWrapLinear: Reading/writing to the GBuffer
			const Sampler::SamplerParams WrapLinearLinearParams = {
				Sampler::Mode::Wrap,
				Sampler::MinFilter::Linear,
				Sampler::MaxFilter::Linear
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::WrapLinearLinear,
				make_shared<gr::Sampler>(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::WrapLinearLinear],
					WrapLinearLinearParams));

			// ClampLinearLinear: Depth maps
			const Sampler::SamplerParams ClampLinearLinearParams = {
				Sampler::Mode::Clamp ,
				Sampler::MinFilter::Linear,
				Sampler::MaxFilter::Linear
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::ClampLinearLinear,
				make_shared<gr::Sampler>(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::ClampLinearLinear],
					ClampLinearLinearParams));

			// ClampNearestNearest: BRDF pre-integration map
			const Sampler::SamplerParams ClampNearestNearestParams = {
				Sampler::Mode::Clamp,
				Sampler::MinFilter::Nearest,
				Sampler::MaxFilter::Nearest
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::ClampNearestNearest, 
				make_shared<gr::Sampler>(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::ClampNearestNearest],
					ClampNearestNearestParams));

			// Clamp, LinearMipMapLinear, Linear: HDR input images for IBL
			const Sampler::SamplerParams ClampLinearMipMapLinearLinearParams = {
				Sampler::Mode::Clamp ,
				Sampler::MinFilter::LinearMipMapLinear,
				Sampler::MaxFilter::Linear
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear,
				make_shared<gr::Sampler>(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear],
					ClampLinearMipMapLinearLinearParams));

			// Wrap, LinearMipMapLinear, Linear: Skybox/IBL cubemaps
			const Sampler::SamplerParams WrapLinearMipMapLinearLinearParams = {
				Sampler::Mode::Wrap,
				Sampler::MinFilter::LinearMipMapLinear,
				Sampler::MaxFilter::Linear
			};
			Sampler::m_samplerLibrary->emplace(Sampler::WrapAndFilterMode::WrapLinearMipMapLinearLinear,  
				make_shared<gr::Sampler>(
					SamplerTypeLibraryNames[(size_t)Sampler::WrapAndFilterMode::WrapLinearMipMapLinearLinear],
					WrapLinearMipMapLinearLinearParams));
		}

		auto const& result = Sampler::m_samplerLibrary->find(type);

		SEAssert("Invalid sampler name", result != Sampler::m_samplerLibrary->end());

		return result->second;
	}


	Sampler::Sampler(string const& name, SamplerParams params) : en::NamedObject(name),
		m_samplerParams{params}
	{
		platform::Sampler::CreatePlatformParams(*this);
	}


	void Sampler::Destroy()
	{
		m_platformParams = nullptr;
		m_samplerParams = {};
	}
}