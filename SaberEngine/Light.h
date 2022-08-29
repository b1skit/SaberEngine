#pragma once

#include <string>
#include <memory>

#include <glm/glm.hpp>

#include "SceneObject.h"
#include "Mesh.h"
#include "Shader.h"


namespace gr
{
	class Camera;
}

namespace SaberEngine
{
	// Pre-declarations:
	class Mesh;
	class ShadowMap;


	enum LIGHT_TYPE
	{
		LIGHT_AMBIENT_COLOR,
		LIGHT_AMBIENT_IBL,
		LIGHT_DIRECTIONAL,
		LIGHT_POINT,
		LIGHT_SPOT,
		LIGHT_AREA,
		LIGHT_TUBE,

		LIGHT_TYPE_COUNT // Resereved: The number of light types
	};


	class Light : public virtual SceneObject
	{
	public:
		Light() = delete;
		Light(std::string const& lightName, 
			LIGHT_TYPE lightType, 
			glm::vec3 color, 
			std::shared_ptr<ShadowMap> shadowMap = nullptr, 
			float radius = 1.0f);

		~Light() { Destroy(); }

		void Destroy();

		// SaberObject interface:
		void Update() override;

		// EventListener interface:
		void HandleEvent(std::shared_ptr<EventInfo const> eventInfo) override;

		// Getters/Setters:
		inline glm::vec3 const& Color() const { return m_color; }
		inline void SetColor(glm::vec3 color) { m_color = color; }
														 
		inline LIGHT_TYPE const& Type() const { return m_type; }
														 
		inline Transform& GetTransform() { return m_transform; }	// Directional lights shine forward (Z+)
														 
		inline std::string const& Name() const { return m_lightName; }

		std::shared_ptr<ShadowMap>& GetShadowMap() { return m_shadowMap; }
		std::shared_ptr<ShadowMap> const& GetShadowMap() const { return m_shadowMap; }

		inline std::shared_ptr<gr::Mesh>& DeferredMesh() { return m_deferredMesh; }
		inline std::shared_ptr<gr::Mesh> const& DeferredMesh() const { return m_deferredMesh; }		

		inline std::shared_ptr<gr::Shader>& GetDeferredLightShader() { return m_deferredLightShader; }
		inline std::shared_ptr<gr::Shader>const& GetDeferredLightShader() const { return m_deferredLightShader; }


	private:
		glm::vec3 m_color = glm::vec3(0.0f, 0.0f, 0.0f); // Note: Intensity is factored into these values
		LIGHT_TYPE m_type = LIGHT_DIRECTIONAL; // Default

		std::string m_lightName = "unnamed_directional_light";

		std::shared_ptr<ShadowMap> m_shadowMap = nullptr;

		// Deferred light setup:
		std::shared_ptr<gr::Mesh> m_deferredMesh = nullptr;
		std::shared_ptr<gr::Shader> m_deferredLightShader = nullptr;

		// TODO: Move initialization to ctor initialization list
	};
}