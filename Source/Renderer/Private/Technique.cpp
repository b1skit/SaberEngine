// © 2024 Adam Badke. All rights reserved.
#include "Private/RasterizationState.h"
#include "Private/Technique.h"
#include "Private/VertexStreamMap.h"


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
		bool isSame = GetNameHash() == rhs.GetNameHash();

#if defined(DEBUG)
		isSame &= m_shaderMetadata.size() == rhs.m_shaderMetadata.size();
		if (isSame)
		{
			for (size_t i = 0; i < m_shaderMetadata.size(); ++i)
			{
				isSame &= m_shaderMetadata[i].m_extensionlessFilename == rhs.m_shaderMetadata[i].m_extensionlessFilename &&
					m_shaderMetadata[i].m_entryPoint == rhs.m_shaderMetadata[i].m_entryPoint &&
					m_shaderMetadata[i].m_type == rhs.m_shaderMetadata[i].m_type;

				if (!isSame)
				{
					break;
				}
			}

			SEAssert(isSame, "Multiple Techniques with the same name but different configuration detected");
		}
#endif

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