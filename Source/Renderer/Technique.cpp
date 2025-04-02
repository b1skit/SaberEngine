// © 2024 Adam Badke. All rights reserved.
#include "RasterizationState.h"
#include "Technique.h"
#include "VertexStreamMap.h"


namespace effect
{
	Technique::Technique(
		char const* name,
		std::vector<re::Shader::Metadata>&& shaderMetadata,
		re::RasterizationState const* rasterizationState,
		re::VertexStreamMap const* vertexStreamMap)
		: INamedObject(name)
		, m_resolvedShader(nullptr)
		, m_shaderMetadata(std::move(shaderMetadata))
		, m_rasterizationState(rasterizationState)
		, m_vertexStreamMap(vertexStreamMap)
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


	core::InvPtr<re::Shader> const& Technique::GetShader() const
	{
		if (!m_resolvedShader)
		{
			m_resolvedShader = re::Shader::GetOrCreate(m_shaderMetadata, m_rasterizationState, m_vertexStreamMap);
		}
		return m_resolvedShader;
	}
}