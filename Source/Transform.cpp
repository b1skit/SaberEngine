// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Transform.h"

using glm::vec3;
using glm::vec4;
using glm::quat;
using glm::mat4;
using glm::normalize;
using glm::rotate;
using glm::fmod;
using glm::abs;
using glm::two_pi;
using glm::sign;
using std::vector;
using std::find;


namespace gr
{


	// Static members:
	//----------------
	const glm::vec3 Transform::WorldAxisX	= vec3(1.0f,	0.0f,	0.0f);
	const glm::vec3 Transform::WorldAxisY	= vec3(0.0f,	1.0f,	0.0f);
	const glm::vec3 Transform::WorldAxisZ	= vec3(0.0f,	0.0f,	1.0f); // Note: SaberEngine (currently) uses a RHCS

	
	Transform::Transform(Transform* parent)
		: m_parent(parent)
		
		, m_localPosition(0.0f, 0.0f, 0.0f)
		, m_localRotationEulerRadians(0.0f, 0.0f, 0.0f)
		, m_localRotationQuat(glm::vec3(0, 0, 0))
		, m_localScale(1.0f, 1.0f, 1.0f)
		
		, m_localMat(1.0f)
		, m_localScaleMat(1.0f)
		, m_localRotationMat(1.0f)
		, m_localTranslationMat(1.0f)
				
		, m_globalMat(1.0f)
		, m_globalScaleMat(1.0f)
		, m_globalRotationMat(1.0f)
		, m_globalTranslationMat(1.0f)

		, m_globalPosition(0.0f, 0.0f, 0.0f)
		, m_globalRotationEulerRadians(0.0f, 0.0f, 0.0f)
		, m_globalRotationQuat(glm::vec3(0, 0, 0))
		, m_globalScale(1.0f, 1.0f, 1.0f)

		, m_globalRight(WorldAxisX)
		, m_globalUp(WorldAxisY)
		, m_globalForward(WorldAxisZ)

