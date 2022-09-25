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
using std::string;
using glm::vec3;


namespace gr
{
	Light::Light(
		std::string const& lightName, 
		gr::Transform* ownerTransform,
		LightType lightType, 
		vec3 colorIntensity,
		std::shared_ptr<gr::ShadowMap> shadowMap /*= nullptr*/,
		float radius /*= 1.0f*/) : 
			m_name(lightName),
			m_ownerTransform(ownerTransform),
			m_colorIntensity(colorIntensity),
			m_type(lightType),
			m_shadowMap(shadowMap),
			m_deferredMesh(nullptr),
			m_deferredLightShader(nullptr),
			m_radius(1.0f)
	{
		// Set up deferred light mesh:
		string shaderName;
		switch (lightType)
		{
		case AmbientIBL:
		{
			m_deferredLightShader = make_shared<Shader>(
				en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredAmbientLightShaderName"));
			
			m_deferredLightShader->ShaderKeywords().emplace_back("AMBIENT_IBL");

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
				en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredKeylightShaderName"));
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
			// Compute the radius: 
			const float cutoff = 0.05f; // Want the sphere mesh radius where light intensity will be close to zero
			const float maxColor = glm::max(glm::max(m_colorIntensity.r, m_colorIntensity.g), m_colorIntensity.b);
			m_radius = glm::sqrt((maxColor / cutoff) - 1.0f);

			m_deferredLightShader = make_shared<Shader>(
				en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredPointLightShaderName"));
			m_deferredLightShader->Create();

			// Create the sphere with a radius of 1, and scale it to allow us to instance deferred lights with a single
			// mesh and multiple MVP matrices
			m_deferredMesh = gr::meshfactory::CreateSphere(1.0f);

			m_deferredMesh->GetTransform().SetParent(m_ownerTransform);
			
			// Currently, we scale the deferred mesh directly. Ideally, a Mesh object wouldn't have a transform (it
			// should be owned by a RenderMesh object and implicitely use its transform)
			m_deferredMesh->GetTransform().SetModelScale(vec3(m_radius, m_radius, m_radius));

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
	}
}

