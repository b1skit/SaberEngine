// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace util
{
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
}