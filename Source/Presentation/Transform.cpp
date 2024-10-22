// © 2022 Adam Badke. All rights reserved.
#include "Transform.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/Util/ImGuiUtils.h"

#include "Renderer/TransformRenderData.h"


namespace
{
	void ClampEulerRotationsToPlusMinus2Pi(glm::vec3& eulerXYZRadians)
	{
		eulerXYZRadians.x = glm::fmod<float>(abs(eulerXYZRadians.x), glm::two_pi<float>()) * glm::sign(eulerXYZRadians.x);
		eulerXYZRadians.y = glm::fmod<float>(abs(eulerXYZRadians.y), glm::two_pi<float>()) * glm::sign(eulerXYZRadians.y);
		eulerXYZRadians.z = glm::fmod<float>(abs(eulerXYZRadians.z), glm::two_pi<float>()) * glm::sign(eulerXYZRadians.z);
	}
}

namespace fr
{
	// gr::k_sharedIdentityTransformID == 0, so we start at 1
	std::atomic<gr::TransformID> Transform::s_transformIDs = gr::k_sharedIdentityTransformID + 1;


	Transform::Transform(Transform* parent)
		: m_parent(nullptr)
		, m_transformID(s_transformIDs.fetch_add(1))
		
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


	Transform::~Transform()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		if (m_parent)
		{
			m_parent->UnregisterChild(this);
		}
		if (!m_children.empty())
		{
			for (fr::Transform* child : m_children)
			{
				child->SetParent(nullptr);
			}
		}
	}


	Transform::Transform(Transform&& rhs) noexcept
		: m_parent(nullptr)
		, m_transformID(rhs.m_transformID)
	{
		std::scoped_lock lock(m_transformMutex, rhs.m_transformMutex);

		Transform* rhsParent = rhs.m_parent;
		rhs.SetParent(nullptr);
		SetParent(rhsParent);

		for (Transform* rhsChild : rhs.m_children)
		{
			rhsChild->SetParent(this);
		}

		m_localPosition = rhs.m_localPosition;
		m_localRotationQuat = rhs.m_localRotationQuat;
		m_localScale = rhs.m_localScale;

		m_localMat = rhs.m_localMat;

		m_isDirty = true;
		m_hasChanged = true;
	}


	glm::mat4 Transform::GetGlobalMatrix()
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


	glm::mat4 Transform::GetGlobalMatrix() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		SEAssert(!m_isDirty, "Transform is dirty");

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

		return normalize(globalRotationQuat * gr::Transform::WorldAxisZ);
	}


	glm::vec3 Transform::GetGlobalRight() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		const glm::quat globalRotationQuat = GetGlobalRotation();

		return glm::normalize(globalRotationQuat * gr::Transform::WorldAxisX);
	}


	glm::vec3 Transform::GetGlobalUp() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		const glm::quat globalRotationQuat = GetGlobalRotation();		

		return normalize(globalRotationQuat * gr::Transform::WorldAxisY);
	}


	Transform* Transform::GetParent() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		return m_parent; 
	}


	void Transform::SetParent(Transform* newParent)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		SEAssert(newParent != this, "Cannot parent a Transform to itself");

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

		Recompute();
		SEAssert(!m_isDirty, "Transformation should not be dirty");

		// Based on the technique presented in GPU Pro 360, Ch.15.2.5: 
		// Managing Transformations in Hierarchy: Parent Switch in Hierarchy (p.243 - p.253).
		// To move from current local space to a new local space where the parent changes but the global transformation
		// stays the same, we first find the current global transform by going up in the hierarchy to the root. Then,
		// we move down the hierarchy to the new parent.
		// If newParent == nullptr, we effectively move the current local Transform to assume the global values (so the
		// objects that have their parent removed stay in the same final location)

		glm::mat4 const& newLocalMatrix = 
			newParent == nullptr ? GetGlobalMatrix() : (glm::inverse(newParent->GetGlobalMatrix()) * GetGlobalMatrix());

		// Decompose our new matrix & update the individual components for when we call Recompute():
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(newLocalMatrix, m_localScale, m_localRotationQuat, m_localPosition, skew, perspective);

		SetParent(newParent);

		MarkDirty();
	}


	void Transform::TranslateLocal(glm::vec3 const& amount)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
			
		m_localPosition += amount;

		MarkDirty();
	}


	void Transform::SetLocalPosition(glm::vec3 const& position)
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
			glm::mat4 const& parentGlobalTRS = m_parent->GetGlobalMatrix();
			SetLocalPosition(glm::inverse(parentGlobalTRS) * glm::vec4(position, 1.f));
		}
		else
		{
			SetLocalPosition(position);
		}

		Recompute(); // Note: Already marked dirty when we called SetLocalPosition
	}


	glm::vec3 Transform::GetGlobalPosition()
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		glm::mat4 const& globalMatrix = GetGlobalMatrix();

		return globalMatrix[3].xyz;
	}


	glm::vec3 Transform::GetGlobalPosition() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		SEAssert(!m_isDirty, "Camera is dirty");

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


	void Transform::RotateLocal(glm::vec3 eulerXYZRadians)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		// Compute rotations via quaternions:
		m_localRotationQuat = m_localRotationQuat * glm::quat(eulerXYZRadians);

		MarkDirty();
	}


	void Transform::RotateLocal(float angleRads, glm::vec3 axis)
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


	void Transform::SetLocalRotation(glm::vec3 const& eulerXYZ)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localRotationQuat = glm::quat(eulerXYZ); // Compute rotations via quaternions:

		MarkDirty();
	}


	void Transform::SetLocalRotation(glm::quat const& newRotation)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		m_localRotationQuat = newRotation;

		MarkDirty();
	}


	glm::quat Transform::GetLocalRotation() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		SEAssert(!m_isDirty, "Transformation should not be dirty");

		return m_localRotationQuat;
	}


	glm::mat4 Transform::GetLocalRotationMat() const
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		SEAssert(!m_isDirty, "Transformation should not be dirty");

		return glm::mat4_cast(m_localRotationQuat);
	}


	void Transform::SetGlobalRotation(glm::quat const& rotation)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		if (m_parent)
		{
			glm::quat const& parentGlobalRotation = m_parent->GetGlobalRotation();
			SetLocalRotation(glm::inverse(parentGlobalRotation) * rotation);
		}
		else
		{
			SetLocalRotation(rotation);
		}

		Recompute(); // Note: Already marked dirty when we called SetLocalPosition
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


	void Transform::SetLocalScale(glm::vec3 const& scale)
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
		return glm::scale(glm::mat4(1.f), m_localScale);
	}


	void Transform::SetGlobalScale(glm::vec3 const& scale)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);

		if (m_parent)
		{
			glm::vec3 const& parentGlobalScale = m_parent->GetGlobalScale();
			SetLocalScale(scale / parentGlobalScale);
		}
		else
		{
			SetLocalScale(scale);
		}

		Recompute(); // Note: Already marked dirty when we called SetLocalPosition
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
		SEAssert(child != this, "Cannot register a Transform to itself");
		SEAssert(child->m_parent == this, "Child must update their parent pointer");
		SEAssert(find(m_children.begin(), m_children.end(), child) == m_children.end(),
			"Child is already registered");

		m_children.push_back(child);
	}


	void Transform::UnregisterChild(Transform const* child)
	{
		std::unique_lock<std::recursive_mutex> lock(m_transformMutex);
		SEAssert(child != this, "Cannot unregister a Transform from itself");
		
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


	void Transform::ImGuiHelper_ShowData(uint64_t uniqueID)
	{
		if (ImGui::CollapsingHeader(std::format("View data##{}", uniqueID).c_str()))
		{
			ImGui::Indent();

			ImGui::Text(std::format("Local Position: {}", glm::to_string(m_localPosition)).c_str());
			ImGui::Text(std::format("Local Quaternion: {}", glm::to_string(m_localRotationQuat)).c_str());
			ImGui::Text(std::format("Local Euler XYZ Radians: {}",
				glm::to_string(GetLocalEulerXYZRotationRadians())).c_str());
			ImGui::Text(std::format("Local Scale: {}", glm::to_string(m_localScale)).c_str());
			
			util::DisplayMat4x4(std::format("Local Matrix:##{}", uniqueID).c_str(), m_localMat);

			ImGui::Separator();

			ImGui::Text(std::format("Global Position: {}", glm::to_string(GetGlobalPosition())).c_str());
			ImGui::Text(std::format("Global Quaternion: {}", glm::to_string(GetGlobalRotation())).c_str());
			ImGui::Text(std::format("Global Euler XYZ Radians: {}", glm::to_string(GetGlobalEulerXYZRotationRadians())).c_str());
			ImGui::Text(std::format("Global Scale: {}", glm::to_string(GetGlobalScale())).c_str());

			util::DisplayMat4x4(std::format("Global Matrix:##{}", uniqueID).c_str(), GetGlobalMatrix());

			ImGui::Separator();

			if (ImGui::TreeNode(std::format("Global Axis##{}", uniqueID).c_str()))
			{
				ImGui::Text(std::format("Global Right (X): {}", glm::to_string(GetGlobalRight())).c_str());
				ImGui::Text(std::format("Global Up (Y): {}", glm::to_string(GetGlobalUp())).c_str());
				ImGui::Text(std::format("Global Forward (Z): {}", glm::to_string(GetGlobalForward())).c_str());
			}

			ImGui::Unindent();
		}
	}


	void Transform::ImGuiHelper_Modify(uint64_t uniqueID)
	{
		// Helper: Displays sliders for a 3-component XYZ element of a transform
		auto Display3ComponentTransform = [&](std::string const& label, glm::vec3& value) -> bool
			{
				constexpr ImGuiSliderFlags flags = ImGuiSliderFlags_None;
				constexpr float k_buttonWidth = 75.f;

				bool isDirty = false;
				ImGui::BeginGroup();
				{
					// Unique imgui IDs for each slider
					std::string const& XimguiID = std::format("##X{}{}", label, uniqueID);
					std::string const& YimguiID = std::format("##Y{}{}", label, uniqueID);
					std::string const& ZimguiID = std::format("##Z{}{}", label, uniqueID);

					ImGui::Text(std::format("{} XYZ:", label).c_str()); // Row label

					ImGui::SameLine(); ImGui::PushItemWidth(k_buttonWidth);
					isDirty |= ImGui::DragFloat(
						XimguiID.c_str(), &value.x, 0.005f, -FLT_MAX, +FLT_MAX, "X %.3f", flags);

					ImGui::SameLine(); ImGui::PushItemWidth(k_buttonWidth);
					isDirty |= ImGui::DragFloat(
						YimguiID.c_str(), &value.y, 0.005f, -FLT_MAX, +FLT_MAX, "Y %.3f", flags);

					ImGui::SameLine(); ImGui::PushItemWidth(k_buttonWidth);
					isDirty |= ImGui::DragFloat(
						ZimguiID.c_str(), &value.z, 0.005f, -FLT_MAX, +FLT_MAX, "Z %.3f", flags);

					ImGui::EndGroup();
				}
				return isDirty;
			};


		if (ImGui::CollapsingHeader(std::format("Modify##{}", uniqueID).c_str()))
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
			if (ImGui::Button(std::format("[-]##{}", uniqueID).c_str()))
			{
				TranslateLocal(-s_translationAmt);
			}
			ImGui::SameLine();
			if (ImGui::Button(std::format("[+]##{}", uniqueID).c_str()))
			{
				TranslateLocal(s_translationAmt);
			}
			ImGui::SameLine();
			ImGui::PushItemWidth(130.f);
			ImGui::DragFloat3(
				std::format("##{}", uniqueID).c_str(), &s_translationAmt.x, 0.001f, -FLT_MAX, FLT_MAX);
			ImGui::PopItemWidth();

			// Local rotation:
			// Note: quaterion rotations are defined in terms of +/- pi/2, glm will wrap the values. This doesn't play
			// nicely with ImGui, which gets confused if a slider value is suddenly wrapped. As a solution, we maintain
			// the value in a local static, and rely on the internal setter to wrap the value
			static glm::vec3 s_localEulerRotation = GetLocalEulerXYZRotationRadians();
			bool localRotationDirty = Display3ComponentTransform("Local Euler Rotation", s_localEulerRotation);
			if (localRotationDirty)
			{
				SetLocalRotation(s_localEulerRotation);
			}

			// Local scale:
			static bool s_uniformScale = false;
			ImGui::Checkbox(std::format("Uniform scale##{}", uniqueID).c_str(), &s_uniformScale);

			glm::vec3 localScale = GetLocalScale();
			if (s_uniformScale)
			{
				static float s_uniformScaleAmount = 1.f;

				ImGui::PushItemWidth(130.f);
				if (ImGui::SliderFloat(std::format("Scale##{}", uniqueID).c_str(), &s_uniformScaleAmount, 0.f, 10.f))
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
	}


	void Transform::ShowImGuiWindow()
	{
		ImGuiHelper_ShowHierarchy(this, true);
	}


	void Transform::ImGuiHelper_ShowHierarchy(
		fr::Transform* node, bool highlightCurrentNode, bool expandAllState, bool expandChangeTriggered)
	{
		constexpr float k_indentSize = 16.f;
		constexpr ImVec4 k_thisObjectMarkerTextCol = ImVec4(0, 1, 0, 1);
		constexpr char const* k_thisObjectText = "<this object>";

		struct NodeState
		{
			fr::Transform* m_node;
			uint32_t m_depth;
		};

		// Find the root node
		fr::Transform* rootNode = node;
		while (rootNode->m_parent != nullptr)
		{
			rootNode = rootNode->m_parent;
		}

		std::stack<NodeState> nodes;
		nodes.push({ rootNode, 1 }); // Depth is offset +1 so the indent value will be > 0

		while (!nodes.empty())
		{
			NodeState cur = nodes.top();
			nodes.pop();

			// Add children for next iteration:
			for (fr::Transform* child : cur.m_node->m_children)
			{
				nodes.push(NodeState{ child, cur.m_depth + 1 });
			}

			ImGui::Indent(k_indentSize * cur.m_depth);

			if (expandChangeTriggered)
			{
				ImGui::SetNextItemOpen(expandAllState);
			}
			if (ImGui::TreeNode(std::format("TransformID: {}", cur.m_node->m_transformID).c_str()))
			{
				if (highlightCurrentNode && cur.m_node == node)
				{
					ImGui::SameLine(); ImGui::TextColored(k_thisObjectMarkerTextCol, k_thisObjectText);
				}

				ImGui::Indent();

				// Show the current node info:
				ImGui::Text(std::format("{} Depth {}, {} {}",
					cur.m_node->m_parent ? "" : "Root:",
					cur.m_depth - 1,
					cur.m_node->m_children.size(),
					cur.m_node->m_children.size() == 1 ? "child" : "children").c_str());

				// View Transform data:
				cur.m_node->ImGuiHelper_ShowData(cur.m_node->m_transformID);

				// Modification controls:
				cur.m_node->ImGuiHelper_Modify(cur.m_node->m_transformID);

				ImGui::Unindent();

				ImGui::TreePop();
			}
			else if (highlightCurrentNode && cur.m_node == node)
			{
				ImGui::SameLine(); ImGui::TextColored(k_thisObjectMarkerTextCol, k_thisObjectText);
			}
			
			ImGui::Unindent(k_indentSize * cur.m_depth);
		}
	}


	void Transform::ShowImGuiWindow(std::vector<fr::Transform*> const& rootNodes, bool* show)
	{
		if (!(*show))
		{
			return;
		}

		static const int windowWidth = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			windowWidth * k_windowWidthPercentage,
			static_cast<float>(windowHeight) - k_windowYOffset),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Transform Hierarchy";
		ImGui::Begin(k_panelTitle, show);

		static bool s_expandAll = false;
		bool showHideAll = false;
		if (ImGui::Button(s_expandAll ? "Hide all" : "Expand all"))
		{
			s_expandAll = !s_expandAll;
			showHideAll = true;
		}

		// Show each root node in the panel
		for (fr::Transform* rootNode : rootNodes)
		{
			ImGuiHelper_ShowHierarchy(rootNode, false, s_expandAll, showHideAll);

			ImGui::Separator();
		}

		ImGui::End();
	}
}

