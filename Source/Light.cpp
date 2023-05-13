// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "Light.h"
#include "MeshPrimitive.h"
#include "SceneManager.h"
#include "Shader.h"


namespace
{
	using gr::Transform;


	gr::Camera::CameraConfig ComputeDirectionalShadowCameraConfigFromSceneBounds(
		gr::Transform* lightTransform, gr::Bounds& sceneWorldBounds)
	{
		gr::Bounds const& transformedBounds = sceneWorldBounds.GetTransformedAABBBounds(
			glm::inverse(lightTransform->GetGlobalMatrix(Transform::TRS)));

		gr::Camera::CameraConfig shadowCamConfig;

		shadowCamConfig.m_near						= -transformedBounds.zMax();
		shadowCamConfig.m_far						= -transformedBounds.zMin();
		shadowCamConfig.m_projectionType			= gr::Camera::CameraConfig::ProjectionType::Orthographic;
		shadowCamConfig.m_orthoLeftRightBotTop.x	= transformedBounds.xMin();
		shadowCamConfig.m_orthoLeftRightBotTop.y	= transformedBounds.xMax();
		shadowCamConfig.m_orthoLeftRightBotTop.z	= transformedBounds.yMin();
		shadowCamConfig.m_orthoLeftRightBotTop.w	= transformedBounds.yMax();

		return shadowCamConfig;
	}
}


namespace gr
{
	using re::Shader;
	using gr::Transform;
	using en::Config;
	using en::SceneManager;
	using std::unique_ptr;
	using std::make_unique;
	using std::string;
	using glm::vec3;
	using re::ParameterBlock;


	Light::Light(string const& name, Transform* ownerTransform, LightType lightType, vec3 colorIntensity, bool hasShadow)
		: en::NamedObject(name)
		, m_ownerTransform(ownerTransform)
		, m_type(lightType)
		, m_shadowMap(nullptr)
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
				const uint32_t shadowMapRes = Config::Get()->GetValue<int>("defaultShadowMapRes");
				m_shadowMap = make_unique<ShadowMap>(
					GetName(),
					shadowMapRes,
					shadowMapRes,
					Camera::CameraConfig(),
					m_ownerTransform,
					glm::vec3(0.f, 0.f, 0.f),
					ShadowMap::ShadowType::Single);
				// Note: We'll compute the camera config from the scene bounds during the first call to Update(); so
				// here we just pass a default camera config
			}
		}
		break;
		case Point:
		{
			// Compute the radius: 
			const float cutoff = 0.05f; // Want the sphere mesh radius where light intensity will be close to zero
			const float maxColor = glm::max(glm::max(m_colorIntensity.r, m_colorIntensity.g), m_colorIntensity.b);
			const float radius = glm::sqrt((maxColor / cutoff) - 1.0f);
			
			// Scale the owning transform such that a sphere created with a radius of 1 will be the correct size
			m_ownerTransform->SetLocalScale(vec3(radius, radius, radius));

			if (hasShadow)
			{
				gr::Camera::CameraConfig shadowCamConfig;
				shadowCamConfig.m_yFOV				= static_cast<float>(std::numbers::pi) / 2.0f;
				shadowCamConfig.m_near				= 0.1f;
				shadowCamConfig.m_far				= radius;
				shadowCamConfig.m_aspectRatio		= 1.0f;
				shadowCamConfig.m_projectionType	= Camera::CameraConfig::ProjectionType::Perspective;
			
				const uint32_t cubeMapRes = Config::Get()->GetValue<int>("defaultShadowCubeMapRes");

				m_shadowMap = make_unique<ShadowMap>(
					GetName(),
					cubeMapRes,
					cubeMapRes,
					shadowCamConfig,
					m_ownerTransform,
					vec3(0.0f, 0.0f, 0.0f),	// shadowCamPosition: No offset
					ShadowMap::ShadowType::CubeMap);

				m_shadowMap->MinMaxShadowBias() = glm::vec2( 
					Config::Get()->GetValue<float>("defaultMinShadowBias"),
					Config::Get()->GetValue<float>("defaultMaxShadowBias"));
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
	}


	void Light::Update(const double stepTimeMs)
	{
		if (m_type == LightType::Directional) // Update shadow cam bounds
		{
			gr::Bounds sceneWorldBounds = SceneManager::GetSceneData()->GetWorldSpaceSceneBounds();
			Camera::CameraConfig shadowCamConfig = ComputeDirectionalShadowCameraConfigFromSceneBounds(
				m_ownerTransform, sceneWorldBounds);

			if (m_shadowMap)
			{
				m_shadowMap->ShadowCamera()->SetCameraConfig(shadowCamConfig);
			}
		}
	}
}

