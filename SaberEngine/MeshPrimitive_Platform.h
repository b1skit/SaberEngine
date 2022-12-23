#pragma once

#include "MeshPrimitive.h"


namespace platform
{
	class MeshPrimitive
	{
	public:
		static void CreatePlatformParams(re::MeshPrimitive& meshPrimitive);


		static void (*Create)(re::MeshPrimitive& meshPrimitive);
		static void (*Bind)(re::MeshPrimitive& meshPrimitive, bool doBind);
		static void (*Destroy)(re::MeshPrimitive& meshPrimitive);
	};
}
