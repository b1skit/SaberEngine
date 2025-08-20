// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"

#include "Core/Util/HashKey.h"


using TechniqueID = util::HashKey;

namespace re
{
	class RasterState;
}

namespace effect
{
	class Technique : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		static TechniqueID ComputeTechniqueID(std::string const& techniqueName);


	public:
		Technique(
			char const* name,
			std::vector<re::Shader::Metadata>&&,
			re::RasterState const*,
			re::VertexStreamMap const*);

		Technique(Technique&&) noexcept = default;
		Technique& operator=(Technique&&) noexcept = default;

		~Technique() = default;

		bool operator==(Technique const&) const;


	public:
		TechniqueID GetTechniqueID() const;

		core::InvPtr<re::Shader> const& GetShader() const;


	private:
		mutable core::InvPtr<re::Shader> m_resolvedShader;

		// For deferred shader resolution:
		std::vector<re::Shader::Metadata> m_shaderMetadata;
		re::RasterState const* m_rasterizationState;
		re::VertexStreamMap const* m_vertexStreamMap;


	private: // No copying allowed
		Technique(Technique const&) = delete;
		Technique& operator=(Technique const&) = delete;
	};


	inline TechniqueID Technique::ComputeTechniqueID(std::string const& techniqueName)
	{
		return util::HashKey(techniqueName);
	}


	inline TechniqueID Technique::GetTechniqueID() const
	{
		return GetNameHash();
	}
}
