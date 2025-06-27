// � 2023 Adam Badke. All rights reserved.
#pragma once


namespace util
{
	// Convenience function to use an object's this pointer as a "unique" ID
	inline uint64_t PtrToID(void const* ptr)
	{
		return reinterpret_cast<std::size_t>(ptr);
	}


	inline void DisplayMat4x4(char const* label, glm::mat4 const& matrix)
	{	
		if (ImGui::TreeNode(label))
		{
			if (ImGui::BeginTable("table1", 4, ImGuiTableFlags_SizingFixedFit))
			{
				// GLM matrices are stored column-major order. 
				// We (currently) print the matrix here in the same horizontal layout it would have if we called 
				// glm::to_string(matrix)), but with nice table formatting that matches what we see in RenderDoc
				for (int row = 0; row < 4; row++)
				{
					ImGui::TableNextRow();
					for (int column = 0; column < 4; column++)
					{
						ImGui::TableNextColumn();
						ImGui::Text("%f", matrix[column][row]);						
					}
				}
				ImGui::EndTable();
			}
			ImGui::TreePop();
		}
	}


	inline void ShowErrorPopup(char const* title, char const* message, bool& doShow)
	{
		ImGui::OpenPopup(title);

		// Center the popup:
		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(title, &doShow, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text(message);

			if (ImGui::Button("OK", ImVec2(120, 0)))
			{
				doShow = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}


	template<typename T>
	bool ShowBasicComboBox(char const* title, std::span<const char* const> options, T& curSelection)
	{
		constexpr ImGuiComboFlags k_comboFlags = 0;

		size_t curSelectionIdx = static_cast<size_t>(curSelection);
		bool didSelect = false;

		SEAssert(curSelectionIdx < options.size(), "Current selection index out of bounds");

		if (ImGui::BeginCombo(title, options[curSelectionIdx], k_comboFlags))
		{
			for (size_t comboIdx = 0; comboIdx < options.size(); comboIdx++)
			{
				const bool isSelected = comboIdx == curSelectionIdx;
				if (ImGui::Selectable(options[comboIdx], isSelected))
				{
					curSelectionIdx = comboIdx;
					didSelect = true;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		curSelection = static_cast<T>(curSelectionIdx);
		return didSelect;
	}

	// Legacy overload for compatibility
	template<typename T>
	bool ShowBasicComboBox(char const* title, char const* const* options, size_t numOptions, T& curSelection)
	{
		return ShowBasicComboBox(title, std::span<const char* const>{options, numOptions}, curSelection);
	}


	template<typename T>
	bool ShowBasicComboBox(char const* title, std::span<const std::string> options, T& curSelection)
	{
		constexpr ImGuiComboFlags k_comboFlags = 0;

		size_t curSelectionIdx = static_cast<size_t>(curSelection);
		bool didSelect = false;

		SEAssert(curSelectionIdx < options.size(), "Current selection index out of bounds");

		if (ImGui::BeginCombo(title, options[curSelectionIdx].c_str(), k_comboFlags))
		{
			for (size_t comboIdx = 0; comboIdx < options.size(); comboIdx++)
			{
				const bool isSelected = comboIdx == curSelectionIdx;
				if (ImGui::Selectable(options[comboIdx].c_str(), isSelected))
				{
					curSelectionIdx = comboIdx;
					didSelect = true;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		curSelection = static_cast<T>(curSelectionIdx);
		return didSelect;
	}

	// Legacy overload for compatibility
	template<typename T>
	bool ShowBasicComboBox(char const* title, std::string const* options, size_t numOptions, T& curSelection)
	{
		return ShowBasicComboBox(title, std::span<const std::string>{options, numOptions}, curSelection);
	}
}