// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "ImGuiUtils.h"
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


namespace
{
	void ClampEulerRotationsToPlusMinus2Pi(glm::vec3& eulerXYZRadians)
	{
		eulerXYZRadians.x = fmod<float>(abs(eulerXYZRadians.x), two_pi<float>()) * sign(eulerXYZRadians.x);
		eulerXYZRadians.y = fmod<float>(abs(eulerXYZRadians.y), two_pi<float>()) * sign(eulerXYZRadians.y);
		eulerXYZRadians.z = fmod<float>(abs(eulerXYZRadians.z), two_pi<float>()) * sign(eulerXYZRadians.z);
	}
}

namespace fr
{
	// Static members:
	//----------------
	constexpr glm::vec3 Transform::WorldAxisX = vec3(1.0f,	0.0f,	0.0f);
	constexpr glm::vec3 Transform::WorldAxisY = vec3(0.0f,	1.0f,	0.0f);
	constexpr glm::vec3 Transform::WorldAxisZ = vec3(0.0f,	0.0f,	1.0f); // Note: SaberEngine uses a RHCS

	
	Transform::Transform(Transform* parent)
		: m_parent(nullptr)
		
		, m_localPosition(0.0f, 0.0f, 0.0f)
		, m_localRotationQuat(glm::vec3(0, 0, 0))
		, m_localScale(1.0f, 1.0f, 1.0f)
		
		, m_localMat(1.0f)

		, m_isDirty(true)
		, m_hasChanged(true)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		m_children.reserve(10);

