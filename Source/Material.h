// � 2022 Adam Badke. All rights reserved.
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
		static void DestroyMaterialLibrary();

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

			static constexpr char const* const s_shaderName = "PBRMetallicRoughnessParams"; // Not counted towards size of struct
		};
		// NOTE: OpenGL std430 rules requires padding on N/2N/4N float strides when buffering UBOs/SSBOs

	private:
		static std::unique_ptr<std::unordered_map<std::string, std::shared_ptr<Material::MaterialDefinition>>> m_materialLibrary;
		static std::mutex m_matLibraryMutex;

	public:
		Material(std::string const& name, std::shared_ptr<MaterialDefinition const> matDefinition);
		~Material() { Destroy(); }
		
		void Destroy();

		Material(Material const&) = default;
		Material(Material&&) = default;
		Material& operator=(Material const&) = default;

		// Getters/Setters:	
		void SetShader(std::shared_ptr<re::Shader> shader);
		re::Shader* GetShader() const;

		void SetParameterBlock(PBRMetallicRoughnessParams const& params);
		std::shared_ptr<re::ParameterBlock> const GetParameterBlock() const;

		void SetTexture(uint32_t slotIndex, std::shared_ptr<re::Texture>);
		std::shared_ptr<re::Texture> const GetTexture(uint32_t slotIndex) const;
		std::shared_ptr<re::Texture> const GetTexture(std::string const& samplerName) const;
		std::vector<TextureSlotDesc> const& GetTexureSlotDescs() const;

		void ShowImGuiWindow();


	private:
		std::vector<TextureSlotDesc> m_texSlots;
		std::unordered_map<std::string, uint32_t> m_namesToSlotIndex;
		std::shared_ptr<re::Shader> m_shader;
		std::shared_ptr<re::ParameterBlock> m_matParams;


	private:
		Material() = delete;
	};


	inline void Material::SetShader(std::shared_ptr<re::Shader> shader)
	{
		m_shader = shader;
	}


	inline re::Shader* Material::GetShader() const
	{
		return m_shader.get();
	}


	inline std::shared_ptr<re::ParameterBlock> const Material::GetParameterBlock() const
	{
		return m_matParams;
	}


	inline void Material::SetTexture(uint32_t slotIndex, std::shared_ptr<re::Texture> texture)
	{
		SEAssert("Out of bounds slot index", slotIndex < m_texSlots.size());
		m_texSlots[slotIndex].m_texture = texture;
	}


	inline std::shared_ptr<re::Texture> const Material::GetTexture(uint32_t slotIndex) const
	{
		return m_texSlots[slotIndex].m_texture;
	}


	inline std::vector<Material::TextureSlotDesc> const& Material::GetTexureSlotDescs() const
	{
		return m_texSlots;
	}
}


