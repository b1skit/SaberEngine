// © 2024 Adam Badke. All rights reserved.
#include "RasterizationState.h"
#include "Technique.h"
#include "VertexStreamMap.h"


namespace effect
{
	Technique::Technique(
		char const* name,
		std::vector<std::pair<std::string, re::Shader::ShaderType>> const& shaderNames,
		re::RasterizationState const* rasterizationState,
		re::VertexStreamMap const* vertexStreamMap)
		: INamedObject(name)
		, m_resolvedShader(re::Shader::GetOrCreate(shaderNames, rasterizationState, vertexStreamMap))
	{
	}


	bool Technique::operator==(Technique const& rhs) const
	{
		if (this == &rhs)
		{
			return true;
		}
		const bool isSame = GetNameHash() == rhs.GetNameHash();

		SEAssert(!isSame ||
			((m_resolvedShader == rhs.m_resolvedShader) &&
				GetUniqueID() == rhs.GetUniqueID()),
			"Multiple Techniques with the same name detected");

		return isSame;
	}
}