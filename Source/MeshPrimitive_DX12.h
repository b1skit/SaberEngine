// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "MeshPrimitive.h"


namespace dx12
{
	class MeshPrimitive
	{
	public:
		struct PlatformParams final : public virtual re::MeshPrimitive::PlatformParams
		{
			PlatformParams(re::MeshPrimitive& meshPrimitive);
		};


	public:
		static void Create(re::MeshPrimitive&);
		static void Destroy(re::MeshPrimitive&);
	};
}