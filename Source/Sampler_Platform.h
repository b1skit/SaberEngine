// � 2022 Adam Badke. All rights reserved.
#pragma once

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
		static void (*Destroy)(re::Sampler&);
	};
}