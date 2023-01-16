// � 2022 Adam Badke. All rights reserved.
#pragma once


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
		explicit Transform(Transform* parent);
		
		~Transform() = default;
		Transform(Transform const&) = default;
		Transform(Transform&&) = default;
		Transform& operator=(Transform const&) = default;
		
		// Transform functionality:
		Transform* GetParent() const;
		void SetParent(Transform* newParent);
		void ReParent(Transform* newParent); // Changes parents, and preserves current global orientation
		std::vector<Transform*> const& GetChildren() { return m_children; }

		// Local-space translation:
		void TranslateLocal(glm::vec3 amount); // Apply additional translation to the current position, in local space
		void SetLocalTranslation(glm::vec3 position); // Set the total translation of this Transform, in local space
		glm::vec3 const& GetLocalPosition(); // The local position

		// Local-space rotation:
		void RotateLocal(glm::vec3 eulerXYZRadians); // Rotation is applied in XYZ order
		void RotateLocal(float angleRads, glm::vec3 axis); // Apply an axis-angle rotation to the current transform state
		void SetLocalRotation(glm::vec3 eulerXYZ);
		void SetLocalRotation(glm::quat newRotation);
		glm::vec3 const& GetLocalEulerXYZRotationRadians();
		
		// Local-space scale:
		void SetLocalScale(glm::vec3 scale);


		// Global transformations: 
		void Recompute() { RecomputeWorldTransforms(); }; // Explicitely recompute the global transforms

		glm::mat4 const& GetGlobalMatrix(TransformComponent component);

		// World-space translation:
		void SetGlobalTranslation(glm::vec3 position);
		glm::vec3 const& GetGlobalPosition(); // World-space position

		// World-space rotation:
		glm::vec3 const& GetGlobalEulerXYZRotationRadians();

		// World-space coordinate system axis:
		glm::vec3 const& GetGlobalForward(); // World-space forward (Z+) vector
		glm::vec3 const& GetGlobalRight(); // World-space right (X+) vector
		glm::vec3 const& GetGlobalUp(); // World-space up (Y+) vector
		

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
		void RecomputeWorldTransforms(); // Recomputes the the global matrices
		void RecomputeEulerXYZRadians(); // Helper: Updates m_localRotationEulerRadians from m_localRotationQuat


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

		// Aquire/release all locks between the current node, and the root
		void AquireLockHierarchy();
		void ReleaseLockHierarchy();

		// Aquire/release all locks in 2 (potentially disjoint) hierarchies. Useful when re-parenting a Transform
		struct SharedHierarchy
		{
			std::unordered_map<size_t, bool> visitedTransforms;
			std::vector<Transform*> visitOrder;
		};
		void AquireSharedHierarchyHelper(SharedHierarchy& sharedHierarchy, Transform* cur);
		SharedHierarchy AquireSharedHierarchy(Transform* parentA, Transform* parentB);
		void ReleaseSharedHierarchy(SharedHierarchy const& sharedHierarchy);


	private:
		Transform() = delete;
	};
}

