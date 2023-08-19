// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace util
{
	void DisplayMat4x4(char const* label, glm::mat4x4 const& matrix)
	{
		if (ImGui::TreeNode(label))
		{
			if (ImGui::BeginTable("table1", 4))
			{
				for (int row = 0; row < 4; row++)
				{
					ImGui::TableNextRow();
					for (int column = 0; column < 4; column++)
					{
						ImGui::TableSetColumnIndex(column);
						ImGui::Text("%f", matrix[row][column]);
					}
				}
				ImGui::EndTable();
			}
			ImGui::TreePop();
		}
	}
}