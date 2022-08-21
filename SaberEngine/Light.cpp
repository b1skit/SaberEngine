#include "Light.h"
#include "CoreEngine.h"
#include "Camera.h"
#include "BuildConfiguration.h"
#include "ShadowMap.h"
#include "Shader.h"
#include "Material.h"
#include "Mesh.h"


namespace SaberEngine
{
	SaberEngine::Light::Light(string lightName, LIGHT_TYPE lightType, vec3 color, ShadowMap* shadowMap /*= nullptr*/, float radius /*= 1.0f*/)
	{
		m_lightName		= lightName;
		m_type			= lightType;
		m_color			= color;

		m_shadowMap		= shadowMap;

		// Set up deferred light mesh:
		string shaderName;
		switch (lightType)
		{
		case LIGHT_AMBIENT_COLOR:
		case LIGHT_AMBIENT_IBL:
		{
			vector<string> shaderKeywords;
			if (lightType == LIGHT_AMBIENT_COLOR)
			{
				shaderKeywords.push_back("AMBIENT_COLOR");
			}
			else // LIGHT_AMBIENT_IBL
			{
				shaderKeywords.push_back("AMBIENT_IBL");
			}

			std::shared_ptr<Shader> ambientLightShader = Shader::CreateShader(
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredAmbientLightShaderName"), 
				&shaderKeywords);

			// Attach a deferred Material:
			m_deferredMaterial = new Material
			(
				lightName + "_deferredMaterial",
				ambientLightShader,
				(TEXTURE_TYPE)0, // No textures
				true
			);

			// Attach a screen aligned quad:
			m_deferredMesh = gr::meshfactory::CreateQuad	// Align along near plane
			(
				vec3(-1.0f, 1.0f,	-1.0f),	// TL
				vec3(1.0f,	1.0f,	-1.0f),	// TR
				vec3(-1.0f, -1.0f,	-1.0f),	// BL
				vec3(1.0f,	-1.0f,	-1.0f)	// BR
			);

			break;
		}			

		case LIGHT_DIRECTIONAL:
		{
			// Attach a deferred Material:
			m_deferredMaterial = new Material
			(
				lightName + "_deferredMaterial",
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredKeylightShaderName"),
				(TEXTURE_TYPE)0, // No textures
				true
			);

			// Attach a screen aligned quad:
			m_deferredMesh = gr::meshfactory::CreateQuad	// Align along near plane
			(
				vec3(-1.0f,	1.0f,	-1.0f),	// TL
				vec3(1.0f,	1.0f,	-1.0f),	// TR
				vec3(-1.0f,	-1.0f,	-1.0f),	// BL
				vec3(1.0f,	-1.0f,	-1.0f)	// BR
			);
			break;
		}

		case LIGHT_POINT:
			m_deferredMaterial = new Material
			(
				lightName + "_deferredMaterial",
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredPointLightShaderName"),
				(TEXTURE_TYPE)0, // No textures
				true
			);

			m_deferredMesh = gr::meshfactory::CreateSphere
			(
				radius
			);
			m_deferredMesh->GetTransform().Parent(&m_transform);

			break;

		case LIGHT_SPOT:
		case LIGHT_AREA:
		case LIGHT_TUBE:
		default:
			// TODO: Implement light meshes for additional light types
			break;
		}
	}


	void Light::Destroy()
	{
		if (m_shadowMap != nullptr)
		{
			delete m_shadowMap;
			m_shadowMap = nullptr;
		}

		if (m_deferredMesh != nullptr)
		{
			m_deferredMesh = nullptr;
		}

		if (m_deferredMaterial != nullptr)
		{
			delete m_deferredMaterial;
			m_deferredMaterial = nullptr;
		}

		m_lightName += "_DELETED";
	}


	void Light::Update()
	{
	}


	void Light::HandleEvent(EventInfo const * eventInfo)
	{
	}


	ShadowMap*& Light::ActiveShadowMap(ShadowMap* newShadowMap /*= nullptr*/)
	{
		// No-arg: Gets the current shadow map
		if (newShadowMap == nullptr)
		{
			return m_shadowMap;
		}

		if (m_shadowMap != nullptr)
		{
			LOG("Deleting an existing shadow map");
			delete m_shadowMap;
			m_shadowMap = nullptr;
		}

		m_shadowMap = newShadowMap;

		return m_shadowMap;
	}
}

