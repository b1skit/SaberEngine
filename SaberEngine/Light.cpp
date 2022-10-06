#include "Light.h"
#include "CoreEngine.h"
#include "Camera.h"
#include "DebugConfiguration.h"
#include "ShadowMap.h"
#include "Shader.h"
#include "Mesh.h"

using gr::Shader;
using gr::Transform;
using std::shared_ptr;
using std::make_shared;
using std::string;
using glm::vec3;


namespace gr
{
	Light::Light(string const& name, Transform* ownerTransform, LightType lightType, vec3 colorIntensity, bool hasShadow) :
		en::NamedObject(name),
			m_ownerTransform(ownerTransform),
			m_colorIntensity(colorIntensity),
			m_type(lightType),
			m_shadowMap(nullptr),
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

			if (hasShadow)
			{
				gr::Bounds sceneWorldBounds =
					en::CoreEngine::GetSceneManager()->GetSceneData()->GetWorldSpaceSceneBounds();

				const gr::Bounds transformedBounds = sceneWorldBounds.GetTransformedBounds(
					glm::inverse(m_ownerTransform->GetWorldMatrix()));

				gr::Camera::CameraConfig shadowCamConfig;
				shadowCamConfig.m_near = -transformedBounds.zMax();
				shadowCamConfig.m_far = -transformedBounds.zMin();
				shadowCamConfig.m_isOrthographic = true;
				shadowCamConfig.m_orthoLeft = transformedBounds.xMin();
				shadowCamConfig.m_orthoRight = transformedBounds.xMax();
				shadowCamConfig.m_orthoBottom = transformedBounds.yMin();
				shadowCamConfig.m_orthoTop = transformedBounds.yMax();

				const uint32_t shadowMapRes = 
					en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowMapRes");
				m_shadowMap = make_shared<ShadowMap>(
					m_name,
					shadowMapRes,
					shadowMapRes,
					shadowCamConfig,
					m_ownerTransform);
			}
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

			if (hasShadow)
			{
				gr::Camera::CameraConfig shadowCamConfig;
				shadowCamConfig.m_fieldOfView		= 90.0f;
				shadowCamConfig.m_near				= 1.0f;
				shadowCamConfig.m_aspectRatio		= 1.0f;
				shadowCamConfig.m_isOrthographic	= false;
				shadowCamConfig.m_far				= m_radius;
			
				const uint32_t cubeMapRes = 
					en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowCubeMapRes");
				m_shadowMap = make_shared<ShadowMap>(
					m_name,
					cubeMapRes,
					cubeMapRes,
					shadowCamConfig,
					m_ownerTransform,
					vec3(0.0f, 0.0f, 0.0f),	// No offset
					true); // useCubeMap

				m_shadowMap->MinShadowBias() =
					en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultMinShadowBias");
				m_shadowMap->MaxShadowBias() =
					en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultMaxShadowBias");
			}
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


	void Light::Update()
	{
		if (m_type == LightType::Directional) // Update shadow cam bounds
		{
			gr::Bounds sceneWorldBounds =
				en::CoreEngine::GetSceneManager()->GetSceneData()->GetWorldSpaceSceneBounds();

			const gr::Bounds transformedBounds = sceneWorldBounds.GetTransformedBounds(
				glm::inverse(m_ownerTransform->GetWorldMatrix()));

			gr::Camera::CameraConfig shadowCamConfig;
			shadowCamConfig.m_near				= -transformedBounds.zMax();
			shadowCamConfig.m_far				= -transformedBounds.zMin();
			shadowCamConfig.m_isOrthographic	= true;
			shadowCamConfig.m_orthoLeft			= transformedBounds.xMin();
			shadowCamConfig.m_orthoRight		= transformedBounds.xMax();
			shadowCamConfig.m_orthoBottom		= transformedBounds.yMin();
			shadowCamConfig.m_orthoTop			= transformedBounds.yMax();

			m_shadowMap->ShadowCamera()->SetCameraConfig(shadowCamConfig);
		}
	}
}

