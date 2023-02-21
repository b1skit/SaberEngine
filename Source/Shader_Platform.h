// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class Shader;
}

namespace platform
{
	class Shader
	{
	public:
		static void CreatePlatformParams(re::Shader& shader);
		
	public: // Api-specific functionality
		static void (*Destroy)(re::Shader&);
	};
}	

