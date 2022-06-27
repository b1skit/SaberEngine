#include "BuildConfiguration.h"
#include "Transform.h"
#include <algorithm>

#include <glm/gtc/constants.hpp>

#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/gtx/common.hpp>

using glm::normalize;
using glm::rotate;

using std::find;


namespace SaberEngine
{
	// Static members:
	//----------------
	const vec3 Transform::WORLD_X	= vec3(1.0f,	0.0f,	0.0f);
	const vec3 Transform::WORLD_Y	= vec3(0.0f,	1.0f,	0.0f);
	const vec3 Transform::WORLD_Z	= vec3(0.0f,	0.0f,	1.0f); // Note: SaberEngine always uses a RHCS


	// Constructor:
	Transform::Transform()
	{
		m_children.reserve(10);

		m_isDirty	= true;
	}


	mat4 Transform::Model(MODEL_MATRIX_COMPONENT component /*= WORLD_MODEL*/)
	{
		if (m_isDirty)
		{
			Recompute();
		}	

		// Return the *combined* world transformations of the entire hierarchy
		switch (component)
		{
		case WORLD_TRANSLATION:
			return m_combinedTranslation;
			break;

		case WORLD_SCALE:
			return m_combinedScale;
			break;

		case WORLD_ROTATION:
			return m_combinedRotation;
			break;

		case WORLD_MODEL:
		default:
			return m_combinedModel;
		}		
	}


	void Transform::Parent(Transform* newParent)
	{
		// Unparent:
		if (newParent == nullptr)
		{
			if (m_parent != nullptr)
			{
				m_parent->UnregisterChild(this);
				m_parent = nullptr;
			}
		}
		// Parent:
		else 
		{
			m_parent = newParent;
			m_parent->RegisterChild(this);
		}
		
		MarkDirty();
	}


	void Transform::Translate(vec3 amount)
	{
		m_translation	= glm::translate(m_translation, amount);
		
		vec4 result			= m_translation * vec4(0.0f, 0.0f, 0.0f, 1.0f); // TODO: Just extract the translation from the end column of the matrix???
		m_worldPosition	= result.xyz();

		MarkDirty();
	}


	void Transform::SetWorldPosition(vec3 position)
	{
		m_translation	= glm::translate(mat4(1.0f), position);
		m_worldPosition = position;

		MarkDirty();
	}


	vec3 const& Transform::WorldPosition()
	{
		if (m_isDirty)
		{
			Recompute();
		}

		return m_worldPosition;
	}


	void Transform::Rotate(vec3 eulerXYZ) // Note: eulerXYZ is in RADIANS
	{
		// Concatenate rotations as quaternions:
		m_worldRotation = m_worldRotation * glm::quat(eulerXYZ);

		m_rotation = glm::mat4_cast(m_worldRotation);

		// Update the world-space orientation of our local CS axis:
		UpdateLocalAxis();

		// Update the rotation value, and keep xyz bound in [0, 2pi]:
		m_eulerWorldRotation += eulerXYZ;
		BoundEulerAngles();

		MarkDirty();
	}


	vec3 const&	Transform::GetEulerRotation()
	{ 
		if (m_isDirty)
		{
			Recompute(); // TODO: Must actually compute the eulerRotation in here!!!
		}

		return m_eulerWorldRotation; // Currently, this will be incorrect if you call GetEulerRotation() before SetWorldRotation() or Rotate()!!!!
	} 


	void Transform::SetWorldRotation(vec3 eulerXYZ)
	{
		// Update Quaternion:
		m_worldRotation = glm::quat(eulerXYZ);

		m_rotation = glm::mat4_cast(m_worldRotation);

		UpdateLocalAxis();

		m_eulerWorldRotation = eulerXYZ;
		BoundEulerAngles();

		MarkDirty();
	}


	void Transform::SetWorldRotation(quat newRotation)
	{
		m_worldRotation = newRotation;

		m_rotation = glm::mat4_cast(newRotation);

		UpdateLocalAxis();

		// Update Euler angles:
		m_eulerWorldRotation = glm::eulerAngles(newRotation);
		BoundEulerAngles();

		MarkDirty();
	}


