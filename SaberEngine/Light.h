#pragma once

#include <string>
#include <memory>

#include <glm/glm.hpp>

#include "SceneObject.h"


namespace gr
{
	class Camera;
	class Mesh;
	class Shader;
	class ShadowMap;
}

namespace gr
{
	class Light : public virtual SaberEngine::SceneObject
	{
	public:
		enum LightType
		{
			AmbientColor,
			AmbientIBL,
			Directional,
			Point,
			Spot,
			Area,
			Tube,

			Light_Count
		};

	public:
		Light(std::string const& lightName, 
			LightType lightType, 
			glm::vec3 color, 
			std::shared_ptr<gr::ShadowMap> shadowMap = nullptr,
			float radius = 1.0f);

		~Light() { Destroy(); }

		Light(Light const&) = default;
		Light(Light&&) = default;
		Light& operator=(Light const&) = default;

		Light() = delete;

		void Destroy();

		// SaberObject interface:
		void Update() override { /* Do nothing */ };

		// EventListener interface:
		void HandleEvent(std::shared_ptr<SaberEngine::EventInfo const> eventInfo) override { /* Do nothing */ };

		// Getters/Setters:
		inline glm::vec3& GetColor() { return m_color; }
		inline glm::vec3 const& GetColor() const { return m_color; }
														 
		inline LightType const& Type() const { return m_type; }
														 
		inline gr::Transform& GetTransform() { return m_transform; }	// Directional lights shine forward (Z+)
		inline gr::Transform const& GetTransform() const { return m_transform; }
														 
		inline std::string const& Name() const { return m_lightName; }

		inline std::shared_ptr<gr::ShadowMap>& GetShadowMap() { return m_shadowMap; }
		inline std::shared_ptr<gr::ShadowMap> const& GetShadowMap() const { return m_shadowMap; }

		inline std::shared_ptr<gr::Mesh>& DeferredMesh() { return m_deferredMesh; }
		inline std::shared_ptr<gr::Mesh> const& DeferredMesh() const { return m_deferredMesh; }		

		inline std::shared_ptr<gr::Shader>& GetDeferredLightShader() { return m_deferredLightShader; }
		inline std::shared_ptr<gr::Shader>const& GetDeferredLightShader() const { return m_deferredLightShader; }


	private:
		glm::vec3 m_color; // Note: Intensity is factored into this value
		LightType m_type;

		std::string m_lightName;

		std::shared_ptr<gr::ShadowMap> m_shadowMap;

		// Deferred light setup:
		std::shared_ptr<gr::Mesh> m_deferredMesh;
		std::shared_ptr<gr::Shader> m_deferredLightShader;
	};
}