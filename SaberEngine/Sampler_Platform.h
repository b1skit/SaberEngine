#pragma once

#include <stdint.h>


namespace gr
{
	class Sampler;
}

namespace platform
{
	class Sampler
	{
	public:
		struct PlatformParams
		{
			// Params contain unique GPU bindings that should not be arbitrarily copied/duplicated
			PlatformParams() = default;
			PlatformParams(PlatformParams&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams&) = delete;
			PlatformParams& operator=(PlatformParams&&) = delete;

			// API-specific GPU bindings should be destroyed here
			virtual ~PlatformParams() = 0;

			static void CreatePlatformParams(gr::Sampler& sampler);
		};


		static void (*Create)(gr::Sampler&);
		static void (*Bind)(gr::Sampler const&, uint32_t textureUnit, bool doBind);
		static void (*Destroy)(gr::Sampler&);

	private:

	};


	// We need to provide a destructor implementation since it's pure virtual
	inline platform::Sampler::PlatformParams::~PlatformParams() {};
}