	void Transform::SetWorldScale(vec3 scale)
	{
		m_worldScale = scale;
		m_scale = glm::scale(mat4(1.0f), scale);

		MarkDirty();
	}


	void Transform::MarkDirty()
	{
		m_isDirty = true;

		for (int i = 0; i < (int)m_children.size(); i++)
		{
			m_children.at(i)->MarkDirty();
		}
	}


	void Transform::DebugPrint()
	{
		#if defined(DEBUG_TRANSFORMS)
			LOG("[TRANSFORM DEBUG]\n\tPostition = " + to_string(m_worldPosition.x) + ", " + to_string(m_worldPosition.y) + ", " + to_string(m_worldPosition.z));
			LOG("Euler rotation = " + to_string(m_eulerWorldRotation.x) + ", " + to_string(m_eulerWorldRotation.y) + ", " + to_string(m_eulerWorldRotation.z) + " (radians)");
			LOG("Scale = " + to_string(m_worldScale.x) + ", " + to_string(m_worldScale.y) + ", " + to_string(m_worldScale.z));
		#else
				return;
		#endif
	}


	// Static helper functions:
	//-------------------------

	vec3& Transform::RotateVector(vec3& targetVector, float const& radians, vec3 const& axis)
	{
		mat4 rotation = glm::rotate(mat4(1.0f), radians, axis);
		
		targetVector = (rotation * vec4(targetVector, 0.0f)).xyz();
		return targetVector;
	}


	// Protected functions:
	//---------------------

	void Transform::RegisterChild(Transform* child)
	{
		if (find(m_children.begin(), m_children.end(), child) ==  m_children.end())
		{
			m_children.push_back(child);

			MarkDirty();
		}
	}


	void Transform::UnregisterChild(Transform const* child)
	{
		for (unsigned int i = 0; i < child->m_children.size(); i++)
		{
			if (m_children.at(i) == child)
			{
				m_children.erase(m_children.begin() + i);
				MarkDirty();
				break;
			}
		}
	}

	
	void Transform::Recompute()
	{
		if (!m_isDirty)
		{
			return;
		}			

		m_model = m_translation * m_scale * m_rotation;

		m_combinedModel			= m_model;
		m_combinedScale			= m_scale;
		m_combinedRotation		= m_rotation;
		m_combinedTranslation	= m_translation;
		if (m_parent != nullptr)
		{
			m_combinedModel			= m_parent->Model(WORLD_MODEL) * m_combinedModel;
			m_combinedScale			= m_parent->Model(WORLD_SCALE) * m_combinedScale;
			m_combinedRotation		= m_parent->Model(WORLD_ROTATION) * m_combinedRotation;
			m_combinedTranslation	= m_parent->Model(WORLD_TRANSLATION) * m_combinedTranslation;
		}

		m_worldPosition = m_combinedModel * vec4(0, 0, 0, 1);
		
		// TODO: Recompute eulerWorldRotation
		//eulerWorldRotation = ???

		m_worldScale = m_combinedScale * vec4(1, 1, 1, 1);

		for (int i = 0; i < (int)m_children.size(); i++)
		{
			m_children.at(i)->MarkDirty();
		}

		m_isDirty = false;
	}


	void Transform::UpdateLocalAxis()
	{
		// Update the world-space orientation of our local CS axis:
		m_right		= normalize((m_rotation * vec4(WORLD_X, 0)).xyz());
		m_up		= normalize((m_rotation * vec4(WORLD_Y, 0)).xyz());
		m_forward	= normalize((m_rotation * vec4(WORLD_Z, 0)).xyz());
	}


	void Transform::BoundEulerAngles()
	{
		// Keep (signed) Euler xyz angles in (-2pi, 2pi):
		m_eulerWorldRotation.x = glm::fmod<float>(glm::abs(m_eulerWorldRotation.x), glm::two_pi<float>()) * glm::sign(m_eulerWorldRotation.x);
		m_eulerWorldRotation.y = glm::fmod<float>(glm::abs(m_eulerWorldRotation.y), glm::two_pi<float>()) * glm::sign(m_eulerWorldRotation.y);
		m_eulerWorldRotation.z = glm::fmod<float>(glm::abs(m_eulerWorldRotation.z), glm::two_pi<float>()) * glm::sign(m_eulerWorldRotation.z);
	}
}

