// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "EffectDB.h"
#include "RenderManager.h"
#include "ShaderBindingTable_Platform.h"
#include "ShaderBindingTable.h"


namespace re
{
	std::shared_ptr<re::ShaderBindingTable> ShaderBindingTable::Create(char const* name, SBTParams const& sbtParams)
	{
		std::shared_ptr<re::ShaderBindingTable> newSBT;
		newSBT.reset(new ShaderBindingTable(name, sbtParams));

		return newSBT;
	}


	ShaderBindingTable::ShaderBindingTable(char const* name, SBTParams const& sbtParams)
		: core::INamedObject(name)
		, m_platformParams(platform::ShaderBindingTable::CreatePlatformParams())
		, m_sbtParams(sbtParams)
	{
	}


	ShaderBindingTable::~ShaderBindingTable()
	{
		Destroy();
	}


	void ShaderBindingTable::Destroy()
	{
		// Guarantee the lifetime of any in-flight resources:
		if (m_platformParams)
		{
			re::RenderManager::Get()->RegisterForDeferredDelete(std::move(m_platformParams));
		}

		m_rayGenShaders.clear();
		m_missShaders.clear();
		m_hitGroupNamesAndShaders.clear();
		m_callableShaders.clear();
	}


	void ShaderBindingTable::Update(std::shared_ptr<re::AccelerationStructure> const& receivedTLAS)
	{
		if (receivedTLAS == m_TLAS)
		{
			return; // Nothing to do: TLAS will be recreated IFF geometry/materials change
		}
		m_TLAS = receivedTLAS;

		if (!m_TLAS) // TLAS either not created yet, or has been destroyed
		{
			return;
		}

		// If we made it this far, we need to (re)build the SBT:
		Destroy();
		m_platformParams = platform::ShaderBindingTable::CreatePlatformParams();
		
		// Resolve our shaders:
		effect::EffectDB const& effectDB = re::RenderManager::Get()->GetEffectDB();

		auto ResolveShaders = [&effectDB](
			std::set<ShaderID>& seenShaders,
			std::vector<std::pair<EffectID, effect::drawstyle::Bitmask>> const& styles,
			std::vector<core::InvPtr<re::Shader>>& shadersOut)
			{
				for (auto const& entry : styles)
				{
					core::InvPtr<re::Shader> const& shader = effectDB.GetResolvedShader(entry.first, entry.second);
					if (seenShaders.emplace(shader->GetShaderIdentifier()).second)
					{
						shadersOut.emplace_back(shader);
					}
				}
			};

		// Ray generation shaders:
		std::set<ShaderID> seenRayGenShaders;
		ResolveShaders(seenRayGenShaders, m_sbtParams.m_rayGenStyles, m_rayGenShaders);

		// Miss shaders:
		std::set<ShaderID> seenMissShaders;
		ResolveShaders(seenMissShaders, m_sbtParams.m_missStyles, m_missShaders);

		// Hit group shaders: Build a unique list of shaders used across all BLAS instances:
		re::AccelerationStructure::TLASParams const* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams const*>(m_TLAS->GetASParams());
		SEAssert(tlasParams, "Failed to get TLASParams");

		std::set<ShaderID> seenHitShaders;
		for (auto const& blas : tlasParams->m_blasInstances)
		{
			re::AccelerationStructure::BLASParams const* blasParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(blas->GetASParams());
			SEAssert(blasParams, "Failed to get TLASParams");

			for (auto const& geo : blasParams->m_geometry)
			{
				SEAssert(geo.m_effectID != 0, "Found an uninitialized EffectID on BLAS geometry record");
				SEAssert(geo.m_materialDrawstyleBits != 0,
					"Found an uninitialized drawstyle bitmask on a BLAS geometry record. This is unexpected");

				const effect::drawstyle::Bitmask finalBitmask =
					geo.m_materialDrawstyleBits | m_sbtParams.m_hitgroupStyles;

				effect::Technique const* technique = effectDB.GetTechnique(geo.m_effectID, finalBitmask);
				
				core::InvPtr<re::Shader> const& shader = technique->GetShader();
				if (seenHitShaders.emplace(shader->GetShaderIdentifier()).second)
				{
					// Note: We use the Technique name as the hit group name
					m_hitGroupNamesAndShaders.emplace_back(technique->GetName(), shader);
				}
			}
		}

		// Callable shaders:
		std::set<ShaderID> seenCallableShaders;
		ResolveShaders(seenCallableShaders, m_sbtParams.m_callableStyles, m_callableShaders);


#if defined (_DEBUG)
		// Validate we don't have any duplicates between our various sets of shaders:
		std::set<ShaderID> seenShaderIDs;
		auto ValidateUniqueIDs = [&seenShaderIDs](std::set<ShaderID> const& ids)
			{
				for (ShaderID id : ids)
				{
					const bool isUnique = seenShaderIDs.emplace(id).second;
					SEAssert(isUnique, "Found a duplicate ShaderID. This should not be possible");
				}
			};
		ValidateUniqueIDs(seenRayGenShaders);
		ValidateUniqueIDs(seenMissShaders);
		ValidateUniqueIDs(seenHitShaders);
		ValidateUniqueIDs(seenCallableShaders);
#endif

		platform::ShaderBindingTable::Update(*this, re::RenderManager::Get()->GetCurrentRenderFrameNum());
	}
}