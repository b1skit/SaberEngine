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
		static void CreatePlatformObject(re::Shader& shader);
		
	public: // Api-specific functionality
		static void (*Create)(re::Shader&);
		static void (*Destroy)(re::Shader&);
	};
}	