		SetParent(parent);
		Recompute();
	}


	mat4 Transform::GetGlobalMatrix()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		Recompute();

		if (m_parent)
		{
			return m_parent->GetGlobalMatrix() * m_localMat;
		}
		else
		{
			return m_localMat;
		}
	}


	glm::vec3 Transform::GetGlobalEulerXYZRotationRadians() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		
		glm::vec3 eulerRadians;
		if (m_parent)
		{
			const glm::quat globalRotationQuat = m_localRotationQuat * m_parent->GetLocalRotation();
			eulerRadians = glm::eulerAngles(globalRotationQuat);
		}
		else
		{
			eulerRadians = glm::eulerAngles(m_localRotationQuat);			
		}

		ClampEulerRotationsToPlusMinus2Pi(eulerRadians);
		return eulerRadians;
	}


	glm::vec3 Transform::GetGlobalForward() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		const glm::quat globalRotationQuat = GetGlobalRotation();

		return normalize(globalRotationQuat * WorldAxisZ);
	}


	glm::vec3 Transform::GetGlobalRight() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		const glm::quat globalRotationQuat = GetGlobalRotation();

		return glm::normalize(globalRotationQuat * WorldAxisX);
	}


	glm::vec3 Transform::GetGlobalUp() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		const glm::quat globalRotationQuat = GetGlobalRotation();		

		return normalize(globalRotationQuat * WorldAxisY);
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
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		SEAssert("New parent cannot be null", newParent != nullptr);

		Recompute();
		SEAssert("Transformation should not be dirty", !m_isDirty);

		// Based on the technique presented in GPU Pro 360, Ch.15.2.5: 
		// Managing Transformations in Hierarchy: Parent Switch in Hierarchy (p.243 - p.253).
		// To move from current local space to a new local space where the parent changes but the global transformation
		// stays the same, we first find the current global transform by going up in the hierarchy to the root. Then,
		// we move down the hierarchy to the new parent:
		mat4 const& newLocalMatrix = glm::inverse(newParent->GetGlobalMatrix()) * GetGlobalMatrix();

		// Decompose our new matrix & update the individual components for when we call Recompute():
		vec3 skew;
		vec4 perspective;
		decompose(newLocalMatrix, m_localScale, m_localRotationQuat, m_localPosition, skew, perspective);

		SetParent(newParent);

		MarkDirty();
	}


	void Transform::TranslateLocal(vec3 const& amount)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
			
		m_localPosition += amount;

		MarkDirty();
	}


	void Transform::SetLocalPosition(vec3 const& position)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localPosition = position;

		MarkDirty();
	}


	glm::vec3 Transform::GetLocalPosition() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		return m_localPosition;
	}


	glm::mat4 Transform::GetLocalTranslationMat() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		return glm::translate(glm::mat4(1.0f), m_localPosition);
	}


	void Transform::SetGlobalPosition(glm::vec3 position)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		if (m_parent)
		{
			const glm::mat4 parentGlobalTRS = m_parent->GetGlobalMatrix();
			SetLocalPosition(glm::inverse(parentGlobalTRS) * glm::vec4(position, 0.f));
		}
		else
		{
			SetLocalPosition(glm::vec4(position, 0.f));
		}

		Recompute(); // Note: Already marked dirty when we called SetLocalPosition
	}


	glm::vec3 Transform::GetGlobalPosition()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		glm::mat4 const& globalMatrix = GetGlobalMatrix();

		return globalMatrix[3].xyz;
	}


	glm::mat4 Transform::GetGlobalTranslationMat() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		if (m_parent)
		{
			return m_parent->GetGlobalTranslationMat() * GetLocalTranslationMat();
		}
		else
		{
			return GetLocalTranslationMat();
		}
	}


	void Transform::RotateLocal(vec3 eulerXYZRadians)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		// Compute rotations via quaternions:
		m_localRotationQuat = m_localRotationQuat * glm::quat(eulerXYZRadians);

		MarkDirty();
	}


	void Transform::RotateLocal(float angleRads, vec3 axis)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localRotationQuat = glm::rotate(m_localRotationQuat, angleRads, axis);

		MarkDirty();
	}


	void Transform::RotateLocal(glm::quat const& rotation)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		SEAssertF("TODO: Test this. If you hit this assert, it's the first time this function has been used");

		m_localRotationQuat = rotation * m_localRotationQuat;

		MarkDirty();
	}


	void Transform::SetLocalRotation(vec3 const& eulerXYZ)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localRotationQuat = glm::quat(eulerXYZ); // Compute rotations via quaternions:

		MarkDirty();
	}


	void Transform::SetLocalRotation(quat const& newRotation)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localRotationQuat = newRotation;

		MarkDirty();
	}


	glm::quat Transform::GetLocalRotation() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		SEAssert("Transformation should not be dirty", !m_isDirty);

		return m_localRotationQuat;
	}


	glm::mat4 Transform::GetLocalRotationMat() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		SEAssert("Transformation should not be dirty", !m_isDirty);

		return glm::mat4_cast(m_localRotationQuat);
	}


	glm::quat Transform::GetGlobalRotation() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		if (m_parent)
		{
			// Apply our local rotation first
			return m_parent->GetGlobalRotation() * m_localRotationQuat;
		}
		else
		{
			return m_localRotationQuat;
		}
	}


	glm::mat4 Transform::GetGlobalRotationMat() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		glm::quat const& globalRotationQuat = GetGlobalRotation();
		return glm::mat4_cast(globalRotationQuat);
	}


	glm::vec3 Transform::GetLocalEulerXYZRotationRadians() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		glm::vec3 localRotationEulerRadians = glm::eulerAngles(m_localRotationQuat);
		ClampEulerRotationsToPlusMinus2Pi(localRotationEulerRadians);

		return localRotationEulerRadians;
	}


	void Transform::SetLocalScale(vec3 const& scale)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localScale = scale;

		MarkDirty();
	}


	glm::vec3 Transform::GetLocalScale() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		return m_localScale;
	}


	glm::mat4 Transform::GetLocalScaleMat() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		return glm::scale(mat4(1.f), m_localScale);
	}


	glm::vec3 Transform::GetGlobalScale() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		if (m_parent)
		{
			return m_parent->GetGlobalScale() * m_localScale;
		}
		else
		{
			return m_localScale;
		}
	}


	glm::mat4 Transform::GetGlobalScaleMat() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		if (m_parent)
		{
			return m_parent->GetGlobalScaleMat() * GetLocalScaleMat();
		}
		else
		{
			return GetLocalScaleMat();
		}
	}


	void Transform::MarkDirty()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		m_isDirty = true;
		m_hasChanged = true;
	}


	bool Transform::IsDirty()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		const bool isDirty = m_isDirty || (m_parent != nullptr && m_parent->IsDirty());
		return isDirty;
	}


	void Transform::RegisterChild(Transform* child)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		SEAssert("Cannot register a Transform to itself", child != this);
		SEAssert("Child must update their parent pointer", child->m_parent == this);
		SEAssert("Child is already registered",
			find(m_children.begin(), m_children.end(), child) == m_children.end());

		m_children.push_back(child);
	}


	void Transform::UnregisterChild(Transform const* child)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		SEAssert("Cannot unregister a Transform from itself", child != this);
		
		for (size_t i = 0; i < m_children.size(); i++)
		{
			if (m_children.at(i) == child)
			{
				m_children.erase(m_children.begin() + i); // Erase the ith element
				break;
			}
		}
	}

	
	void Transform::Recompute()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		if (!IsDirty())
		{
			return;
		}
		m_isDirty = false;
		m_hasChanged = true;

		m_localMat = GetLocalTranslationMat() * GetLocalRotationMat() * GetLocalScaleMat();
	}


	void Transform::ShowImGuiWindow(bool markAsParent /*= false*/, uint32_t depth /*= 0*/)
	{
		// Helper: Displays sliders for a 3-component XYZ element of a transform
		auto Display3ComponentTransform = [&](std::string const& label, glm::vec3& copiedValue) -> bool
		{
			constexpr ImGuiSliderFlags flags = ImGuiSliderFlags_None;
			constexpr float k_buttonWidth = 75.f;

			bool isDirty = false;
			ImGui::BeginGroup();
			{
				// Unique imgui IDs for each slider
				const std::string XimguiID = std::format("##X{}{}", label, util::PtrToID(this));
				const std::string YimguiID = std::format("##Y{}{}", label, util::PtrToID(this));
				const std::string ZimguiID = std::format("##Z{}{}", label, util::PtrToID(this));

				ImGui::Text(std::format("{} XYZ:", label).c_str()); // Row label

				ImGui::SameLine(); ImGui::PushItemWidth(k_buttonWidth);
				isDirty |= ImGui::DragFloat(XimguiID.c_str(), &copiedValue.x, 0.005f, -FLT_MAX, +FLT_MAX, "X %.3f", flags);

				ImGui::SameLine(); ImGui::PushItemWidth(k_buttonWidth);
				isDirty |= ImGui::DragFloat(YimguiID.c_str(), &copiedValue.y, 0.005f, -FLT_MAX, +FLT_MAX, "Y %.3f", flags);

				ImGui::SameLine(); ImGui::PushItemWidth(k_buttonWidth);
				isDirty |= ImGui::DragFloat(ZimguiID.c_str(), &copiedValue.z, 0.005f, -FLT_MAX, +FLT_MAX, "Z %.3f", flags);

				ImGui::EndGroup();
			}
			return isDirty;
		};


		if (markAsParent && m_parent)
		{
			m_parent->ShowImGuiWindow(true, depth + 1);
		}
		
		if (markAsParent && !m_parent && !ImGui::CollapsingHeader(std::format("Root parent: (node {})##{}", depth, util::PtrToID(this)).c_str()))
		{
			return;
		}
		else if (markAsParent && m_parent && !ImGui::CollapsingHeader(std::format("Parent: (node {})##{}", depth, util::PtrToID(this)).c_str()))
		{
			return;
		}
		else if (!markAsParent && 
			m_parent && 
			ImGui::CollapsingHeader(std::format("Parent hierarchy##{}", depth, util::PtrToID(this)).c_str()))
		{
			m_parent->ShowImGuiWindow(true, depth + 1);
		}

		if(markAsParent || ImGui::CollapsingHeader(std::format("(node {})##{}", depth, util::PtrToID(this)).c_str()))
		{
			ImGui::Indent();

			// View Transform data:
			if (ImGui::CollapsingHeader(std::format("Show data##{}", util::PtrToID(this)).c_str()))
			{
				ImGui::Indent();
				ImGui::Text(std::format("Local Position: {}", glm::to_string(m_localPosition)).c_str());
				ImGui::Text(std::format("Local Quaternion: {}", glm::to_string(m_localRotationQuat)).c_str());
				ImGui::Text(std::format("Local Euler XYZ Radians: {}", glm::to_string(GetLocalEulerXYZRotationRadians())).c_str());
				ImGui::Text(std::format("Local Scale: {}", glm::to_string(m_localScale)).c_str());
				util::DisplayMat4x4(std::format("Local Matrix:##{}", util::PtrToID(this)).c_str(), m_localMat);
				util::DisplayMat4x4(std::format("Global Matrix:##{}", util::PtrToID(this)).c_str(), GetGlobalMatrix());
				ImGui::Unindent();
			}
			

			// Modification controls:
			if (ImGui::CollapsingHeader(std::format("Modify##{}", util::PtrToID(this)).c_str()))
			{
				// Dragable local translation:
				glm::vec3 localPosition = GetLocalPosition();
				bool localTranslationDirty = Display3ComponentTransform("Local Translation", localPosition);
				if (localTranslationDirty)
				{
					SetLocalPosition(localPosition);
				}

				// Clickable local translation
				static glm::vec3 s_translationAmt = glm::vec3(0.f);
				if (ImGui::Button(std::format("[-]##{}", util::PtrToID(this)).c_str()))
				{
					TranslateLocal(-s_translationAmt);
				}
				ImGui::SameLine();
				if (ImGui::Button(std::format("[+]##{}", util::PtrToID(this)).c_str()))
				{
					TranslateLocal(s_translationAmt);
				}
				ImGui::SameLine();
				ImGui::PushItemWidth(130.f);
				ImGui::DragFloat3(std::format("##{}", util::PtrToID(this)).c_str(), &s_translationAmt.x, 0.001f, -FLT_MAX, FLT_MAX);
				ImGui::PopItemWidth();

				// Local rotation:
				glm::vec3 localEulerRotation = GetLocalEulerXYZRotationRadians();
				bool localRotationDirty = Display3ComponentTransform("Local Euler Rotation", localEulerRotation);
				if (localRotationDirty)
				{
					SetLocalRotation(localEulerRotation);
				}

				// Local scale:
				static bool s_uniformScale = false;
				ImGui::Checkbox(std::format("Uniform scale##{}", util::PtrToID(this)).c_str(), &s_uniformScale);

				glm::vec3 localScale = GetLocalScale();
				if (s_uniformScale)
				{
					static float s_uniformScaleAmount = 1.f;

					ImGui::PushItemWidth(130.f);
					if (ImGui::SliderFloat("Scale", &s_uniformScaleAmount, 0.f, 10.f))
					{
						SetLocalScale(glm::vec3(s_uniformScaleAmount));
					}
					ImGui::PopItemWidth();
				}
				else if (Display3ComponentTransform("Local Scale", localScale))
				{
					SetLocalScale(localScale);
				}

				// Global translation:
				glm::vec3 globalTranslation = GetGlobalPosition();
				bool globalTranslationDirty = Display3ComponentTransform("Global Translation", globalTranslation);
				if (globalTranslationDirty)
				{
					SetGlobalPosition(globalTranslation);
				}
			}

			
			ImGui::Unindent();
			
		}
	}
}

