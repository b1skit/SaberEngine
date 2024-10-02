// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"


using TechniqueID = util::StringHash;

namespace re
{
	class PipelineState;
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
			std::vector<std::pair<std::string, re::Shader::ShaderType>> const& shaderNames,
			re::PipelineState const*,
			re::VertexStreamMap const*);

		Technique(Technique&&) noexcept = default;
		Technique& operator=(Technique&&) noexcept = default;

		~Technique() = default;

		bool operator==(Technique const&) const;


	public:
		TechniqueID GetTechniqueID() const;

		re::Shader const* GetShader() const;


	private:
		std::shared_ptr<re::Shader> m_resolvedShader;


	private: // No copying allowed
		Technique(Technique const&) = delete;
		Technique& operator=(Technique const&) = delete;
	};


	inline TechniqueID Technique::ComputeTechniqueID(std::string const& techniqueName)
	{
		return util::StringHash(techniqueName);
	}


	inline TechniqueID Technique::GetTechniqueID() const
	{
		return GetNameHash();
	}


	inline re::Shader const* Technique::GetShader() const
	{
		return m_resolvedShader.get();
	}
}
