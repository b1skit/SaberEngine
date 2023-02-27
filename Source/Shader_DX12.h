// � 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "Shader.h"


namespace dx12
{
	class Shader
	{
	public:
		enum ShaderType
		{
			Vertex,
			Geometry,
			Pixel,
			Compute,

			ShaderType_Count
		};

		// Arbitrary: Limits the number of indexes we search for semantics (POSITION, NORMAL[n], COLOR[n], etc)
		static const uint8_t k_maxVShaderVertexInputs = 32;


	public:
		struct PlatformParams final : public virtual re::Shader::PlatformParams
		{
			std::array< Microsoft::WRL::ComPtr<ID3DBlob>, ShaderType_Count> m_shaderBlobs = {0};
		};

		// TODO: Handle copying of d3dcompiler_47.dll into the same folder as the compiled Saber Engine .exe 

	public:
		static void Create(re::Shader& shader);
		static void Destroy(re::Shader& shader);
	};
}