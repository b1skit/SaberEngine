// © 2024 Adam Badke. All rights reserved.
#include "PipelineState.h"
#include "Technique.h"
#include "VertexStreamMap.h"


namespace effect
{
	Technique::Technique(
		char const* name,
		std::vector<std::pair<std::string, re::Shader::ShaderType>> const& shaderNames,
		re::PipelineState const* pipelineState,
		re::VertexStreamMap const* vertexStreamMap)
		: INamedObject(name)
		, m_resolvedShader(nullptr)
	{
		m_resolvedShader = re::Shader::GetOrCreate(shaderNames, pipelineState, vertexStreamMap);
	}


	bool Technique::operator==(Technique const& rhs) const
	{
		if (this == &rhs)
		{
			return true;
		}
		const bool isSame = GetNameHash() == rhs.GetNameHash();

		SEAssert(!isSame ||
			((m_resolvedShader.get() == rhs.m_resolvedShader.get()) &&
				GetUniqueID() == rhs.GetUniqueID()),
			"Multiple Techniques with the same name detected");

		return isSame;
	}
}