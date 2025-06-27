// � 2022 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/RenderObjectIDs.h"


namespace fr
{
	class EntityManager;


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
		
		Transform(Transform&&) noexcept;
		~Transform();
		
		// Hierarchical relationships:
		Transform* GetParent() const;
		void SetParent(Transform* newParent);
		void ReParent(Transform* newParent); // Changes parents, and preserves current global orientation
		std::vector<Transform*> const& GetChildren() const { return m_children; }

		// Translation:
		void TranslateLocal(glm::vec3 const& amount); // Apply additional translation to the current position, in local space
		void SetLocalTranslation(glm::vec3 const& position); // Set the total translation of this Transform, in local space
		glm::vec3 GetLocalTranslation() const;
		glm::vec3 GetLocalTranslation();
		glm::mat4 GetLocalTranslationMat() const;
		glm::mat4 GetLocalTranslationMat();
		void SetGlobalTranslation(glm::vec3 position);
		glm::vec3 GetGlobalTranslation() const; // Transform must not be dirty
		glm::vec3 GetGlobalTranslation();
		glm::mat4 GetGlobalTranslationMat() const;

		// Rotation:
		void RotateLocal(glm::vec3 eulerXYZRadians); // Rotation is applied in XYZ order
		void RotateLocal(float angleRads, glm::vec3 axis); // Apply an axis-angle rotation to the current transform state
		void RotateLocal(glm::quat const& rotation);
		void SetLocalRotation(glm::vec3 const& eulerXYZ);
		void SetLocalRotation(glm::quat const& newRotation);
		glm::quat GetLocalRotation() const;
		glm::quat GetLocalRotation();
		glm::mat4 GetLocalRotationMat() const;
		glm::mat4 GetLocalRotationMat();

		void SetGlobalRotation(glm::quat const&);
		glm::quat GetGlobalRotation() const;
		glm::quat GetGlobalRotation();
		glm::mat4 GetGlobalRotationMat() const;
		glm::mat4 GetGlobalRotationMat();
		glm::vec3 GetLocalEulerXYZRotationRadians() const;
		glm::vec3 GetLocalEulerXYZRotationRadians();
		glm::vec3 GetGlobalEulerXYZRotationRadians() const;
		glm::vec3 GetGlobalEulerXYZRotationRadians();
		
		// Scale:
		void SetLocalScale(glm::vec3 const& scale);
		glm::vec3 GetLocalScale() const;
		glm::vec3 GetLocalScale();
		glm::mat4 GetLocalScaleMat() const;
		glm::mat4 GetLocalScaleMat();

		void SetGlobalScale(glm::vec3 const& scale);
		glm::vec3 GetGlobalScale() const;
		glm::vec3 GetGlobalScale();
		glm::mat4 GetGlobalScaleMat() const;
		glm::mat4 GetGlobalScaleMat();
		
		// World-space transformations:
		glm::mat4 GetGlobalMatrix() const; // Transform must not be dirty
		glm::mat4 GetGlobalMatrix();

		glm::vec3 GetGlobalForward() const; // World-space forward (Z+) vector
		glm::vec3 GetGlobalForward();
		glm::vec3 GetGlobalRight() const; // World-space right (X+) vector
		glm::vec3 GetGlobalRight();
		glm::vec3 GetGlobalUp() const; // World-space up (Y+) vector
		glm::vec3 GetGlobalUp();

		// Local:
		glm::mat4 GetLocalMatrix() const;
		glm::mat4 GetLocalMatrix();

		// Utility functions:
		bool Recompute(bool returnHasChanged = false); // Recompute the local matrix. Returns true if recomputation occurred

		void ClearHasChangedFlag();
		bool HasChanged() const;

		gr::TransformID GetTransformID() const;


	public:
		void ShowImGuiWindow(fr::EntityManager&, entt::entity owningEntity);
		
		// Hierarchy view window
		static void ShowImGuiWindow(fr::EntityManager&, std::vector<entt::entity> const& rootNodeEntities, bool* show); 


	private:
		void ImGuiHelper_ShowData(uint64_t uniqueID);
		void ImGuiHelper_Modify(uint64_t uniqueID);
		void ImGuiHelper_Hierarchy(fr::EntityManager&, entt::entity owningEntity, uint64_t uniqueID);

		static void ImGuiHelper_ShowHierarchy(
			fr::EntityManager&,
			entt::entity nodeEntity,
			bool highlightCurrentNode = false,
			bool expandAllState = false,
			bool expandChangeTriggered = false);


	private:
		// Ordered from largest to smallest to reduce padding:
		
		// 64-byte aligned matrices (largest)
		glm::mat4 m_localMat; // == T*R*S
		glm::mat4 m_globalMat;

		// 32-byte containers
		std::vector<Transform*> m_children; 

		// 16-byte vec4/quat types
		glm::quat m_localRotationQuat;	// Rotation as a quaternion

		// 12-byte vec3 types
		glm::vec3 m_localTranslation;
		glm::vec3 m_localScale;

		// 8-byte pointers and IDs
		Transform* m_parent;
		const gr::TransformID m_transformID;

		// 1-byte bools (grouped together to minimize padding)
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
		Transform(Transform const&) = delete;
		Transform& operator=(Transform const&) = delete;
		Transform& operator=(Transform&&) noexcept = delete;


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


