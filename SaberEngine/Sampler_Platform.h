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
		static void CreatePlatformParams(gr::Sampler& sampler);


		static void (*Create)(gr::Sampler&);
		static void (*Bind)(gr::Sampler&, uint32_t textureUnit, bool doBind);
		static void (*Destroy)(gr::Sampler&);

	private:

	};
}