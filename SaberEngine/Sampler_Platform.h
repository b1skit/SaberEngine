#pragma once

#include <stdint.h>


namespace re
{
	class Sampler;
}

namespace platform
{
	class Sampler
	{
	public:
		static void CreatePlatformParams(re::Sampler& sampler);


		static void (*Create)(re::Sampler&);
		static void (*Bind)(re::Sampler&, uint32_t textureUnit, bool doBind);
		static void (*Destroy)(re::Sampler&);

	private:

	};
}