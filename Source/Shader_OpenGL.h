// � 2022 Adam Badke. All rights reserved.
#pragma once

#include "Shader.h"
#include "Shader_Platform.h"


namespace re
{
	class ParameterBlock;
	class Shader;
	class Texture;
	class Sampler;
}

namespace opengl
{
	class Shader
	{
	public:
		enum ShaderType : uint8_t
		{
			Vertex,
			TesselationControl,
			TesselationEvaluation,
			Geometry,
			Fragment,
			Compute,

			ShaderType_Count
		};


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
			std::array<std::string, ShaderType_Count> m_shaderTexts;

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
			void * value, 
			opengl::Shader::UniformType const type, 
			int const count);
		static void SetTextureAndSampler(
			re::Shader const&, 
			std::string const& uniformName, 
			std::shared_ptr<re::Texture>, 
			std::shared_ptr<re::Sampler>, 
			uint32_t subresource);
		static void SetParameterBlock(re::Shader const&, re::ParameterBlock const&);
	};
}