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
		/***************************************************************************************************************
		* Notes:
		* Model space = local -> world transformations, without consideration of the parent transformations/hierarchy
		* World space = Final world transformations, after considering the parent transformations/hierarchy
		* 
		* GLTF specifies X- as right, Z+ as forward:
		* https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#coordinate-system-and-units
		* But cameras are defined with X+ as right, Z- as forward
		* https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#cameras
		*	-> Transforms use GLTF's Camera convention
		* 
		* GLM stores matrices in memory in column-major order.
		*	OpenGL: Vectors are column vectors
		*	D3D: Vectors are row vectors
		*	-> Expect matrices to be transposes of each other between APIs
		***************************************************************************************************************/

	public:
		enum TransformComponent
		{
			Translation,
			Scale,
			Rotation,
			WorldModel, // Composition of Translation, Rotation, and Scale

			TransformComponent_Count
		};

		// Static world-space CS axis (SaberEngine currently uses a RHCS)
		static const glm::vec3 WORLD_X;	// +X
		static const glm::vec3 WORLD_Y;	// +Y
		static const glm::vec3 WORLD_Z;	// +Z

	public:
		Transform();
		
		~Transform() = default;
		Transform(Transform const&) = default;
		Transform(Transform&&) = default;
		Transform& operator=(Transform const&) = default;
		
		// Transform functionality:
		inline Transform* GetParent() const { return m_parent; }
		void SetParent(Transform* newParent);

		glm::mat4 const& GetWorldMatrix(TransformComponent component = WorldModel) const;
		
		void TranslateModel(glm::vec3 amount); // Apply additional translation to the current position, in model space
		
		void SetModelPosition(glm::vec3 position); // Set the total translation of this Transform, in model space
		inline glm::vec3 const& GetModelPosition() const { return m_modelPosition; }
		inline glm::vec3 const& GetWorldPosition() const { return m_worldPosition; } // Get the final world-space position

		// Apply additional rotation:
		void RotateModel(glm::vec3 eulerXYZRadians); // Rotation is applied in XYZ order
		void RotateModel(float angleRads, glm::vec3 axis); // Apply an axis-angle rotation to the current transform state

		void SetModelRotation(glm::vec3 eulerXYZ);
		void SetModelRotation(glm::quat newRotation);

		inline glm::vec3 const& GetModelEulerXYZRotationRadians() const { return m_modelRotationEulerRadians; }
		inline glm::vec3 const& GetWorldEulerXYZRotationRadians() const { return m_worldRotationEulerRadians; }

		void SetModelScale(glm::vec3 scale);

		inline glm::vec3 const& ForwardWorld() const { return m_worldForward; } // Transform's world-space forward (Z+) vector
		inline glm::vec3 const& RightWorld() const { return m_worldRight; } // Transform's world-space right (X+) vector
		inline glm::vec3 const& UpWorld() const { return m_worldUp; } // Transform's world-space up (Y+) vector
		// TODO: Add ForwardModel, RightModel, UpModel
		

	public: // Static helper functions:
		
		// Rotate a targetVector about an axis by radians
		static glm::vec3& RotateVector(glm::vec3& targetVector, const float radians, glm::vec3 const& axis);
		// TODO: Does this need to be a static member? Why not just compute this inline when it's needed? (Eg. PlayerObject)
		

	private:
		Transform* m_parent;
		std::vector<Transform*> m_children;

		// Transform's world-space orientation components, *before* any parent transforms are applied:
		glm::vec3 m_modelPosition;
		glm::vec3 m_modelRotationEulerRadians; // Rotation as Euler angles (pitch, yaw, roll), in Radians
		glm::quat m_modelRotationQuat;	// Rotation as a quaternion
		glm::vec3 m_modelScale;
		
		// Transform's world-space oreientation component matrices, *before* any parent transforms are applied
		glm::mat4 m_modelMat; // == T*R*S
		glm::mat4 m_modelScaleMat;
		glm::mat4 m_modelRotationMat;
		glm::mat4 m_modelTranslationMat;

		// Combined world-space transformation, with respect to the entire transformation hierarchy
		glm::mat4 m_worldMat;
		glm::mat4 m_worldScaleMat;
		glm::mat4 m_worldRotationMat;
		glm::mat4 m_worldTranslationMat;

		// Transform's world-space orientation components, *after* any parent transforms are applied:
		glm::vec3 m_worldPosition;
		glm::vec3 m_worldRotationEulerRadians;
		glm::quat m_worldRotationQuat;
		glm::vec3 m_worldScale;

		// Transform's final world-space CS axis, *after* parent transformations are applied
		// Note: SaberEngine currently uses a RHCS
		glm::vec3 m_worldRight;
		glm::vec3 m_worldUp;
		glm::vec3 m_worldForward;

		bool m_isDirty;	// Do our model or combinedModel matrices need to be recomputed?


	private:
		// TODO: MarkDirty() should be used trigger RecomputeWorldTransforms() on an as-needed basis; currently we
		// pair these instructions since we have some const dependencies that need to be untangled...

		void MarkDirty(); // Mark this transform as dirty, requiring a recomputation of it's local matrices
		void RecomputeWorldTransforms(); // Recomute the components of the model matrix. Sets isDirty to false

		// Helper functions for SetParent()/Unparent():
		void RegisterChild(Transform* child);
		void UnregisterChild(Transform const* child);

		// RecomputeWorldTransforms (normalized) world-space Right/Up/Forward CS axis vectors by applying m_worldRotationMat
		void UpdateWorldSpaceAxis();

		void RecomputeEulerXYZRadians(); // Helper: Updates m_modelRotationEulerRadians from m_modelRotationQuat
	};
}


