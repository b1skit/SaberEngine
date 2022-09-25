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
	const glm::vec3 Transform::WORLD_X	= vec3(1.0f,	0.0f,	0.0f);
	const glm::vec3 Transform::WORLD_Y	= vec3(0.0f,	1.0f,	0.0f);
	const glm::vec3 Transform::WORLD_Z	= vec3(0.0f,	0.0f,	1.0f); // Note: SaberEngine (currently) uses a RHCS


	// Static helper functions:
	//-------------------------

	vec3& Transform::RotateVector(vec3& targetVector, const float radians, vec3 const& axis)
	{
		mat4 rotation = glm::rotate(mat4(1.0f), radians, axis);

		targetVector = (rotation * vec4(targetVector, 0.0f)).xyz();
		return targetVector;
	}

	/********************************/

	
	Transform::Transform() :
		m_parent(nullptr),
		
		m_modelPosition(0.0f, 0.0f, 0.0f),
		m_modelRotationEulerRadians(0.0f, 0.0f, 0.0f),
		m_modelRotationQuat(glm::vec3(0, 0, 0)),
		m_modelScale(1.0f, 1.0f, 1.0f),
		
		m_modelMat(1.0f),
		m_modelScaleMat(1.0f),
		m_modelRotationMat(1.0f),
		m_modelTranslationMat(1.0f),
				
		m_worldMat(1.0f),
		m_worldScaleMat(1.0f),
		m_worldRotationMat(1.0f),
		m_worldTranslationMat(1.0f),

		m_worldPosition(0.0f, 0.0f, 0.0f),
		m_worldRotationEulerRadians(0.0f, 0.0f, 0.0f),
		m_worldRotationQuat(glm::vec3(0, 0, 0)),
		m_worldScale(1.0f, 1.0f, 1.0f),

		m_worldRight(WORLD_X),
		m_worldUp(WORLD_Y),
		m_worldForward(WORLD_Z),

		m_isDirty(true)
	{
		m_children.reserve(10);
	}


	mat4 const& Transform::GetWorldMatrix(TransformComponent component /*= WorldModel*/) const
	{
		switch (component)
		{
		case Translation:
			return m_worldTranslationMat;
			break;

		case Scale:
			return m_worldScaleMat;
			break;

		case Rotation:
			return m_worldRotationMat;
			break;

		case WorldModel:
		default:
			return m_worldMat;
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


	void Transform::TranslateModel(vec3 amount)
	{
		m_modelTranslationMat = glm::translate(m_modelTranslationMat, amount);
		
		// Extract the translation from the last column of the matrix:
		m_modelPosition = m_modelTranslationMat[3].xyz; // == (m_modelTranslationMat * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::SetModelPosition(vec3 position)
	{
		m_modelTranslationMat = glm::translate(mat4(1.0f), position);
		m_modelPosition = position;

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::RotateModel(vec3 eulerXYZRadians)
	{
		// Compute rotations via quaternions:
		m_modelRotationQuat = m_modelRotationQuat * glm::quat(eulerXYZRadians);
		m_modelRotationMat = glm::mat4_cast(m_modelRotationQuat);

		RecomputeEulerXYZRadians();

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::RotateModel(float angleRads, vec3 axis)
	{
		m_modelRotationQuat = glm::rotate(m_modelRotationQuat, angleRads, axis);
		m_modelRotationMat = glm::mat4_cast(m_modelRotationQuat);

		RecomputeEulerXYZRadians();

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::SetModelRotation(vec3 eulerXYZ)
	{
		// Compute rotations via quaternions:
		m_modelRotationQuat = glm::quat(eulerXYZ);
		m_modelRotationMat = glm::mat4_cast(m_modelRotationQuat);

		RecomputeEulerXYZRadians();

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::SetModelRotation(quat newRotation)
	{
		m_modelRotationQuat = newRotation;
		m_modelRotationMat = glm::mat4_cast(newRotation);

		RecomputeEulerXYZRadians();

		MarkDirty();
		RecomputeWorldTransforms();
	}


	void Transform::SetModelScale(vec3 scale)
	{
		m_modelScale = scale;
		m_modelScaleMat = glm::scale(mat4(1.0f), scale);

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

		m_modelMat = m_modelTranslationMat * m_modelScaleMat * m_modelRotationMat;

		// Update the combined world-space transformations with respect to the parent hierarchy:
		if (m_parent != nullptr)
		{
			m_worldMat				= m_parent->GetWorldMatrix(WorldModel) * m_modelMat;
			m_worldScaleMat			= m_parent->GetWorldMatrix(Scale) * m_modelScaleMat;
			m_worldRotationMat		= m_parent->GetWorldMatrix(Rotation) * m_modelRotationMat;
			m_worldTranslationMat	= m_parent->GetWorldMatrix(Translation) * m_modelTranslationMat;
		}
		else
		{
			m_worldMat				= m_modelMat;
			m_worldScaleMat			= m_modelScaleMat;
			m_worldRotationMat		= m_modelRotationMat;
			m_worldTranslationMat	= m_modelTranslationMat;
		}

		// Decompose our world matrix & update the individual components:
		vec3 skew;
		vec4 perspective;
		decompose(m_worldMat, m_worldScale, m_worldRotationQuat, m_worldPosition, skew, perspective);
		m_worldRotationEulerRadians = glm::eulerAngles(m_worldRotationQuat);

		// RecomputeWorldTransforms (normalized) world-space Right/Up/Forward CS axis vectors by applying m_worldRotationMat
		UpdateWorldSpaceAxis();

		for (int i = 0; i < (int)m_children.size(); i++)
		{
			m_children.at(i)->MarkDirty(); // Propagates down the entire hierarchy
			m_children.at(i)->RecomputeWorldTransforms();
		}
	}


	void Transform::UpdateWorldSpaceAxis()
	{
		// TODO: Optimize: Only need a 3x3 here?!?!?!

		// Update the world-space orientation of our local CS axis:
		m_worldRight	= normalize((m_worldRotationMat * vec4(WORLD_X, 0)).xyz());
		m_worldUp		= normalize((m_worldRotationMat * vec4(WORLD_Y, 0)).xyz());
		m_worldForward	= normalize((m_worldRotationMat * vec4(WORLD_Z, 0)).xyz());
	}


	void Transform::RecomputeEulerXYZRadians() // Should be anytime rotation has been modified
	{
		// Update our Euler rotation tracker:
		m_modelRotationEulerRadians = glm::eulerAngles(m_modelRotationQuat);

		// Bound the Euler radians to (-2pi, 2pi):
		m_modelRotationEulerRadians.x = 
			fmod<float>(abs(m_modelRotationEulerRadians.x), two_pi<float>()) * sign(m_modelRotationEulerRadians.x);
		m_modelRotationEulerRadians.y = 
			fmod<float>(abs(m_modelRotationEulerRadians.y), two_pi<float>()) * sign(m_modelRotationEulerRadians.y);
		m_modelRotationEulerRadians.z = 
			fmod<float>(abs(m_modelRotationEulerRadians.z), two_pi<float>()) * sign(m_modelRotationEulerRadians.z);
	}
}

