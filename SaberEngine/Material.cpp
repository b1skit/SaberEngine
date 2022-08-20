#include "Material.h"
#include "CoreEngine.h"
#include "Shader.h"
#include "Texture.h"
#include "BuildConfiguration.h"
#include "Shader.h"


#include <string>
using std::to_string;


namespace SaberEngine
{
	// Static members:
	const string Material::TEXTURE_SAMPLER_NAMES[TEXTURE_COUNT] =
	{
		"albedo",			// TEXTURE_ALBEDO
		"normal",			// TEXTURE_NORMAL
		"RMAO",				// TEXTURE_EMISSIVE
		"emissive",			// TEXTURE_RMAO
	};


	const string Material::RENDER_TEXTURE_SAMPLER_NAMES[RENDER_TEXTURE_COUNT] = 
	{
		"GBuffer_Albedo",		// RENDER_TEXTURE_ALBEDO
		"GBuffer_WorldNormal",	// RENDER_TEXTURE_WORLD_NORMAL
		"GBuffer_RMAO",			// RENDER_TEXTURE_RMAO
		"GBuffer_Emissive",		// RENDER_TEXTURE_EMISSIVE

		"GBuffer_WorldPos",		// RENDER_TEXTURE_WORLD_POSITION
		"GBuffer_MatProp0",		// RENDER_TEXTURE_MATERIAL_PROPERTY_0

		"GBuffer_Depth",		// RENDER_TEXTURE_DEPTH
	};


	const string Material::DEPTH_TEXTURE_SAMPLER_NAMES[DEPTH_TEXTURE_COUNT] = 
	{
		"shadowDepth",			// DEPTH_TEXTURE_SHADOW
	};


	const string Material::CUBE_MAP_TEXTURE_SAMPLER_NAMES[CUBE_MAP_COUNT] = 
	{
		"CubeMap_0",				// CUBE_MAP_RIGHT
		"CubeMap_1",				// CUBE_MAP_RIGHT
	};


	const string Material::MATERIAL_PROPERTY_NAMES[MATERIAL_PROPERTY_COUNT] =
	{
		"matProperty0",
		//"matProperty1",
		//"matProperty2",
		//"matProperty3",
		//"matProperty4",
		//"matProperty5",
		//"matProperty6",
		//"matProperty7"
	};


	Material::Material(string materialName, string shaderName, TEXTURE_TYPE textureCount /*= TEXTURE_COUNT*/, bool isRenderMaterial /*= false*/)
	{
		m_name				= materialName;

		m_shader			= Shader::CreateShader(shaderName, &m_shaderKeywords);

		m_textures = std::vector<std::shared_ptr<gr::Texture>>(textureCount, nullptr);

		Init();	// Initialize textures and properties arrays
	}


	Material::Material(string materialName, Shader* shader, TEXTURE_TYPE textureCount /*= TEXTURE_COUNT*/, bool isRenderMaterial /*= false*/)
	{
		m_name				= materialName;

		m_shader			= shader;

		m_textures = std::vector<std::shared_ptr<gr::Texture>>(textureCount, nullptr);

		Init();	// Initialize textures and properties arrays
	}


	void Material::Destroy()
	{
		if (m_shader != nullptr)
		{
			m_shader->Destroy();
			delete m_shader;
		}
	}


	void Material::Init()
	{
		for (size_t i = 0; i < m_textures.size(); i++)
		{
			if (m_textures[i] != nullptr)
			{
				m_textures[i] = nullptr;
			}			
		}

		for (int i = 0; i < MATERIAL_PROPERTY_COUNT; i++)
		{
			m_properties[i] = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}


	std::shared_ptr<gr::Texture>& Material::AccessTexture(TEXTURE_TYPE textureType)
	{
		return m_textures[textureType];
	}


	void Material::AddShaderKeyword(string const& newKeyword)
	{
		m_shaderKeywords.emplace_back(newKeyword);
	}


	void Material::BindAllTextures(int startingTextureUnit, bool doBind)
	{
		for (uint32_t i = 0; i < m_textures.size(); i++)
		{
			if (m_textures[i] != nullptr)
			{
				m_textures[i]->Bind(startingTextureUnit + i, doBind);
			}
		}
	}


	void Material::AttachCubeMapTextures(std::shared_ptr<gr::Texture> cubemapTexture)
	{
		assert("Cannot attach nullptr cubeMapFaces" && cubemapTexture != nullptr);

		for (size_t i = 0; i < m_textures.size(); i++)
		{
			if (m_textures[i] != nullptr)
			{
				m_textures[i] = nullptr;
			}
		}

		m_textures = std::vector<std::shared_ptr<gr::Texture>>(RENDER_TEXTURE_COUNT, nullptr);
		m_textures[0] = cubemapTexture;
	}
}

