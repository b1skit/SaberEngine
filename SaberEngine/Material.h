#pragma once

#include <vector>
#include <string>
#include <memory>

#include <GL/glew.h>
#include <glm/glm.hpp>


// Predeclarations:
namespace gr
{
	class Texture;
}
namespace SaberEngine
{
	class Shader;
}


namespace gr
{
	class Material
	{
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

			// Additional generic texture samplers:
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
			Depth0 = 11,
			Depth_Count = 1,

			// Cube map samplers:
			CubeMap0 = 20,
			CubeMap1 = 26,
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
		// Shader texture target sampler names:
		const static std::vector<std::string> k_MatTexNames;
		const static std::vector<std::string> k_GBufferTexNames;
		const static std::vector<std::string> k_GenericTexNames;
		const static std::vector<std::string> k_DepthTexNames;
		const static std::vector<std::string> k_CubeMapTexNames;
		const static std::vector<std::string> k_MatPropNames;


	public:
		Material(std::string const& materialName, std::string const& shaderName, TextureSlot const& textureCount = Mat_Count);
		Material(std::string const& materialName, std::shared_ptr<SaberEngine::Shader> const& shader, TextureSlot const& textureCount = Mat_Count);
		Material() = delete;

		Material(Material const&) = default;
		Material(Material&&) = default;

		Material& operator=(Material const&) = default;

		~Material() { Destroy(); }
		void Destroy();


		// Getters/Setters:
		inline std::string const& Name() { return m_name; }
		
		inline std::shared_ptr<SaberEngine::Shader>& GetShader()	{ return m_shader; }
		inline std::shared_ptr<SaberEngine::Shader> const& GetShader() const { return m_shader; }

		inline glm::vec4& Property(MATERIAL_PROPERTY_INDEX index) { return m_properties[index]; }
		inline glm::vec4 const& Property(MATERIAL_PROPERTY_INDEX index) const { return m_properties[index]; }

		std::shared_ptr<gr::Texture>& GetTexture(TextureSlot textureType) { return m_textures[textureType]; }
		std::shared_ptr<gr::Texture>const& GetTexture(TextureSlot textureType) const { return m_textures[textureType]; }

		inline size_t const& NumTextureSlots() { return m_textures.size(); }

		std::vector<std::string> const& ShaderKeywords() const { return m_shaderKeywords; }
		void AddShaderKeyword(std::string const& newKeyword);
		

		// TODO: Materials should specify the texture unit; we shouldn't need to specify textureUnit
		// -> Deleting all Texture constructors etc ensures textures are unique w.r.t the GPU
		// Bind operation should probably go through the material???
		void BindAllTextures(int startingTextureUnit, bool doBind);


	private:
		std::string m_name;	// Must be unique: Identifies this material
		std::shared_ptr<SaberEngine::Shader> m_shader;
		std::vector<std::shared_ptr<gr::Texture>> m_textures;		
		std::vector<glm::vec4> m_properties; // Generic material properties
		std::vector<std::string> m_shaderKeywords;
	};
}


