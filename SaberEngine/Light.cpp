#include "Light.h"
#include "CoreEngine.h"
#include "Camera.h"
#include "DebugConfiguration.h"
#include "ShadowMap.h"
#include "Shader.h"
#include "Mesh.h"
using gr::Shader;
using std::shared_ptr;
using std::make_shared;


namespace SaberEngine
{
	SaberEngine::Light::Light(
		std::string const& lightName, 
		LIGHT_TYPE lightType, 
		vec3 color, 
		std::shared_ptr<ShadowMap> shadowMap /*= nullptr*/, 
		float radius /*= 1.0f*/)
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
			m_deferredLightShader = make_shared<Shader>(
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredAmbientLightShaderName"));
			
			if (lightType == LIGHT_AMBIENT_COLOR)
			{
				m_deferredLightShader->ShaderKeywords().emplace_back("AMBIENT_COLOR");
			}
			else // LIGHT_AMBIENT_IBL
			{
				m_deferredLightShader->ShaderKeywords().emplace_back("AMBIENT_IBL");
			}

			m_deferredLightShader->Create();

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
			m_deferredLightShader = make_shared<Shader>(
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredKeylightShaderName"));
			m_deferredLightShader->Create();

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
			m_deferredLightShader = make_shared<Shader>(
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredPointLightShaderName"));
			m_deferredLightShader->Create();
			
			m_deferredMesh = gr::meshfactory::CreateSphere(radius);
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
		m_shadowMap = nullptr;
		m_deferredMesh = nullptr;
		m_deferredLightShader = nullptr;

		m_lightName += "_DELETED";
	}


	void Light::Update()
	{
	}


	void Light::HandleEvent(std::shared_ptr<EventInfo const> eventInfo)
	{
	}
}

