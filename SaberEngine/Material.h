// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "NamedObject.h"
#include "ParameterBlock.h"
#include "Sampler.h"


namespace re
{
	class ParameterBlock;
	class Texture;
	class Shader;
}

namespace gr
{
	class Material final : public virtual en::NamedObject
	{
	public:
		// Material definitions:
		struct TextureSlotDesc
		{
			std::shared_ptr<re::Texture> m_texture = nullptr;
			std::shared_ptr<re::Sampler> m_samplerObject = nullptr; // eg. Sampler object from the sampler library
			std::string m_shaderSamplerName;
		};

		struct MaterialDefinition
		{
			std::string m_definitionName = "uninitializeddMaterialDefinition";
			std::vector<TextureSlotDesc> m_textureSlots; // Vector index == shader binding index
			std::shared_ptr<re::Shader> m_shader = nullptr;
		};
		static std::shared_ptr<MaterialDefinition const> GetMaterialDefinition(std::string const& matName);


		struct PBRMetallicRoughnessParams
		{
			// GLTF PBR material			
			glm::vec4 g_baseColorFactor { 1.f, 1.f, 1.f, 1.f };

			float g_metallicFactor = 1.f;
			float g_roughnessFactor = 1.f;
			float g_normalScale = 1.f;
			float g_occlusionStrength = 1.f;

			float g_emissiveStrength = 1.f; // KHR_materials_emissive_strength: Multiplies g_emissiveFactor
			glm::vec3 padding0;

			glm::vec3 g_emissiveFactor{ 0.f, 0.f, 0.f};
			float padding1;

			// Non-GLTF properties:
			glm::vec3 g_f0{ 0.f, 0.f, 0.f }; // For non-metals only
			float padding2;

			//float g_isDoubleSided;
		};
		// NOTE: OpenGL std430 rules requires padding on N/2N/4N float strides when buffering UBOs/SSBOs

	private:
		static std::unique_ptr<std::unordered_map<std::string, std::shared_ptr<Material::MaterialDefinition>>> m_materialLibrary;

	public:
		Material(std::string const& name, std::shared_ptr<MaterialDefinition const> matDefinition);
		~Material() { Destroy(); }
		
		void Destroy();

		Material(Material const&) = default;
		Material(Material&&) = default;
		Material& operator=(Material const&) = default;

		// Getters/Setters:	
		inline std::shared_ptr<re::Shader>& GetShader()	{ return m_shader; }
		inline std::shared_ptr<re::Shader> const& GetShader() const { return m_shader; }

		void SetParameterBlock(PBRMetallicRoughnessParams const& params);
		inline std::shared_ptr<re::ParameterBlock> const GetParameterBlock() const { return m_matParams; }

		inline std::shared_ptr<re::Texture>& GetTexture(uint32_t slotIndex) { return m_texSlots[slotIndex].m_texture; }
		inline std::shared_ptr<re::Texture> const GetTexture(uint32_t slotIndex) const { return m_texSlots[slotIndex].m_texture; }

		std::shared_ptr<re::Texture>& GetTexture(std::string const& samplerName);
		std::shared_ptr<re::Texture> const& GetTexture(std::string const& samplerName) const;
		std::vector<TextureSlotDesc> const& GetTexureSlotDescs() { return m_texSlots; }


	private:
		std::vector<TextureSlotDesc> m_texSlots;
		std::unordered_map<std::string, uint32_t> m_namesToSlotIndex;
		std::shared_ptr<re::Shader> m_shader;
		std::shared_ptr<re::ParameterBlock> m_matParams;

	private:
		Material() = delete;
	};
}


