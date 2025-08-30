// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"


namespace grutil
{
	inline uint32_t GetRoundedDispatchDimension(uint32_t totalDimension, uint32_t workGroupDimension)
	{
		return (totalDimension + (workGroupDimension - 1)) / workGroupDimension;
	}


	inline float LinearToLuminance(glm::vec3 const& linearColor)
	{
		// https://en.wikipedia.org/wiki/Luma_(video)
		return glm::dot(linearColor, glm::vec3(0.2126f, 0.7152f, 0.0722f));
	}


	struct AliasTableData
	{
		// .x = probability, .y = alias index
		std::vector<glm::vec2> m_rowData; // Marginal 1D alias table of rows
		std::vector<glm::vec2> m_columnData; // Contiguously packed conditional 2D alias tables per row of columns
	};
	std::unique_ptr<AliasTableData> CreateAliasTableData(
		re::Texture::TextureParams const& texParams, re::Texture::IInitialData const* texData);
}