#include "DebugConfiguration.h"
#include "Transform.h"
#include <algorithm>

#include <glm/gtc/constants.hpp>

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

	vec3& Transform::RotateVector(vec3& targetVector, float const& radians, vec3 const& axis)
	{
		mat4 rotation = glm::rotate(mat4(1.0f), radians, axis);

		targetVector = (rotation * vec4(targetVector, 0.0f)).xyz();
		return targetVector;
	}

	/********************************/

	
	Transform::Transform() :
		m_parent(nullptr),
		m_worldPosition(0.0f, 0.0f, 0.0f),
		m_eulerWorldRotation(0.0f, 0.0f, 0.0f),
		m_worldScale(1.0f, 1.0f, 1.0f),
		m_right(WORLD_X),
		m_up(WORLD_Y),
		m_forward(WORLD_Z),
		m_model(1.0f),
		m_scale(1.0f),
		m_rotation(1.0f),
		m_translation(1.0f),
		m_combinedModel(1.0f),
		m_combinedScale(1.0f),
		m_combinedRotation(1.0f),
		m_combinedTranslation(1.0f),
		m_worldRotation(glm::vec3(0,0,0)),
		m_isDirty(true)
	{
		m_children.reserve(10);
	}


	mat4 Transform::Model(ModelMatrixComponent component /*= WorldModel*/) const
	{
		// Return the *combined* world transformations of the entire hierarchy
		switch (component)
		{
		case WorldTranslation:
			return m_combinedTranslation;
			break;

		case WorldScale:
			return m_combinedScale;
			break;

		case WorldRotation:
			return m_combinedRotation;
			break;

		case WorldModel:
		default:
			return m_combinedModel;
		}		
	}


	void Transform::SetParent(Transform* newParent)
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
		Recompute();
	}


	void Transform::Translate(vec3 amount)
	{
		m_translation	= glm::translate(m_translation, amount);
		
		// Extract the translation from the last column of the matrix:
		m_worldPosition = m_translation[3].xyz; // == (m_translation * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;

		MarkDirty();
		Recompute();
	}


	void Transform::SetWorldPosition(vec3 position)
	{
		m_translation	= glm::translate(mat4(1.0f), position);
		m_worldPosition = position;

		MarkDirty();
		Recompute();
	}


	vec3 const& Transform::GetWorldPosition() const
	{
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
		Recompute();
	}


	vec3 const&	Transform::GetEulerRotation() const
	{ 
		// TODO: Currently, this will be incorrect if you call GetEulerRotation() before SetWorldRotation() or Rotate()!!!!
		return m_eulerWorldRotation; 
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
		Recompute();
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
		Recompute();
	}


	void Transform::SetWorldScale(vec3 scale)
	{
		m_worldScale = scale;
		m_scale = glm::scale(mat4(1.0f), scale);

		MarkDirty();
		Recompute();
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

			MarkDirty();
			Recompute();
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
				Recompute();
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
		m_isDirty = false; // Must immediately remove our dirty flag due to recursive calls

		m_model = m_translation * m_scale * m_rotation;

		// Update the combined transformations, if we have a parent
		if (m_parent != nullptr)
		{
			m_combinedModel			= m_parent->Model(WorldModel) * m_model;
			m_combinedScale			= m_parent->Model(WorldScale) * m_scale;
			m_combinedRotation		= m_parent->Model(WorldRotation) * m_rotation;
			m_combinedTranslation	= m_parent->Model(WorldTranslation) * m_translation;
		}
		else
		{
			m_combinedModel			= m_model;
			m_combinedScale			= m_scale;
			m_combinedRotation		= m_rotation;
			m_combinedTranslation	= m_translation;
		}

		// Extract the translation from the last column of the matrix:
		m_worldPosition = m_combinedModel[3].xyz; // == (m_combinedModel * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
		
		// TODO: Recompute eulerWorldRotation
		//eulerWorldRotation = ???

		m_worldScale = (m_combinedScale * vec4(1, 1, 1, 1)).xyz;

		for (int i = 0; i < (int)m_children.size(); i++)
		{
			m_children.at(i)->MarkDirty();
			m_children.at(i)->Recompute();
		}
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
		m_eulerWorldRotation.x = fmod<float>(abs(m_eulerWorldRotation.x), two_pi<float>()) * sign(m_eulerWorldRotation.x);
		m_eulerWorldRotation.y = fmod<float>(abs(m_eulerWorldRotation.y), two_pi<float>()) * sign(m_eulerWorldRotation.y);
		m_eulerWorldRotation.z = fmod<float>(abs(m_eulerWorldRotation.z), two_pi<float>()) * sign(m_eulerWorldRotation.z);
	}
}

