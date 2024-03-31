// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"


namespace fr
{
	class Transform
	{
		/***************************************************************************************************************
		* Notes:
		* SaberEngine uses a RHCS.
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
		***************************************************************************************************************/


	public:
		explicit Transform(Transform* parent);
		
		Transform(Transform&&);
		~Transform();
		
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
		glm::vec3 GetGlobalPosition(); // May cause recomputation
		glm::vec3 GetGlobalPosition() const; // Transform must not be dirty
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
		glm::mat4 GetGlobalMatrix(); // May cause recomputation
		glm::mat4 GetGlobalMatrix() const; // Transform must not be dirty

		glm::vec3 GetGlobalForward() const; // World-space forward (Z+) vector
		glm::vec3 GetGlobalRight() const; // World-space right (X+) vector
		glm::vec3 GetGlobalUp() const; // World-space up (Y+) vector


		// Utility functions:
		void Recompute(); // Explicitely recompute the local matrix


		void ClearHasChangedFlag();
		bool HasChanged() const;

		gr::TransformID GetTransformID() const;

		void ShowImGuiWindow();

		static void ShowImGuiWindow(std::vector<fr::Transform*> const& rootNodes, bool* show); // Hierarchy view window

	private:
		void ImGuiHelper_ShowData(uint64_t uniqueID);
		void ImGuiHelper_Modify(uint64_t uniqueID);
		static void ImGuiHelper_ShowHierarchy(
			fr::Transform* node, 
			bool highlightCurrentNode = false,
			bool expandAllState = false, 
			bool expandChangeTriggered = false);

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

		const gr::TransformID m_transformID;


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
		Transform(Transform const&) = delete;
		Transform& operator=(Transform const&) = delete;
		Transform& operator=(Transform&&) = delete;


	private: // Static TransformID functionality:
		static std::atomic<gr::TransformID> s_transformIDs;
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


	inline gr::TransformID Transform::GetTransformID() const
	{
		return m_transformID;
	}
}


