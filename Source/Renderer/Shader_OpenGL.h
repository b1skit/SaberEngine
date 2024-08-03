// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Shader.h"
#include "Shader_Platform.h"


namespace re
{
	class Buffer;
	class Shader;
	class Texture;
	class TextureTargetSet;

	struct TextureAndSamplerInput;
}

namespace opengl
{
	class Shader
	{
	public:
		enum UniformType : uint8_t
		{
			Matrix4x4f,
			Matrix3x3f,
			Vec3f,
			Vec4f,
			Float,
			Int,
			Texture,
			Sampler
		};


	public:
		struct PlatformParams final : public re::Shader::PlatformParams
		{
			std::array<std::string, re::Shader::ShaderType_Count> m_shaderTexts;

			uint32_t m_shaderReference = 0;

			std::unordered_map<std::string, int32_t> m_samplerUnits;
		};


	public:
		static void Create(re::Shader& shader);
		static void Destroy(re::Shader& shader);

		// OpenGL-specific functions:
		static void Bind(re::Shader const& shader);

		static void SetUniform(
			re::Shader const& shader,
			std::string const& uniformName, 
			void const* value, 
			opengl::Shader::UniformType const type, 
			int const count);

		static void SetTextureAndSampler(re::Shader const&, re::TextureAndSamplerInput const&);
		
		static void SetImageTextureTargets(re::Shader const&, re::TextureTargetSet const&);

		static void SetBuffer(re::Shader const&, re::Buffer const&);
	};
}