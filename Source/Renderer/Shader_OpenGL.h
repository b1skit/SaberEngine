// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer_OpenGL.h"
#include "Shader.h" 


namespace re
{
	class RootConstants;
	class Shader;
	class Texture;
	class TextureTargetSet;

	struct RWTextureInput;
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

			Float,
			Vec2f,
			Vec3f,
			Vec4f,
			
			Int,
			Int2,
			Int3,
			Int4,

			UInt,
			UInt2,
			UInt3,
			UInt4,

			/*Texture,
			Sampler*/
		};


	public:
		struct PlatObj final : public re::Shader::PlatObj
		{
			std::array<std::string, re::Shader::ShaderType_Count> m_shaderTexts;

			uint32_t m_shaderReference = 0;

			std::unordered_map<util::HashKey, GLint> m_samplerUnits;
			std::unordered_map<util::HashKey, GLint> m_vertexAttributeLocations;

			struct BufferMetadata final
			{
				opengl::Buffer::BindTarget m_bindTarget;
				
				std::vector<GLint> m_bufferLocations; // Indexed by shader array index
			};
			std::unordered_map<util::HashKey, BufferMetadata> m_bufferMetadata;

			void AddBufferMetadata(
				char const* name, opengl::Buffer::BindTarget, GLint bufferLocation);
		};


	public:
		static void Create(re::Shader&);
		static void Destroy(re::Shader&);

		// OpenGL-specific functions:
		static void Bind(re::Shader const&);

		static void SetRootConstants(re::Shader const&, re::RootConstants const&);

		static void SetUniform(
			re::Shader const&,
			std::string const& uniformName, 
			void const* value, 
			opengl::Shader::UniformType const, 
			int count);

		static void SetTextureAndSampler(re::Shader const&, re::TextureAndSamplerInput const&);
		
		static void SetImageTextureTargets(re::Shader const&, std::vector<re::RWTextureInput> const&);

		static void SetBuffer(re::Shader const&, re::BufferInput const&);
	};
}