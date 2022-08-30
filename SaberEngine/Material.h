#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Sampler.h"


// Predeclarations:
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

		struct PropertyDesc
		{
			std::string m_propertyName;
			glm::vec4 m_property;
		};

		struct MaterialDefinition
		{
			std::string m_definitionName = "uninitializeddMaterialDefinition";
			std::vector<TextureSlotDesc> m_textureSlots; // Slot index == shader binding index
			std::vector<PropertyDesc> m_propertySlots;
			std::shared_ptr<gr::Shader> m_shader = nullptr;
			
		};

		static std::shared_ptr<MaterialDefinition const> GetMaterialDefinition(std::string const& matName);

	private:
		static std::unique_ptr<std::unordered_map<std::string, std::shared_ptr<Material::MaterialDefinition>>> m_materialLibrary;

	public:
		enum TextureSlot
		{
			// GBuffer stage sampler inputs:
			MatAlbedo	= 0,	// Contains transparency in the alpha channel
			MatNormal	= 1,
			MatRMAO		= 2,	// Packed Roughness, Metalic, AmbientOcclusion (RGB) + unused A
			MatEmissive = 3,
			Mat_Count = 4,

			// Deferred lighting GBuffer sampler inputs:
			GBufferAlbedo	= 0,
			GBufferWNormal	= 1,
			GBufferRMAO		= 2,
			GBufferEmissive = 3,
			GBufferWPos		= 4,
			GBufferMatProp0 = 5,	// MatProperty0
			GBufferDepth	= 6,	// Make this the last element
			GBuffer_Count = 7,

			// Generic texture samplers:
			Tex0 = 0,
			Tex1 = 1,
			Tex2 = 2,
			Tex3 = 3,
			Tex4 = 4,
			Tex5 = 5,
			Tex6 = 6,
			Tex7 = 7, // E.g. BRDF pre-integration map
			Tex8 = 8,
			Tex_Count = 9,

			// Depth map texture samplers:
			Depth0 = 10,
			Depth_Count = 1,

			// Cube map samplers:
			CubeMap0 = 11,
			CubeMap1 = 12,
			CubeMap_Count = 2,

		}; // Note: If new enums are added, don't forget to update Material::k_GBufferTexNames as well
		// TODO: Make this an assert^^^^


		// TODO: Materials should contain generic blocks of parameters (eg. within a struct?)
		enum MATERIAL_PROPERTY_INDEX
		{
			MatProperty0 = 0,
			MatProperty_Count = 1
		}; // Note: If new enums are added, don't forget to update Material::k_MatPropNames as well


	public:
		Material(std::string const& name, std::shared_ptr<MaterialDefinition const> matDefinition);
		~Material() { Destroy(); }
		
		void Destroy();

		Material()							= delete;

		Material(Material const&)			= default;
		Material(Material&&)				= default;
		Material& operator=(Material const&) = default;


		// Getters/Setters:
		inline std::string const& Name() { return m_name; }
		
		inline std::shared_ptr<gr::Shader>& GetShader()	{ return m_shader; }
		inline std::shared_ptr<gr::Shader> const& GetShader() const { return m_shader; }

		inline glm::vec4& Property(MATERIAL_PROPERTY_INDEX index) { return m_properties[index].m_property; }
		inline glm::vec4 const& Property(MATERIAL_PROPERTY_INDEX index) const { return m_properties[index].m_property; }

		std::shared_ptr<gr::Texture>& GetTexture(uint32_t slotIndex) { return m_texSlots[slotIndex].m_texture; }
		std::shared_ptr<gr::Texture>const& GetTexture(uint32_t slotIndex) const { return m_texSlots[slotIndex].m_texture; }

		std::shared_ptr<gr::Texture>& GetTexture(std::string const& samplerName);
		std::shared_ptr<gr::Texture>const& GetTexture(std::string const& samplerName) const;

		inline size_t const& NumTextureSlots() { return m_texSlots.size(); }

		void BindToShader(std::shared_ptr<gr::Shader const> shaderOverride);


	private:
		std::string m_name;	// Must be unique: Identifies this material
		std::vector<TextureSlotDesc> m_texSlots;
		std::unordered_map<std::string, uint32_t> m_namesToSlotIndex;
		std::vector<PropertyDesc> m_properties; // Generic material properties
		std::shared_ptr<gr::Shader> m_shader;
		
	};
}


