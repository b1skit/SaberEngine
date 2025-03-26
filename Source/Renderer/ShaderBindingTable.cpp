// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "Effect.h"
#include "EffectDB.h"
#include "RenderManager.h"
#include "Shader.h"
#include "ShaderBindingTable_Platform.h"
#include "ShaderBindingTable.h"
#include "Technique.h"

#include "Core/Assert.h"
#include "Core/InvPtr.h"
#include "Core/Logger.h"

#include "Core/Interfaces/INamedObject.h"

#include "Generated/DrawStyles.h"


namespace re
{
	std::shared_ptr<re::ShaderBindingTable> ShaderBindingTable::Create(char const* name, SBTParams const& sbtParams)
	{
		std::shared_ptr<re::ShaderBindingTable> newSBT;
		newSBT.reset(new ShaderBindingTable(name, sbtParams));

		// The SBT we're creating stores a weak pointer to itself so it can pass a shared_ptr to the RenderManager in 
		// case our API objects need to be re-created
		newSBT->m_self = newSBT;

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
		LOG("Destroying shader binding table \"%s\"", GetName().c_str());

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
				SEAssert(geo.GetEffectID() != 0, "Found an uninitialized EffectID on BLAS geometry record");
				SEAssert(geo.GetDrawstyleBits() != 0,
					"Found an uninitialized drawstyle bitmask on a BLAS geometry record. This is unexpected");

				const effect::drawstyle::Bitmask finalBitmask =
					geo.GetDrawstyleBits() | m_sbtParams.m_hitgroupStyles;

				effect::Technique const* technique = effectDB.GetTechnique(geo.GetEffectID(), finalBitmask);
				
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

		// Finally, register our SBT for API (re)creation. This needs to be done to ensure any shaders we access have
		// already been created (as we'll need their shader blobs etc)
		std::shared_ptr<re::ShaderBindingTable> thisSBT = m_self.lock();
		SEAssert(thisSBT, "Failed to convert SBT weak_ptr to a shared_ptr");

		re::RenderManager::Get()->RegisterForCreate<re::ShaderBindingTable>(thisSBT);
	}
}