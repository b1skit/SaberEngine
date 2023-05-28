// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "Sampler_Platform.h"
#include "NamedObject.h"


namespace platform
{
	bool RegisterPlatformFunctions();
}

namespace re
{
	class Sampler final : public virtual en::NamedObject
	{
	public:
		struct PlatformParams : public re::IPlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_isCreated = false;
		};

	public:
		static const std::vector<std::string> SamplerTypeLibraryNames;

		enum class WrapAndFilterMode
		{
			// EdgeMode, MinFilter, MaxFilter
			WrapLinearLinear,
			ClampLinearLinear,
			ClampNearestNearest,
			ClampLinearMipMapLinearLinear,
			WrapLinearMipMapLinearLinear,

			SamplerType_Count
		};
		static std::shared_ptr<re::Sampler> const GetSampler(WrapAndFilterMode type);
		static std::shared_ptr<re::Sampler> const GetSampler(std::string const& samplerTypeLibraryName);
		static void DestroySamplerLibrary();

	private:
		static std::unique_ptr<std::unordered_map<WrapAndFilterMode, std::shared_ptr<re::Sampler>>> m_samplerLibrary;
		static std::mutex m_samplerLibraryMutex;

	public:
		enum class AddressMode
		{
			Wrap,
			Mirror,
			MirrorOnce, // TODO: Support this in OpenGL
			Clamp,
			Border, // TODO: Support this in OpenGL

			Invalid,
			Mode_Count = Invalid
		};

		enum class MinFilter
		{
			Nearest,				// Point sample
			NearestMipMapLinear,	// Linear interpolation of point samples from 2 nearest mips
			Linear,					// Bilinear interpolation of 4 closest texels
			LinearMipMapLinear,		// Linear interpolation of bilinear interpolation results of 2 nearest mips

			Invalid,
			MinFilter_Count = Invalid
		};

		enum class MaxFilter
		{
			Nearest,	// Point sample
			Linear,		// Bilinear interpolation of 4 closest texels

			Invalid,
			MaxFilter_Count = Invalid
		};

		struct SamplerParams
		{
			AddressMode m_addressMode = AddressMode::Wrap; // TODO: Support individual U/V/W address modes

			glm::vec4 m_borderColor = glm::vec4(0.f, 0.f, 0.f, 0.f);

			// TODO: Combine min/max filters into a single enum, ala D3D
			MinFilter m_texMinMode = MinFilter::LinearMipMapLinear;
			MaxFilter m_texMaxMode = MaxFilter::Linear;

			// TODO: Support these in OpenGL:
			float m_mipLODBias = 0.f; 
			uint32_t m_maxAnisotropy = 16; // Valid values [1, 16]
		};


	public:
		static std::shared_ptr<re::Sampler> Create(std::string const& name, SamplerParams params);
		~Sampler() { Destroy(); }

		SamplerParams const& GetSamplerParams() const { return m_samplerParams; }

		void SetPlatformParams(std::unique_ptr<Sampler::PlatformParams> params) { m_platformParams = std::move(params); }
		Sampler::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }


	private:
		explicit Sampler(std::string const& name, SamplerParams params);
		void Destroy();


	private:
		SamplerParams m_samplerParams;
		std::unique_ptr<Sampler::PlatformParams> m_platformParams;


	private:
		Sampler() = delete;
		Sampler(Sampler const& rhs) = delete;
		Sampler(Sampler const&& rhs) = delete;
		Sampler& operator=(Sampler const& rhs) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Sampler::PlatformParams::~PlatformParams() {};
}