// © 2023 Adam Badke. All rights reserved.
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
}