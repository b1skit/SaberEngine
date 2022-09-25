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
	class Light
	{
	public:
		enum LightType
		{
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
			gr::Transform* ownerTransform,
			LightType lightType, 
			glm::vec3 colorIntensity, 
			std::shared_ptr<gr::ShadowMap> shadowMap = nullptr,
			float radius = 1.0f
		);

		~Light() { Destroy(); }

		Light(Light const&) = default;
		Light(Light&&) = default;
		Light& operator=(Light const&) = default;

		Light() = delete;

		void Destroy();

		// Getters/Setters:
		inline std::string const& GetName() const { return m_name; }

		inline glm::vec3& GetColor() { return m_colorIntensity; }
		inline glm::vec3 const& GetColor() const { return m_colorIntensity; }

		inline float GetRadius() const { return m_radius; }
														 
		inline LightType const& Type() const { return m_type; }
														 
		inline gr::Transform* GetTransform() { return m_ownerTransform; }	// Directional lights shine forward (Z+)
		inline gr::Transform const* const GetTransform() const { return m_ownerTransform; }

		inline std::shared_ptr<gr::ShadowMap>& GetShadowMap() { return m_shadowMap; }
		inline std::shared_ptr<gr::ShadowMap> const& GetShadowMap() const { return m_shadowMap; }

		inline std::shared_ptr<gr::Mesh>& DeferredMesh() { return m_deferredMesh; }
		inline std::shared_ptr<gr::Mesh> const& DeferredMesh() const { return m_deferredMesh; }		

		// TODO: Delete these accessors, and load/assign these shaders within the deferred lighting GS
		inline std::shared_ptr<gr::Shader>& GetDeferredLightShader() { return m_deferredLightShader; }
		inline std::shared_ptr<gr::Shader>const& GetDeferredLightShader() const { return m_deferredLightShader; }


	private:
		std::string const m_name;
		gr::Transform* m_ownerTransform;

		glm::vec3 m_colorIntensity;
		LightType m_type;
		float m_radius; // For point lights

		std::shared_ptr<gr::ShadowMap> m_shadowMap;

		// Deferred light setup:
		std::shared_ptr<gr::Mesh> m_deferredMesh; // TODO: This should be a RenderMesh
		std::shared_ptr<gr::Shader> m_deferredLightShader;
	};
}