#include "Material.h"
#include "CoreEngine.h"
#include "Shader.h"
#include "Texture.h"
#include "BuildConfiguration.h"
#include "Shader.h"
#include "CoreEngine.h"

using std::string;
using std::shared_ptr;
using std::vector;
using SaberEngine::Shader;
using gr::Texture;
using SaberEngine::CoreEngine;


namespace gr
{
	// Static members:
	const vector<string> Material::k_MatTexNames
	{
		"MatAlbedo",
		"MatNormal",
		"MatRMAO",
		"MatEmissive",
	};

	const vector<string> Material::k_GBufferTexNames
	{
		"GBufferAlbedo",
		"GBufferWNormal",
		"GBufferRMAO",	
		"GBufferEmissive",
		"GBufferWPos",	
		"GBufferMatProp0",
		"GBufferDepth",	
	};

	const vector<string> Material::k_GenericTexNames
	{
		"Tex0",
		"Tex1",
		"Tex2",
		"Tex3",
		"Tex4",
		"Tex5",
		"Tex6",
		"Tex7",
		"Tex8",
	};

	const vector<string> Material::k_DepthTexNames
	{
		"Depth0",
	};


	const vector<string> Material::k_CubeMapTexNames
	{
		"CubeMap0",
		"CubeMap1",
	};

	const vector<string> Material::k_MatPropNames
	{
		"MatProperty0",
	};


	Material::Material(string const& materialName, string const& shaderName, TextureSlot const& textureCount /*= Mat_Count*/) :
		Material(materialName, Shader::CreateShader(shaderName, &m_shaderKeywords),	textureCount) {}


	Material::Material(string const& materialName, shared_ptr<Shader> const& shader, TextureSlot const& textureCount /*= Mat_Count*/) :
		m_name{materialName},
		m_shader{shader},
		m_textures(textureCount, nullptr),
		m_properties(MatProperty_Count, vec4(0.0f, 0.0f, 0.0f, 0.0f))
	{
		//32
		/*const uint32_t maxTextures = CoreEngine::GetRenderManager()->GetContext().GetMaxTextureInputs();*/
	}


	void Material::Destroy()
	{
		m_name += "_DESTROYED";
		m_shader = nullptr;
		m_textures.clear();
		m_properties.clear();
		m_shaderKeywords.clear();
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
}

