// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class ParameterBlock;
}


namespace platform
{
	class ParameterBlock
	{
	public:
		static void CreatePlatformParams(re::ParameterBlock& paramBlock);

	
		static void (*Create)(re::ParameterBlock&);
		static void (*Update)(re::ParameterBlock const&);
		static void (*Destroy)(re::ParameterBlock&);
	};
}