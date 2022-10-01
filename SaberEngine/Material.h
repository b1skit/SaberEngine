#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Sampler.h"
#include "ParameterBlock.h"


namespace re
{
	class PermanentParameterBlock;
}

namespace gr
{
	class Texture;
	class Shader;
}

namespace gr
{
	class Material
	{
	public:
		// Material definitions:
		struct TextureSlotDesc
		{
			std::shared_ptr<gr::Texture> m_texture = nullptr;
			std::shared_ptr<gr::Sampler const> m_samplerObject = nullptr; // eg. Sampler object from the sampler library
			std::string m_shaderSamplerName;
		};

		struct MaterialDefinition
		{
			std::string m_definitionName = "uninitializeddMaterialDefinition";
			std::vector<TextureSlotDesc> m_textureSlots; // Vector index == shader binding index
			std::shared_ptr<gr::Shader> m_shader = nullptr;
			
		};
		static std::shared_ptr<MaterialDefinition const> GetMaterialDefinition(std::string const& matName);

		struct PBRMetallicRoughnessParams
		{
			// Currently, these are hard-coded to match the GLTF material params
			// TODO: Support generic material params

			glm::vec4 g_baseColorFactor;

			float g_metallicFactor;
			float g_roughnessFactor;
			float g_normalScale;
			float g_occlusionStrength;

			glm::vec3 g_emissiveFactor;
			float padding0;

			// Non-GLTF properties:
			glm::vec3 g_f0; // For non-metals only
			float padding1;

			//float g_isDoubleSided;
		};
		// NOTE: OpenGL std430 rules requires padding on N/2N/4N float strides when buffering UBOs/SSBOs

	private:
		static std::unique_ptr<std::unordered_map<std::string, std::shared_ptr<Material::MaterialDefinition>>> m_materialLibrary;

	public:
		Material(std::string const& name, std::shared_ptr<MaterialDefinition const> matDefinition);
		~Material() { Destroy(); }
		
		void Destroy();

		Material() = delete;

		Material(Material const&) = default;
		Material(Material&&) = default;
		Material& operator=(Material const&) = default;


		// Getters/Setters:
		inline std::string const& Name() { return m_name; }
		
		inline std::shared_ptr<gr::Shader>& GetShader()	{ return m_shader; }
		inline std::shared_ptr<gr::Shader> const& GetShader() const { return m_shader; }

		inline std::shared_ptr<re::PermanentParameterBlock>& GetMatParams() { return m_matParams; }
		inline std::shared_ptr<re::PermanentParameterBlock const> const GetMatParams() const { return m_matParams; }

		inline std::shared_ptr<gr::Texture>& GetTexture(uint32_t slotIndex) { return m_texSlots[slotIndex].m_texture; }
		inline std::shared_ptr<gr::Texture> const GetTexture(uint32_t slotIndex) const { return m_texSlots[slotIndex].m_texture; }

		std::shared_ptr<gr::Texture>& GetTexture(std::string const& samplerName);
		std::shared_ptr<gr::Texture> const& GetTexture(std::string const& samplerName) const;

		inline size_t const& NumTextureSlots() { return m_texSlots.size(); }

		void BindToShader(std::shared_ptr<gr::Shader const> shaderOverride);

	private:
		std::string const m_name;	// Must be unique: Identifies this material
		std::vector<TextureSlotDesc> m_texSlots;
		std::unordered_map<std::string, uint32_t> m_namesToSlotIndex;
		std::shared_ptr<gr::Shader> m_shader;
		std::shared_ptr<re::PermanentParameterBlock> m_matParams;
	};
}


