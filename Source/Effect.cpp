// © 2024 Adam Badke. All rights reserved.
#include "Effect.h"

#include "Core\Assert.h"
#include "Core\Definitions\ConfigKeys.h"


namespace effect
{
	Effect::Effect(char const* name)
		: INamedObject(name)
	{
	}


	bool Effect::operator==(Effect const& rhs) const
	{
		if (this == &rhs)
		{
			return true;
		}

		const bool isSame = GetEffectID() == rhs.GetEffectID();

		SEAssert(!isSame || m_techniques == rhs.m_techniques, 
			"Found an Effect with the same name but different Techniques");

		return  isSame;
	}


	void Effect::AddTechnique(effect::DrawStyle::Bitmask drawStyleBitmask, effect::Technique const* technique)
	{
		SEAssert(!m_techniques.contains(drawStyleBitmask),
			"A Technique has already been added for the given draw style bitmask");

		m_techniques.emplace(drawStyleBitmask, technique);
	}


	// -----------------------------------------------------------------------------------------------------------------


	Technique::Technique(
		char const* name,
		std::vector<std::pair<std::string, re::Shader::ShaderType>> const& shaderNames,
		re::PipelineState const* pipelineState)
		: INamedObject(name)
		, m_resolvedShader(nullptr)
	{
		m_resolvedShader = re::Shader::GetOrCreate(shaderNames, pipelineState);
	}


	bool Technique::operator==(Technique const& rhs) const
	{
		if (this == &rhs)
		{
			return true;
		}
		const bool isSame = GetNameID() == rhs.GetNameID();
		
		SEAssert(!isSame || 
			((m_resolvedShader.get() == rhs.m_resolvedShader.get()) &&
				GetUniqueID() == rhs.GetUniqueID()),
			"Multiple Techniques with the same name detected");

		return isSame;
	}


	TechniqueID Technique::GetTechniqueID() const
	{
		return GetNameID();
	}
}