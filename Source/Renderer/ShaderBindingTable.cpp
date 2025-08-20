// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "Context.h"
#include "Effect.h"
#include "Shader.h"
#include "ShaderBindingTable.h"
#include "ShaderBindingTable_Platform.h"
#include "Technique.h"

#include "Core/Assert.h"
#include "Core/InvPtr.h"
#include "Core/Logger.h"

#include "Core/Interfaces/INamedObject.h"

#include "Core/Util/HashKey.h"

#include "_generated/DrawStyles.h"


namespace re
{
	std::shared_ptr<re::ShaderBindingTable> ShaderBindingTable::Create(
		char const* name, SBTParams const& sbtParams, re::AccelerationStructure const* tlas)
	{
		std::shared_ptr<re::ShaderBindingTable> newSBT;
		newSBT.reset(new ShaderBindingTable(name, sbtParams));

		newSBT->Initialize(tlas);
		
		// Finally, register our SBT for API creation. This needs to be done to ensure any shaders we access have
		// already been created (as we'll need their shader blobs etc)
		newSBT->m_platObj->GetContext()->RegisterForCreate<re::ShaderBindingTable>(newSBT);

		return newSBT;
	}


	ShaderBindingTable::ShaderBindingTable(char const* name, SBTParams const& sbtParams)
		: core::INamedObject(name)
		, m_platObj(platform::ShaderBindingTable::CreatePlatformObject())
		, m_sbtParams(sbtParams)
	{
	}


	ShaderBindingTable::~ShaderBindingTable()
	{
		Destroy();
	}


	void ShaderBindingTable::Destroy()
	{
		LOG("Destroying shader binding table \"%s\"", GetName().c_str());

		// Guarantee the lifetime of any in-flight resources:
		if (m_platObj)
		{
			m_platObj->GetContext()->RegisterForDeferredDelete(std::move(m_platObj));
		}

		m_rayGenShaders.clear();
		m_missShaders.clear();
		m_hitGroupNamesAndShaders.clear();
		m_callableShaders.clear();
	}


	void ShaderBindingTable::Initialize(re::AccelerationStructure const* tlas)
	{
		SEAssert(tlas, "Invalid TLAS");

		SEAssert(m_platObj, "platform object should have been registered for deferred delete");
		m_platObj = platform::ShaderBindingTable::CreatePlatformObject();

		// Resolve our shaders:
		auto ResolveShaders = [](
			std::set<ShaderID>& seenShaders,
			EffectID effectID, 
			std::vector<effect::drawstyle::Bitmask> const& drawstyles,
			std::vector<core::InvPtr<re::Shader>>& shadersOut)
			{
				for (auto const& drawstyleBits : drawstyles)
				{
					core::InvPtr<re::Shader> const& shader = effectID.GetResolvedShader(drawstyleBits);
					if (seenShaders.emplace(shader->GetShaderIdentifier()).second)
					{
						shadersOut.emplace_back(shader);
					}
				}
			};

		// Ray generation shaders:
		std::set<ShaderID> seenRayGenShaders;
		ResolveShaders(seenRayGenShaders, m_sbtParams.m_effectID, m_sbtParams.m_rayGenStyles, m_rayGenShaders);

		// Miss shaders:
		std::set<ShaderID> seenMissShaders;
		ResolveShaders(seenMissShaders, m_sbtParams.m_effectID, m_sbtParams.m_missStyles, m_missShaders);

		// Hit group shaders: Build a unique list of shaders used across all BLAS instances:
		re::AccelerationStructure::TLASParams const* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams const*>(tlas->GetASParams());
		SEAssert(tlasParams, "Failed to get TLASParams");

		// Note: We must filter on Techniques,to ensure correct hit group layouts, as multiple Techniques can share the
		// same shader(s)
		std::set<TechniqueID> seenTechniques;

		for (auto const& blas : tlasParams->GetBLASInstances())
		{
			re::AccelerationStructure::BLASParams const* blasParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(blas->GetASParams());
			SEAssert(blasParams, "Failed to get TLASParams");

			for (auto const& geo : blasParams->m_geometry)
			{
				SEAssert(geo.GetEffectID() != 0, "Found an uninitialized EffectID on BLAS geometry record");
				SEAssert(geo.GetDrawstyleBits() != 0,
					"Found an uninitialized drawstyle bitmask on a BLAS geometry record. This is unexpected");

				const effect::drawstyle::Bitmask finalBitmask =
					geo.GetDrawstyleBits() | m_sbtParams.m_hitgroupStyles;

				effect::Technique const* technique = geo.GetEffectID().GetTechnique(finalBitmask);

				if (seenTechniques.emplace(technique->GetTechniqueID()).second)
				{
					// Note: We use the Technique name as the hit group name
					m_hitGroupNamesAndShaders.emplace_back(technique->GetName(), technique->GetShader());
				}
			}
		}

		// Callable shaders:
		std::set<ShaderID> seenCallableShaders;
		ResolveShaders(seenCallableShaders, m_sbtParams.m_effectID, m_sbtParams.m_callableStyles, m_callableShaders);


#if defined (_DEBUG)
		// Validate we don't have any duplicates between our various sets of shaders:
		std::set<util::HashKey> seenIDs;
		auto ValidateUniqueIDs = [&seenIDs](std::set<ShaderID> const& ids)
			{
				for (ShaderID id : ids)
				{
					const bool isUnique = seenIDs.emplace(id).second;
					SEAssert(isUnique, "Found a duplicate ID. This should not be possible");
				}
			};
		ValidateUniqueIDs(seenRayGenShaders);
		ValidateUniqueIDs(seenMissShaders);
		ValidateUniqueIDs(seenTechniques);
		ValidateUniqueIDs(seenCallableShaders);
#endif
	}
}