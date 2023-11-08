// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "NamedObject.h"


namespace gr
{
	class Transform : public virtual en::NamedObject
	{
		/***************************************************************************************************************
		* Notes:
		* Local transformations: Translation/Rotation/Scale of a node, without considering the parent hierarchy
		* Global transformations: Final Translation/Rotation/Scale in world space, after considering the parent hierarchy
		* 
		* GLTF specifies X- as right, Z+ as forward:
		* https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#coordinate-system-and-units
		* But cameras are defined with X+ as right, Z- as forward
		* https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#cameras
		*	-> SaberEngine universally uses GLTF's Camera convention of forward = Z-
		* 
		* GLM stores matrices in memory in column-major order.
		* 
		* --------------------------------------------------------------------------------------------------------------
		* A Transform object should NOT be instantiated directly. Instead, use the SceneNode object for parental
		* hierarchy without an attached object, or the Transformable interface for objects that have a Transform.
		* 
		* Beware: Transformation updates are multi-threaded.
		***************************************************************************************************************/

	public:
		// Static world-space CS axis (SaberEngine currently uses a RHCS)
		static const glm::vec3 WorldAxisX;	// +X
		static const glm::vec3 WorldAxisY;	// +Y
		static const glm::vec3 WorldAxisZ;	// +Z

	public:
		explicit Transform(std::string const& name, Transform* parent);
		
		~Transform() = default;
		Transform(Transform const&) = default;
		Transform(Transform&&) = default;
		Transform& operator=(Transform const&) = default;
		
		// Hierarchical relationships:
		Transform* GetParent() const;
		void SetParent(Transform* newParent);
		void ReParent(Transform* newParent); // Changes parents, and preserves current global orientation
		std::vector<Transform*> const& GetChildren() const { return m_children; }

		// Translation:
		void TranslateLocal(glm::vec3 const& amount); // Apply additional translation to the current position, in local space
		void SetLocalPosition(glm::vec3 const& position); // Set the total translation of this Transform, in local space
		glm::vec3 GetLocalPosition() const;
		glm::mat4 GetLocalTranslationMat() const;
		void SetGlobalPosition(glm::vec3 position);
		glm::vec3 GetGlobalPosition() const; // World-space position
		glm::mat4 GetGlobalTranslationMat() const;

		// Rotation:
		void RotateLocal(glm::vec3 eulerXYZRadians); // Rotation is applied in XYZ order
		void RotateLocal(float angleRads, glm::vec3 axis); // Apply an axis-angle rotation to the current transform state
		void RotateLocal(glm::quat const& rotation);
		void SetLocalRotation(glm::vec3 const& eulerXYZ);
		void SetLocalRotation(glm::quat const& newRotation);
		glm::quat GetLocalRotation() const;
		glm::mat4 GetLocalRotationMat() const;
		glm::quat GetGlobalRotation() const;
		glm::mat4 GetGlobalRotationMat() const;
		glm::vec3 GetLocalEulerXYZRotationRadians() const;
		glm::vec3 GetGlobalEulerXYZRotationRadians() const;
		
		// Scale:
		void SetLocalScale(glm::vec3 const& scale);
		glm::vec3 GetLocalScale() const;
		glm::mat4 GetLocalScaleMat() const;
		glm::vec3 GetGlobalScale() const;
		glm::mat4 GetGlobalScaleMat() const;
		
		// World-space transformations:
		glm::mat4 GetGlobalMatrix() const;

		glm::vec3 GetGlobalForward() const; // World-space forward (Z+) vector
		glm::vec3 GetGlobalRight() const; // World-space right (X+) vector
		glm::vec3 GetGlobalUp() const; // World-space up (Y+) vector


		// Utility functions:
		void Recompute(); // Explicitely recompute the local matrix


		void ClearHasChangedFlag();
		bool HasChanged() const;

		void ShowImGuiWindow(bool markAsParent = false, uint32_t depth = 0);


	private:
		Transform* m_parent;
		std::vector<Transform*> m_children;

		// Transform's local orientation, *before* any parent transforms are applied:
		glm::vec3 m_localPosition;
		glm::quat m_localRotationQuat;	// Rotation as a quaternion
		glm::vec3 m_localScale;
		
		// Concatenated local TRS matrix, *before* any parent transforms are applied
		glm::mat4 m_localMat; // == T*R*S

		bool m_isDirty;	// Do our local or combinedModel matrices need to be recomputed?
		bool m_hasChanged; // Has the transform (or its parental heirarchy) changed since the last time this was false?


	private:
		void MarkDirty(); // Mark this transform as requiring a recomputation of it's global matrices
		bool IsDirty();		
		
		// Helper functions for SetParent()/Unparent():
		void RegisterChild(Transform* child);
		void UnregisterChild(Transform const* child);


	private:
		// NOTE: To prevent deadlocks, Transforms aquire locks along the hierarchy in the order of child -> parent, and
		// release in the reverse order (parent -> child) ONLY
		mutable std::recursive_mutex m_transformMutex;


	private:
		Transform() = delete;
	};


	inline void Transform::ClearHasChangedFlag()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		m_hasChanged = false;
	}


	inline bool Transform::HasChanged() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		const bool hasChanged = m_hasChanged || (m_parent != nullptr && m_parent->HasChanged());
		return hasChanged;
	}
}


