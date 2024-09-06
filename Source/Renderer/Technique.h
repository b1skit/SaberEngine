// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"

#include "Core/Interfaces/INamedObject.h"


using TechniqueID = NameID;

namespace re
{
	class PipelineState;
}

namespace effect
{
	class Technique : public virtual core::INamedObject
	{
	public:
		static constexpr TechniqueID k_invalidTechniqueID = core::INamedObject::k_invalidNameID;

		static TechniqueID ComputeTechniqueID(std::string const& techniqueName);


	public:
		Technique(
			char const* name,
			std::vector<std::pair<std::string, re::Shader::ShaderType>> const& shaderNames,
			re::PipelineState const*,
			re::VertexStreamMap const*);

		Technique(Technique&&) = default;
		Technique& operator=(Technique&&) = default;

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
		return core::INamedObject::ComputeIDFromName(techniqueName);
	}


	inline TechniqueID Technique::GetTechniqueID() const
	{
		return GetNameID();
	}


	inline re::Shader const* Technique::GetShader() const
	{
		return m_resolvedShader.get();
	}
}
