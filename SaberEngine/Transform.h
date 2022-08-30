// Game object transformation component

#pragma once

#include <vector>

#define GLM_FORCE_SWIZZLE // Enable swizzle operators
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>


namespace gr
{
	class Transform
	{
	public:
		enum ModelMatrixComponent
		{
			WorldTranslation,
			WorldScale,
			WorldRotation,

			WorldModel
		};

	public:
		Transform();
		
		~Transform() = default;
		Transform(Transform const&) = default;
		Transform(Transform&&) = default;
		Transform& operator=(Transform const&) = default;
		

		// Get the model matrix, used to transform from local->world space
		glm::mat4 Model(ModelMatrixComponent component = WorldModel) const;
		
		// Hierarchy functions:
		inline Transform* GetParent() const { return m_parent; }
		void SetParent(Transform* newParent);
		
		// Functionality:
		//---------------

		void Translate(glm::vec3 amount); // Translate, in (relative) world space
		
		void SetWorldPosition(glm::vec3 position); // Set the world space position
		glm::vec3 const& GetWorldPosition() const; // Get the world space position

		// Rotate about the world X, Y, Z axis, in that order
		// eulerXYZ = Rotation angles about each axis, in RADIANS
		void Rotate(glm::vec3 eulerXYZ);

		glm::vec3 const& GetEulerRotation() const;
		void SetWorldRotation(glm::vec3 eulerXYZ);
		void SetWorldRotation(glm::quat newRotation);
		
		// Scaling:
		void SetWorldScale(glm::vec3 scale);

		inline glm::vec3 const& Forward() const { return m_forward; } // Transform's world-space forward (Z+) vector
		inline glm::vec3 const& Right() const { return m_right; } // Transform's world-space right (X+) vector
		inline glm::vec3 const& Up() const { return m_up; } // Transform's world-space up (Y+) vector

		// Recomute the components of the model matrix. Sets isDirty to false
		void Recompute();

	public: // Static helper functions:
		
		// Rotate a targetVector about an axis by radians
		static glm::vec3& RotateVector(glm::vec3& targetVector, float const & radians, glm::vec3 const & axis);

		// Static world CS axis: SaberEngine always uses a RHCS
		static const glm::vec3 WORLD_X;	// +X
		static const glm::vec3 WORLD_Y;	// +Y
		static const glm::vec3 WORLD_Z;	// +Z


	private:
		Transform* m_parent;
		std::vector<Transform*> m_children;

		// Note: GLM stores matrices in memory in column-major order.
		// OpenGL: Vectors are column vectors
		// D3D: Vectors are row vectors
		// -> Expect matrices to be transposes of each other between APIs

		// World-space orientation:
		glm::vec3 m_worldPosition;	// World position, relative to parent transforms
		glm::vec3 m_eulerWorldRotation;	// World-space Euler angles (pitch, yaw, roll), in Radians
		glm::vec3 m_worldScale;
		
		// Local CS axis: SaberEngine always uses a RHCS
		glm::vec3 m_right;
		glm::vec3 m_up;
		glm::vec3 m_forward;

		// model == T*R*S
		glm::mat4 m_model;
		glm::mat4 m_scale;
		glm::mat4 m_rotation;
		glm::mat4 m_translation;

		// Combined transformation heirarchy (parents etc)
		glm::mat4 m_combinedModel;
		glm::mat4 m_combinedScale;
		glm::mat4 m_combinedRotation;
		glm::mat4 m_combinedTranslation;

		glm::quat m_worldRotation; // Rotation of this transform. For assembling rotation matrix

		bool m_isDirty;			// Do our model or combinedModel matrices need to be recomputed?


	private:
		// Mark this transform as dirty, requiring a recomputation of it's local matrices
		void MarkDirty();

		// Helper functions for SetParent()/Unparent():
		void RegisterChild(Transform* child);
		void UnregisterChild(Transform const* child);

		// Recomputes world orientation of the right/up/forward local CS axis vectors according to the current rotation mat4
		void UpdateLocalAxis();

		// Helper function: Clamps Euler angles to be in (-2pi, 2pi)
		void BoundEulerAngles();
	};
}


