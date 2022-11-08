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
using re::ParameterBlock;


namespace gr
{
	Light::Light(string const& name, Transform* ownerTransform, LightType lightType, vec3 colorIntensity, bool hasShadow) :
		en::NamedObject(name),
			m_ownerTransform(ownerTransform),
			m_type(lightType),
			m_shadowMap(nullptr),
			m_deferredMesh(nullptr)
	{		
		m_colorIntensity = colorIntensity;

		// Set up deferred light mesh:
		string shaderName;
		switch (lightType)
		{
		case AmbientIBL:
		{
		}
		break;
		case Directional:
		{
			if (hasShadow)
			{
				gr::Bounds sceneWorldBounds =
					en::CoreEngine::GetSceneManager()->GetSceneData()->GetWorldSpaceSceneBounds();

				const gr::Bounds transformedBounds = sceneWorldBounds.GetTransformedBounds(
					glm::inverse(m_ownerTransform->GetWorldMatrix()));

				gr::Camera::CameraConfig shadowCamConfig;
				shadowCamConfig.m_near				= -transformedBounds.zMax();
				shadowCamConfig.m_far				= -transformedBounds.zMin();
				shadowCamConfig.m_projectionType	= Camera::CameraConfig::ProjectionType::Orthographic;
				shadowCamConfig.m_orthoLeft			= transformedBounds.xMin();
				shadowCamConfig.m_orthoRight		= transformedBounds.xMax();
				shadowCamConfig.m_orthoBottom		= transformedBounds.yMin();
				shadowCamConfig.m_orthoTop			= transformedBounds.yMax();

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
			const float radius = glm::sqrt((maxColor / cutoff) - 1.0f);

			// Create the sphere with a radius of 1, and scale it to allow us to instance deferred lights with a single
			// mesh and multiple MVP matrices
			m_deferredMesh = make_shared<gr::RenderMesh>(m_ownerTransform, gr::meshfactory::CreateSphere(1.0f));
			
			// TODO: Currently, we scale the deferred mesh directly. Ideally, lights should not have
			// a mesh; one should be created by the GS and assigned as a batch
			m_deferredMesh->GetTransform().SetModelScale(vec3(radius, radius, radius));

			if (hasShadow)
			{
				gr::Camera::CameraConfig shadowCamConfig;
				shadowCamConfig.m_fieldOfView		= 90.0f;
				shadowCamConfig.m_near				= 0.1f;
				shadowCamConfig.m_far				= radius;
				shadowCamConfig.m_aspectRatio		= 1.0f;
				shadowCamConfig.m_projectionType	= Camera::CameraConfig::ProjectionType::Perspective;
			
				const uint32_t cubeMapRes = 
					en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<uint32_t>("defaultShadowCubeMapRes");

				m_shadowMap = make_shared<ShadowMap>(
					m_name,
					cubeMapRes,
					cubeMapRes,
					shadowCamConfig,
					m_ownerTransform,
					vec3(0.0f, 0.0f, 0.0f),	// shadowCamPosition: No offset
					true);					// useCubeMap

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
			shadowCamConfig.m_projectionType	= Camera::CameraConfig::ProjectionType::Orthographic;
			shadowCamConfig.m_orthoLeft			= transformedBounds.xMin();
			shadowCamConfig.m_orthoRight		= transformedBounds.xMax();
			shadowCamConfig.m_orthoBottom		= transformedBounds.yMin();
			shadowCamConfig.m_orthoTop			= transformedBounds.yMax();

			m_shadowMap->ShadowCamera()->SetCameraConfig(shadowCamConfig);
		}
	}
}

