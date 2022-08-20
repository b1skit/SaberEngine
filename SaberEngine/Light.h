// Base light class. All other forwardLights inherit from this.
// Defaults as a directional light.

#pragma once

#include <string>
#include <memory>

#include <glm/glm.hpp>
using glm::vec3;
using glm::vec4;

#include "SceneObject.h"	// Base class
#include "grMesh.h"




namespace SaberEngine
{
	// Pre-declarations:
	class Camera;
	class Material;
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


	class Light : public SceneObject
	{
	public:
		Light() {}; // Default constructor
		Light(std::string lightName, LIGHT_TYPE lightType, vec3 color, ShadowMap* shadowMap = nullptr, float radius = 1.0f);

		void Destroy();

		// SaberObject interface:
		void Update();

		// EventListener interface:
		void HandleEvent(EventInfo const* eventInfo);

		// Getters/Setters:
		inline vec3 const&			Color() const							{ return m_color; }
		inline void					SetColor(vec3 color)					{ m_color = color; }

		inline LIGHT_TYPE const&	Type() const							{ return m_type; }

		inline Transform&			GetTransform()							{ return m_transform; }	// Directional lights shine forward (Z+)

		inline string const&		Name() const							{ return m_lightName; }
		
		ShadowMap*&					ActiveShadowMap(ShadowMap* newShadowMap = nullptr);	// Get/set the current shadow map

		inline std::shared_ptr<gr::Mesh>&	DeferredMesh() { return m_deferredMesh; }
		inline Material*&		DeferredMaterial()	{ return m_deferredMaterial; }


	protected:


	private:
		vec3 m_color					= vec3(0.0f, 0.0f, 0.0f);	// Note: Intensity is factored into these values
		LIGHT_TYPE m_type				= LIGHT_DIRECTIONAL;		// Default

		string m_lightName			= "unnamed_directional_light";

		ShadowMap* m_shadowMap		= nullptr; // Deallocated by calling Destroy() during SceneManager.Shutdown()

		// Deferred light setup:
		std::shared_ptr<gr::Mesh> m_deferredMesh = nullptr;
		Material* m_deferredMaterial	= nullptr;

		// TODO: Move initialization to ctor initialization list
	};
}