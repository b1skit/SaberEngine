// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"


namespace dx12
{
	class Shader
	{
	public:
		struct PlatformParams final : public virtual re::Shader::PlatformParams
		{
		};

		// TODO: Handle copying of d3dcompiler_47.dll into the same folder as the compiled Saber Engine .exe 

	public:
		static void Create(re::Shader& shader);
		static void Destroy(re::Shader& shader);
	};
}