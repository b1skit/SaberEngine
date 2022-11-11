#include "DebugConfiguration.h"
#include "Transform.h"
#include <algorithm>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/gtx/common.hpp>

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

	
	Transform::Transform() :
		m_parent(nullptr),
		
		m_localPosition(0.0f, 0.0f, 0.0f),
		m_localRotationEulerRadians(0.0f, 0.0f, 0.0f),
		m_localRotationQuat(glm::vec3(0, 0, 0)),
		m_localScale(1.0f, 1.0f, 1.0f),
		
		m_localMat(1.0f),
		m_localScaleMat(1.0f),
		m_localRotationMat(1.0f),
		m_localTranslationMat(1.0f),
				
		m_globalMat(1.0f),
		m_globalScaleMat(1.0f),
		m_globalRotationMat(1.0f),
		m_globalTranslationMat(1.0f),

		m_globalPosition(0.0f, 0.0f, 0.0f),
		m_globalRotationEulerRadians(0.0f, 0.0f, 0.0f),
		m_globalRotationQuat(glm::vec3(0, 0, 0)),
		m_globalScale(1.0f, 1.0f, 1.0f),

		m_globalRight(WorldAxisX),
		m_globalUp(WorldAxisY),
		m_globalForward(WorldAxisZ),

		m_isDirty(true)
	{
		m_children.reserve(10);
	}


	mat4 const& Transform::GetGlobalMatrix(TransformComponent component) const
	{
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


	void Transform::SetParent(Transform* newParent)
	{
		SEAssert("Cannot parent a Transform to itself", newParent != this);

		if (newParent == nullptr) // Unparent:
		{
			if (m_parent != nullptr)
			{
				m_parent->UnregisterChild(this);
				m_parent = nullptr;
			}
		}
		else // Parent:
		{
			m_parent = newParent;
			m_parent->RegisterChild(this);
		}
		
		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::TranslateLocal(vec3 amount)
	{
		m_localTranslationMat = glm::translate(m_localTranslationMat, amount);
		
		// Extract the translation from the last column of the matrix:
		m_localPosition = m_localTranslationMat[3].xyz; // == (m_localTranslationMat * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::SetLocalTranslation(vec3 position)
	{
		m_localTranslationMat = glm::translate(mat4(1.0f), position);
		m_localPosition = position;

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::RotateLocal(vec3 eulerXYZRadians)
	{
		// Compute rotations via quaternions:
		m_localRotationQuat = m_localRotationQuat * glm::quat(eulerXYZRadians);
		m_localRotationMat = glm::mat4_cast(m_localRotationQuat);

		RecomputeEulerXYZRadians();

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::RotateLocal(float angleRads, vec3 axis)
	{
		m_localRotationQuat = glm::rotate(m_localRotationQuat, angleRads, axis);
		m_localRotationMat = glm::mat4_cast(m_localRotationQuat);

		RecomputeEulerXYZRadians();

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::SetLocalRotation(vec3 eulerXYZ)
	{
		// Compute rotations via quaternions:
		m_localRotationQuat = glm::quat(eulerXYZ);
		m_localRotationMat = glm::mat4_cast(m_localRotationQuat);

		RecomputeEulerXYZRadians();

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::SetLocalRotation(quat newRotation)
	{
		m_localRotationQuat = newRotation;
		m_localRotationMat = glm::mat4_cast(newRotation);

		RecomputeEulerXYZRadians();

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::SetLocalScale(vec3 scale)
	{
		m_localScale = scale;
		m_localScaleMat = glm::scale(mat4(1.0f), scale);

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::MarkDirty()
	{
		m_isDirty = true;

		for (int i = 0; i < (int)m_children.size(); i++)
		{
			m_children.at(i)->MarkDirty();
		}
	}


	void Transform::RegisterChild(Transform* child)
	{
		if (find(m_children.begin(), m_children.end(), child) ==  m_children.end())
		{
			m_children.push_back(child);

			child->MarkDirty();
			child->RecomputeWorldTransforms();
		}
	}


	void Transform::UnregisterChild(Transform const* child)
	{
		for (size_t i = 0; i < child->m_children.size(); i++)
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
		if (!m_isDirty)
		{
			return;
		}
		m_isDirty = false; // Must immediately remove our dirty flag due to recursive calls

		m_localMat = m_localTranslationMat * m_localScaleMat * m_localRotationMat;

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
		vec3 skew;
		vec4 perspective;
		decompose(m_globalMat, m_globalScale, m_globalRotationQuat, m_globalPosition, skew, perspective);
		m_globalRotationEulerRadians = glm::eulerAngles(m_globalRotationQuat);

		// RecomputeWorldTransforms (normalized) world-space Right/Up/Forward CS axis vectors by applying m_globalRotationMat
		UpdateWorldSpaceAxis();

		for (int i = 0; i < (int)m_children.size(); i++)
		{
			m_children.at(i)->MarkDirty(); // Propagates down the entire hierarchy
			m_children.at(i)->RecomputeWorldTransforms();
		}
	}


	void Transform::UpdateWorldSpaceAxis()
	{
		const glm::mat3 rot = glm::mat3(m_globalRotationMat); // Convert to mat3 to save some operations

		// Update the world-space orientation of our local CS axis:
		m_globalRight	= normalize(rot * WorldAxisX);
		m_globalUp		= normalize(rot * WorldAxisY);
		m_globalForward = normalize(rot * WorldAxisZ);
	}


	void Transform::RecomputeEulerXYZRadians() // Should be anytime rotation has been modified
	{
		// Update our Euler rotation tracker:
		m_localRotationEulerRadians = glm::eulerAngles(m_localRotationQuat);

		// Bound the Euler radians to (-2pi, 2pi):
		m_localRotationEulerRadians.x = 
			fmod<float>(abs(m_localRotationEulerRadians.x), two_pi<float>()) * sign(m_localRotationEulerRadians.x);
		m_localRotationEulerRadians.y = 
			fmod<float>(abs(m_localRotationEulerRadians.y), two_pi<float>()) * sign(m_localRotationEulerRadians.y);
		m_localRotationEulerRadians.z = 
			fmod<float>(abs(m_localRotationEulerRadians.z), two_pi<float>()) * sign(m_localRotationEulerRadians.z);
	}
}

