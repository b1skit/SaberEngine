// Game object transformation component

#pragma once

#define GLM_FORCE_SWIZZLE // Enable swizzle operators
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

using glm::vec3;
using glm::vec4;
using glm::quat;
using glm::mat4;
using std::vector;


namespace SaberEngine
{
	enum MODEL_MATRIX_COMPONENT
	{
		WORLD_TRANSLATION,
		WORLD_SCALE,
		WORLD_ROTATION,
		
		WORLD_MODEL
	};


	class Transform
	{
	public:
		Transform();

		// TODO: Copy constructor ??

		// Get the model matrix, used to transform from local->world space
		mat4 Model(MODEL_MATRIX_COMPONENT component = WORLD_MODEL);
		
		// Hierarchy functions:
		inline Transform*	Parent() const { return m_parent; }
		void				Parent(Transform* newParent);
		
		// Functionality:
		//---------------

		// Translate, in (relative) world space
		void Translate(vec3 amount);

		// Set the position, in (relative) world space
		void SetWorldPosition(vec3 position);

		// Get the position, in (relative) world space
		vec3 const&		WorldPosition();

		// Rotate about the world X, Y, Z axis, in that order
		// eulerXYZ = Rotation angles about each axis, in RADIANS
		void			Rotate(vec3 eulerXYZ);

		vec3 const&		GetEulerRotation();
		void			SetWorldRotation(vec3 eulerXYZ);
		void			SetWorldRotation(quat newRotation);
		
		// Scaling:
		void SetWorldScale(vec3 scale);


		// Mark this transform as dirty, requiring a recomputation of it's local matrices
		void MarkDirty();


		// Getters/Setters:
		//-----------------

		// Transform's world-space forward (Z+) vector
		inline vec3 const& Forward() const { return m_forward; }

		// Transform's world-space right (X+) vector
		inline vec3 const& Right()	const { return m_right; }

		// Transform's world-space up (Y+) vector
		inline vec3 const& Up()		const { return m_up; }


		// World CS axis: SaberEngine always uses a RHCS
		static const vec3 WORLD_X;		// +X
		static const vec3 WORLD_Y;		// +Y
		static const vec3 WORLD_Z;		// +Z
		
		// Print debug information for this transform. Does nothing if DEBUG_LOG_OUTPUT is undefined in BuildConfiguration.h
		void DebugPrint();

		// Static helper functions:
		//-------------------------
		// Rotate a targetVector about an axis by radians
		static vec3& RotateVector(vec3& targetVector, float const & radians, vec3 const & axis);


	protected:
		// Helper functions for SetParent()/Unparent():
		void RegisterChild(Transform* child);
		void UnregisterChild(Transform const* child);


	private:
		Transform* m_parent = nullptr;
		vector<Transform*> m_children;

		// World-space orientation:
		vec3 m_worldPosition		= vec3(0.0f, 0.0f, 0.0f);	// World position, relative to any parent transforms
		vec3 m_eulerWorldRotation	= vec3(0.0f, 0.0f, 0.0f);	// Current world-space Euler angles (pitch, yaw, roll), in Radians
		vec3 m_worldScale			= vec3(1.0f, 1.0f, 1.0f);
		
		// Local CS axis: SaberEngine always uses a RHCS
		vec3 m_right		= WORLD_X;
		vec3 m_up			= WORLD_Y;
		vec3 m_forward	= WORLD_Z;

		// model == T*R*S
		mat4 m_model			= mat4(1.0f);
		mat4 m_scale			= mat4(1.0f);
		mat4 m_rotation		= mat4(1.0f);
		mat4 m_translation	= mat4(1.0f);

		mat4 m_combinedModel			= mat4(1.0f);
		mat4 m_combinedScale			= mat4(1.0f);
		mat4 m_combinedRotation		= mat4(1.0f);
		mat4 m_combinedTranslation	= mat4(1.0f);

		quat m_worldRotation = glm::quat(vec3(0,0,0));	// Rotation of this transform. Used to assemble rotation matrix

		bool m_isDirty;			// Do our model or combinedModel matrices need to be recomputed?


		// Private functions:
		//-------------------

		// Recomute the components of the model matrix. Sets isDirty to false
		void Recompute();

		// Helper function: Recomputes world orientation of the right/up/forward local CS axis vectors according to the current rotation mat4
		void UpdateLocalAxis();

		// Helper function: Clamps Euler angles to be in (-2pi, 2pi)
		void BoundEulerAngles();
	};
}