		, m_isDirty(true)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		m_children.reserve(10);
	}


	mat4 const& Transform::GetGlobalMatrix(TransformComponent component)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		RecomputeWorldTransforms();
		SEAssert("Transformation should not be dirty", !m_isDirty);

		switch (component)
		{
		case Translation:
			return m_globalTranslationMat;
			break;

		case Scale:
			return m_globalScaleMat;
			break;

		case Rotation:
			return m_globalRotationMat;
			break;

		case TRS:
		default:
			return m_globalMat;
		}		
	}


	void Transform::SetGlobalTranslation(glm::vec3 position)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		const mat4 parentGlobalTRS = m_parent ? m_parent->GetGlobalMatrix(TRS) : mat4(1.f);
		SetLocalTranslation(inverse(parentGlobalTRS) * glm::vec4(position, 0.f));
		MarkDirty();
	}


	glm::vec3 const& Transform::GetGlobalPosition()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		RecomputeWorldTransforms();
		return m_globalPosition; 
	}


	glm::vec3 const& Transform::GetGlobalEulerXYZRotationRadians()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		RecomputeWorldTransforms();
		return m_globalRotationEulerRadians; 
	}


	glm::vec3 const& Transform::GetGlobalForward()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		RecomputeWorldTransforms();
		return m_globalForward; 
	}


	glm::vec3 const& Transform::GetGlobalRight()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		RecomputeWorldTransforms();
		return m_globalRight; 
	}


	glm::vec3 const& Transform::GetGlobalUp()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		RecomputeWorldTransforms();
		return m_globalUp; 
	}


	Transform* Transform::GetParent() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		return m_parent; 
	}


	void Transform::SetParent(Transform* newParent)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		SEAssert("Cannot parent a Transform to itself", newParent != this);

		if (m_parent != nullptr)
		{
			m_parent->UnregisterChild(this);
		}

		m_parent = newParent;
	
		if (m_parent)
		{
			m_parent->RegisterChild(this);
		}
		
		MarkDirty();
	}


	void Transform::ReParent(Transform* newParent)
	{
		SharedHierarchy const& sharedHierarchy = AquireSharedHierarchy(m_parent, newParent);

		SEAssert("New parent cannot be null", newParent != nullptr);

		RecomputeWorldTransforms();
		SEAssert("Transformation should not be dirty", !m_isDirty);

		// Based on the technique presented in GPU Pro 360, Ch.15.2.5: 
		// Managing Transformations in Hierarchy: Parent Switch in Hierarchy (p.243 - p.253).
		// To move from current local space to a new local space where the parent changes but the global transformation
		// stays the same, we first find the current global transform by going up in the hierarchy to the root. Then,
		// we move down the hierarchy to the new parent:
		mat4 newLocalMatrix = glm::inverse(newParent->GetGlobalMatrix(TRS)) * GetGlobalMatrix(TRS);

		// Decompose our new matrix & update the individual components for when we call RecomputeWorldTransforms():
		vec3 skew;
		vec4 perspective;
		decompose(newLocalMatrix, m_localScale, m_localRotationQuat, m_localPosition, skew, perspective);

		m_localTranslationMat	= glm::translate(mat4(1.0f), m_localPosition);
		m_localRotationMat		= glm::mat4_cast(m_localRotationQuat);
		m_localScaleMat			= glm::scale(mat4(1.0f), m_localScale);

		SetParent(newParent);

		MarkDirty();

		ReleaseSharedHierarchy(sharedHierarchy);
	}


	void Transform::TranslateLocal(vec3 amount)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localTranslationMat = glm::translate(m_localTranslationMat, amount);
		
		// Extract the translation from the last column of the matrix:
		m_localPosition = m_localTranslationMat[3].xyz; // == (m_localTranslationMat * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;

		MarkDirty();
	}


	void Transform::SetLocalTranslation(vec3 position)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localTranslationMat = glm::translate(mat4(1.0f), position);
		m_localPosition = position;

		MarkDirty();
	}


	glm::vec3 const& Transform::GetLocalPosition()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		RecomputeWorldTransforms();
		return m_localPosition;
	}


	void Transform::RotateLocal(vec3 eulerXYZRadians)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		// Compute rotations via quaternions:
		m_localRotationQuat = m_localRotationQuat * glm::quat(eulerXYZRadians);
		m_localRotationMat = glm::mat4_cast(m_localRotationQuat);

		RecomputeEulerXYZRadians();
		MarkDirty();
	}


	void Transform::RotateLocal(float angleRads, vec3 axis)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localRotationQuat = glm::rotate(m_localRotationQuat, angleRads, axis);
		m_localRotationMat = glm::mat4_cast(m_localRotationQuat);

		RecomputeEulerXYZRadians();
		MarkDirty();
	}


	void Transform::SetLocalRotation(vec3 eulerXYZ)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		// Compute rotations via quaternions:
		m_localRotationQuat = glm::quat(eulerXYZ);
		m_localRotationMat = glm::mat4_cast(m_localRotationQuat);

		RecomputeEulerXYZRadians();
		MarkDirty();
	}


	void Transform::SetLocalRotation(quat newRotation)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localRotationQuat = newRotation;
		m_localRotationMat = glm::mat4_cast(newRotation);

		RecomputeEulerXYZRadians();
		MarkDirty();
	}


	glm::vec3 const& Transform::GetLocalEulerXYZRotationRadians()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		RecomputeWorldTransforms();
		return m_localRotationEulerRadians;
	}


	void Transform::SetLocalScale(vec3 scale)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localScale = scale;
		m_localScaleMat = glm::scale(mat4(1.0f), scale);

		MarkDirty();
	}


	void Transform::MarkDirty()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_isDirty = true;
	}


	bool Transform::IsDirty()
	{
		AquireLockHierarchy();

		bool result = m_isDirty || (m_parent != nullptr && m_parent->IsDirty());

		ReleaseLockHierarchy();

		return result;
	}


	void Transform::AquireLockHierarchy()
	{
		m_transformMutex.lock();

		if(m_parent)
		{
			m_parent->AquireLockHierarchy();
		}
	}


	void Transform::ReleaseLockHierarchy()
	{
		if (m_parent)
		{
			m_parent->ReleaseLockHierarchy();
		}

		m_transformMutex.unlock();
	}


	void Transform::AquireSharedHierarchyHelper(SharedHierarchy& sharedHierarchy, Transform* cur)
	{
		if (cur == nullptr)
		{
			return; // Previous node was a root; We're done!
		}

		const size_t curPtrVal = reinterpret_cast<size_t>(cur);
		auto const& result = sharedHierarchy.visitedTransforms.find(curPtrVal);

		if (result == sharedHierarchy.visitedTransforms.end())
		{
			// Found a new node; Lock it, add it to the list, and keep traversing
			cur->m_transformMutex.lock();
			
			sharedHierarchy.visitedTransforms.insert({ curPtrVal, true });
			sharedHierarchy.visitOrder.emplace_back(cur);

			AquireSharedHierarchyHelper(sharedHierarchy, cur->GetParent());
		}
		else
		{
			return; // Found an existing node; We're done!
		}
	}


	Transform::SharedHierarchy Transform::AquireSharedHierarchy(Transform* parentA, Transform* parentB)
	{
		m_transformMutex.lock();

		SharedHierarchy sharedHierarchy;

		// Store the address of the current Transform object
		sharedHierarchy.visitedTransforms.insert({ reinterpret_cast<size_t>(this), true });
		sharedHierarchy.visitOrder.emplace_back(this);

		// Aquire locks on all nodes between each parent, and their respective roots
		AquireSharedHierarchyHelper(sharedHierarchy, parentA);
		AquireSharedHierarchyHelper(sharedHierarchy, parentB);

		return sharedHierarchy;
	}


	void Transform::ReleaseSharedHierarchy(SharedHierarchy const& sharedHierarchy)
	{
		SEAssert("Too many transforms", sharedHierarchy.visitOrder.size() < std::numeric_limits<int32_t>::max());

		// Release in reverse order: Parent -> child
		for (int32_t i = static_cast<int32_t>(sharedHierarchy.visitOrder.size()) - 1; i >= 0; i--)
		{
			sharedHierarchy.visitOrder[i]->m_transformMutex.unlock();
		}
	}


	void Transform::RegisterChild(Transform* child)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		SEAssert("Child must update their parent pointer", child->m_parent == this);

		if (find(m_children.begin(), m_children.end(), child) ==  m_children.end())
		{
			m_children.push_back(child);
		}
		else
		{
			SEAssertF("Child is already registered");
		}
	}


	void Transform::UnregisterChild(Transform const* child)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		for (size_t i = 0; i < m_children.size(); i++)
		{
			if (m_children.at(i) == child)
			{
				m_children.erase(m_children.begin() + i); // Erase the ith element
				break;
			}
		}
	}

	
	void Transform::RecomputeWorldTransforms()
	{
		AquireLockHierarchy();

		if (!IsDirty())
		{
			ReleaseLockHierarchy();

			return;
		}

		m_localMat = m_localTranslationMat * m_localRotationMat * m_localScaleMat;

		// Update the combined world-space transformations with respect to the parent hierarchy:
		if (m_parent != nullptr)
		{
			m_globalMat				= m_parent->GetGlobalMatrix(TRS) * m_localMat;
			m_globalScaleMat		= m_parent->GetGlobalMatrix(Scale) * m_localScaleMat;
			m_globalRotationMat		= m_parent->GetGlobalMatrix(Rotation) * m_localRotationMat;
			m_globalTranslationMat	= m_parent->GetGlobalMatrix(Translation) * m_localTranslationMat;
		}
		else
		{
			m_globalMat				= m_localMat;
			m_globalScaleMat		= m_localScaleMat;
			m_globalRotationMat		= m_localRotationMat;
			m_globalTranslationMat	= m_localTranslationMat;
		}

		// Decompose our world matrix & update the individual components:
		{
			vec3 skew;
			vec4 perspective;
			decompose(m_globalMat, m_globalScale, m_globalRotationQuat, m_globalPosition, skew, perspective);
			m_globalRotationEulerRadians = glm::eulerAngles(m_globalRotationQuat);
		}

		// Update the world-space orientation of our local CS axis:
		{
			const glm::mat3 rot = glm::mat3(m_globalRotationMat); // Convert to mat3 to save some operations
			m_globalRight = normalize(rot * WorldAxisX);
			m_globalUp = normalize(rot * WorldAxisY);
			m_globalForward = normalize(rot * WorldAxisZ);
		}

		m_isDirty = false;

		ReleaseLockHierarchy();
	}


	void Transform::RecomputeEulerXYZRadians() // Should be called anytime rotation has been modified
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		// Update our Euler rotation tracker:
		m_localRotationEulerRadians = glm::eulerAngles(m_localRotationQuat);

		// Bound the Euler radians to (-2pi, 2pi):
		m_localRotationEulerRadians.x = 
			fmod<float>(abs(m_localRotationEulerRadians.x), two_pi<float>()) * sign(m_localRotationEulerRadians.x);
		m_localRotationEulerRadians.y = 
			fmod<float>(abs(m_localRotationEulerRadians.y), two_pi<float>()) * sign(m_localRotationEulerRadians.y);
		m_localRotationEulerRadians.z = 
			fmod<float>(abs(m_localRotationEulerRadians.z), two_pi<float>()) * sign(m_localRotationEulerRadians.z);

		MarkDirty();
	}
}

