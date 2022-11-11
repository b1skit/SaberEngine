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
		* Local transformations: Translation/Rotation/Scale of a node, relative to any parent hierarchy
		* Global transformations: Final Translation/Rotation/Scale in world space, after considering the parent hierarchy
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
			Rotation,
			Scale,			
			TRS, // Composition of Translation, Rotation, and Scale

			TransformComponent_Count
		};

		// Static world-space CS axis (SaberEngine currently uses a RHCS)
		static const glm::vec3 WorldAxisX;	// +X
		static const glm::vec3 WorldAxisY;	// +Y
		static const glm::vec3 WorldAxisZ;	// +Z

	public:
		Transform();
		
		~Transform() = default;
		Transform(Transform const&) = default;
		Transform(Transform&&) = default;
		Transform& operator=(Transform const&) = default;
		
		// Transform functionality:
		inline Transform* GetParent() const { return m_parent; }
		void SetParent(Transform* newParent);

		// Local-space translation:
		void TranslateLocal(glm::vec3 amount); // Apply additional translation to the current position, in local space
		void SetLocalTranslation(glm::vec3 position); // Set the total translation of this Transform, in local space
		inline glm::vec3 const& GetLocalPosition() const { return m_localPosition; } // The local position

		// Local-space rotation:
		void RotateLocal(glm::vec3 eulerXYZRadians); // Rotation is applied in XYZ order
		void RotateLocal(float angleRads, glm::vec3 axis); // Apply an axis-angle rotation to the current transform state
		void SetLocalRotation(glm::vec3 eulerXYZ);
		void SetLocalRotation(glm::quat newRotation);
		inline glm::vec3 const& GetLocalEulerXYZRotationRadians() const { return m_localRotationEulerRadians; }
		
		// Local-space scale:
		void SetLocalScale(glm::vec3 scale);


		// Global transformations: 
		glm::mat4 const& GetGlobalMatrix(TransformComponent component) const;

		// World-space translation:
		inline glm::vec3 const& GetGlobalPosition() const { return m_globalPosition; } // World-space position

		// World-space rotation:
		inline glm::vec3 const& GetGlobalEulerXYZRotationRadians() const { return m_globalRotationEulerRadians; }

		// World-space coordinate system axis:
		inline glm::vec3 const& GetGlobalForward() const { return m_globalForward; } // World-space forward (Z+) vector
		inline glm::vec3 const& GetGlobalRight() const { return m_globalRight; } // World-space right (X+) vector
		inline glm::vec3 const& GetGlobalUp() const { return m_globalUp; } // World-space up (Y+) vector
		// TODO: Add ForwardModel, RightModel, UpModel
		

	private:
		Transform* m_parent;
		std::vector<Transform*> m_children;

		// Transform's local orientation, *before* any parent transforms are applied:
		glm::vec3 m_localPosition;
		glm::vec3 m_localRotationEulerRadians; // Rotation as Euler angles (pitch, yaw, roll), in Radians
		glm::quat m_localRotationQuat;	// Rotation as a quaternion
		glm::vec3 m_localScale;
		
		// Transform's local oreientation component matrices, *before* any parent transforms are applied
		glm::mat4 m_localMat; // == T*R*S
		glm::mat4 m_localScaleMat;
		glm::mat4 m_localRotationMat;
		glm::mat4 m_localTranslationMat;

		// Combined world-space transformation, with respect to the entire transformation hierarchy
		glm::mat4 m_globalMat;
		glm::mat4 m_globalScaleMat;
		glm::mat4 m_globalRotationMat;
		glm::mat4 m_globalTranslationMat;

		// Transform's world-space orientation components, *after* any parent transforms are applied:
		glm::vec3 m_globalPosition;
		glm::vec3 m_globalRotationEulerRadians;
		glm::quat m_globalRotationQuat;
		glm::vec3 m_globalScale;

		// Transform's final world-space CS axis, *after* parent transformations are applied
		// Note: SaberEngine currently uses a RHCS
		glm::vec3 m_globalRight;
		glm::vec3 m_globalUp;
		glm::vec3 m_globalForward;

		bool m_isDirty;	// Do our local or combinedModel matrices need to be recomputed?


	private:
		// TODO: MarkDirty() should be used trigger RecomputeWorldTransforms() on an as-needed basis; currently we
		// pair these instructions since we have some const dependencies that need to be untangled...

		void MarkDirty(); // Mark this transform as dirty, requiring a recomputation of it's local matrices
		void RecomputeWorldTransforms(); // Recomute the components of the local matrix. Sets isDirty to false

		// Helper functions for SetParent()/Unparent():
		void RegisterChild(Transform* child);
		void UnregisterChild(Transform const* child);

		// RecomputeWorldTransforms (normalized) world-space Right/Up/Forward CS axis vectors by applying m_globalRotationMat
		void UpdateWorldSpaceAxis();

		void RecomputeEulerXYZRadians(); // Helper: Updates m_localRotationEulerRadians from m_localRotationQuat
	};
}


