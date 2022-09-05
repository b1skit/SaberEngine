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
using glm::vec3;


namespace gr
{
	Light::Light(
		std::string const& lightName, 
		LightType lightType, 
		vec3 color, 
		std::shared_ptr<gr::ShadowMap> shadowMap /*= nullptr*/,
		float radius /*= 1.0f*/) :
			m_color(color),
			m_type(lightType),
			m_lightName(lightName),
			m_shadowMap(shadowMap),
			m_deferredMesh(nullptr),
			m_deferredLightShader(nullptr)
	{
		// Set up deferred light mesh:
		string shaderName;
		switch (lightType)
		{
		case AmbientColor:
		case AmbientIBL:
		{
			m_deferredLightShader = make_shared<Shader>(
				SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredAmbientLightShaderName"));
			
			if (lightType == AmbientColor)
			{
				m_deferredLightShader->ShaderKeywords().emplace_back("AMBIENT_COLOR");
			}
			else // AmbientIBL
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
		}
		break;
		case Directional:
		{
			m_deferredLightShader = make_shared<Shader>(
				SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredKeylightShaderName"));
			m_deferredLightShader->Create();

			// Attach a screen aligned quad:
			m_deferredMesh = gr::meshfactory::CreateQuad	// Align along near plane
			(
				vec3(-1.0f,	1.0f,	-1.0f),	// TL
				vec3(1.0f,	1.0f,	-1.0f),	// TR
				vec3(-1.0f,	-1.0f,	-1.0f),	// BL
				vec3(1.0f,	-1.0f,	-1.0f)	// BR
			);
		}
		break;
		case Point:
		{
			m_deferredLightShader = make_shared<Shader>(
				SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredPointLightShaderName"));
			m_deferredLightShader->Create();

			// Create the sphere with a radius of 1, and scale it to allow us to instance deferred lights with a single
			// mesh and multiple MVP matrices
			m_deferredMesh = gr::meshfactory::CreateSphere(1.0f);

			m_deferredMesh->GetTransform().SetParent(&m_transform);
			
			// TODO: BUG HERE! If we call m_transform.SetWorldScale(vec3(radius, radius, radius)); here, the scale
			// values get stomped later during the call to SceneManager::InitializeTransformValues()
			// -> For now, set the scale on the deferred mesh, which is technically what we expect anyway...
			// -> But eventually, we should set the scale on the Light m_transform, which will scale the child deferred
			// mesh
			m_deferredMesh->GetTransform().SetWorldScale(vec3(radius, radius, radius));

		}
		break;
		case Spot:
		case Area:
		case Tube:
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
}

