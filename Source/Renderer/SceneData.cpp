// © 2022 Adam Badke. All rights reserved.
#include "AssetLoadUtils.h"
#include "MeshPrimitive.h"
#include "SceneData.h"
#include "Sampler.h"
#include "Shader.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/Inventory.h"

#include "Core/Interfaces/INamedObject.h"


namespace re
{
	SceneData::SceneData()
		: m_isCreated(false)
	{
	}


	SceneData::~SceneData()
	{
		SEAssert(m_isCreated == false, "Did the SceneData go out of scope before Destroy was called?");
	}


	void SceneData::Initialize()
	{
		//
	}


	void SceneData::Destroy()
	{
		{
			std::lock_guard<std::mutex> lock(m_meshPrimitivesMutex);

			m_meshPrimitives.clear();
		}

		m_isCreated = false; // Flag that Destroy has been called
	}


	bool SceneData::AddUniqueMeshPrimitive(std::shared_ptr<gr::MeshPrimitive>& meshPrimitive)
	{
		const uint64_t meshPrimitiveDataHash = meshPrimitive->GetDataHash();
		bool replacedIncomingPtr = false;
		{
			std::lock_guard<std::mutex> lock(m_meshPrimitivesMutex);

			auto const& result = m_meshPrimitives.find(meshPrimitiveDataHash);
			if (result != m_meshPrimitives.end())
			{
				LOG("MeshPrimitive \"%s\" has the same data hash as an existing MeshPrimitive. It will be replaced "
					"with a shared copy",
					meshPrimitive->GetName().c_str());

				meshPrimitive = result->second;
				replacedIncomingPtr = true;

				//// Add a marker to simplify debugging of shared meshes
				//constexpr char const* k_sharedMeshTag = " <shared>";
				//if (meshPrimitive->GetName().find(k_sharedMeshTag) == std::string::npos)
				//{
				//	meshPrimitive->SetName(meshPrimitive->GetName() + k_sharedMeshTag);
				//}
				
				// BUG HERE: We (currently) can't set the name on something that is shared, as another thread might be
				// using it (e.g. dereferencing .c_str())
			}
			else
			{
				m_meshPrimitives.insert({ meshPrimitiveDataHash, meshPrimitive });
			}
		}
		return replacedIncomingPtr;
	}
}