#pragma once

#include <string>
#include <memory>

#include <glm/glm.hpp>

#include "NamedObject.h"
#include "Updateable.h"
#include "Transform.h"
#include "ParameterBlock.h"


namespace gr
{
	class Camera;
	class Mesh;
	class Shader;
	class ShadowMap;
}

namespace gr
{
	class Light : public virtual en::NamedObject, public virtual en::Updateable
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
		struct LightParams
		{
			glm::vec3 g_colorIntensity = glm::vec3(0.f, 0.f, 0.f);
			const float padding0 = 0.f;

			// Directional lights: Normalized, world-space dir pointing towards source (ie. parallel)
			glm::vec3 g_worldPos = glm::vec3(0.f, 0.f, 0.f); 
			const float padding1 = 0.f;
		};

	public:
		Light(std::string const& name,
			gr::Transform* ownerTransform,
			LightType lightType, 
			glm::vec3 colorIntensity,
			bool hasShadow
		);

		~Light() { Destroy(); }

		Light(Light const&) = default;
		Light(Light&&) = default;
		Light& operator=(Light const&) = default;

		Light() = delete;

		void Destroy();

		void Update() override;

		// Getters/Setters:
		inline glm::vec3& GetColor() { return m_lightParams.g_colorIntensity; }
		inline glm::vec3 const& GetColor() const { return m_lightParams.g_colorIntensity; }
	 
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

		inline std::shared_ptr<re::ParameterBlock const>& GetParameterBlock() { return m_lightParameterBlock; }
		inline std::shared_ptr<re::ParameterBlock const> const GetParameterBlock() const { return m_lightParameterBlock; }

	private:
		gr::Transform* m_ownerTransform;

		LightParams m_lightParams;
		std::shared_ptr<re::ParameterBlock const> m_lightParameterBlock;

		LightType m_type;

		std::shared_ptr<gr::ShadowMap> m_shadowMap;

		// Deferred light setup:
		std::shared_ptr<gr::Mesh> m_deferredMesh; // TODO: This should be a RenderMesh
		std::shared_ptr<gr::Shader> m_deferredLightShader;
	};
}