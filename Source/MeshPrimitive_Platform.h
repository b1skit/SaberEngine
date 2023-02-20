// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "MeshPrimitive.h"


namespace platform
{
	class MeshPrimitive
	{
	public:
		static void CreatePlatformParams(re::MeshPrimitive& meshPrimitive);


		static void (*Destroy)(re::MeshPrimitive& meshPrimitive);
	};
}
