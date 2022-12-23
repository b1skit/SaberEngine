#pragma once

#include "Math.h"
#include "Mesh.h"
#include "NamedObject.h"
#include "ParameterBlock.h"
#include "ShadowMap.h"
#include "Transform.h"
#include "Updateable.h"


namespace gr
{
	class Camera;
	class MeshPrimitive;
}

namespace re
{
	class Shader;
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

		void Destroy();

		void Update(const double stepTimeMs) override;

		// Getters/Setters:
		inline glm::vec3& GetColor() { return m_colorIntensity; }
		inline glm::vec3 const& GetColor() const { return m_colorIntensity; }
	 
		inline LightType const& Type() const { return m_type; }
														 
		inline gr::Transform* GetTransform() { return m_ownerTransform; }	// Directional lights shine forward (Z+)

		inline gr::ShadowMap* GetShadowMap() const { return m_shadowMap.get(); }

	private:
		gr::Transform* m_ownerTransform;

		glm::vec3 m_colorIntensity;

		LightType m_type;

		std::unique_ptr<gr::ShadowMap> m_shadowMap;

	private:
		Light() = delete;
	};
}