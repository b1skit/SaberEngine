// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "Material.h"
#include "Material_GLTF.h"
#include "RenderManager.h"
#include "SysInfo_Platform.h"
#include "Texture.h"

#include "Core/Assert.h"
#include "Core/Util/ImGuiUtils.h"


namespace gr
{
	Material::EffectMaterial Material::EffectIDToEffectMaterial(EffectID effectID)
	{
		constexpr uint64_t k_GLTF_PBRMetallicRoughnessHash =
			util::HashKey(k_effectMaterialNames[GLTF_PBRMetallicRoughness]).GetHash();

		util::HashKey matEffectHashKey =
			util::HashKey::Create(re::RenderManager::Get()->GetEffectDB().GetEffect(effectID)->GetName());

		switch (matEffectHashKey.GetHash())
		{
		case k_GLTF_PBRMetallicRoughnessHash: return GLTF_PBRMetallicRoughness;
		default: SEAssertF("Invalid EffectID . Material names and Effect names must be the same to be associated "
			"via an Effect Buffers definition");
		}
		return EffectMaterial_Count; // This should never happen
	}


	std::shared_ptr<gr::Material> Material::Create(std::string const& name, EffectMaterial materialType)
	{
		std::shared_ptr<gr::Material> newMat;

		switch (materialType)
		{
		case gr::Material::EffectMaterial::GLTF_PBRMetallicRoughness:
		{
			newMat.reset(new Material_GLTF(name));
		}
		break;
		default:
			SEAssertF("Invalid material type");
		}
		return newMat;
	}


	Material::Material(std::string const& name, EffectMaterial effectMaterial)
		: INamedObject(name)
		, m_effectMaterial(effectMaterial)
		, m_effectID(effect::Effect::ComputeEffectID(k_effectMaterialNames[effectMaterial]))
		, m_alphaMode(AlphaMode::AlphaMode_Count)
		, m_alphaCutoff(0.f)
		, m_isDoubleSided(false)
		, m_isShadowCaster(true)
	{
		SEAssert(platform::SysInfo::GetMaxTextureBindPoints() >= k_numTexInputs,
			"GPU does not support enough texture binding points");

		m_texSlots.reserve(k_numTexInputs);
		m_namesToSlotIndex.reserve(k_numTexInputs);
	}


	re::Texture const* Material::GetTexture(std::string const& samplerName) const
	{
		auto const& index = m_namesToSlotIndex.find(samplerName);

		SEAssert(index != m_namesToSlotIndex.end() && 
			(uint32_t)index->second < (uint32_t)m_texSlots.size(),
			"Invalid sampler name");

		return m_texSlots[index->second].m_texture.get();
	}


	void Material::PackMaterialInstanceTextureSlotDescs(
		re::Texture const** textures, core::InvPtr<re::Sampler>* samplers, char shaderNames[][k_shaderSamplerNameLength]) const
	{
		SEAssert(m_texSlots.size() <= k_numTexInputs, "Too many texture slot descriptions");

		// Populate the texture/sampler data:
		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			textures[i] = m_texSlots[i].m_texture.get();
			samplers[i] = m_texSlots[i].m_samplerObject;

			SEAssert(m_texSlots[i].m_shaderSamplerName.size() < k_shaderSamplerNameLength, 
				"Shader name is too long. Consider shortening it, or increasing k_shaderSamplerNameLength");

			strcpy(&shaderNames[i][0], m_texSlots[i].m_shaderSamplerName.c_str());
		}
	}


	void Material::InitializeMaterialInstanceData(MaterialInstanceRenderData& instanceData) const
	{
		// Zero out the instance data struct:
		memset(&instanceData, 0, sizeof(gr::Material::MaterialInstanceRenderData));

		PackMaterialInstanceTextureSlotDescs(
			instanceData.m_textures.data(), instanceData.m_samplers.data(), instanceData.m_shaderSamplerNames);

		// Pipeline configuration flags:
		instanceData.m_alphaMode = m_alphaMode;
		instanceData.m_isDoubleSided = m_isDoubleSided;
		instanceData.m_isShadowCaster = m_isShadowCaster;

		// GPU data:
		PackMaterialParamsData(instanceData.m_materialParamData.data(), instanceData.m_materialParamData.size());

		// Metadata:
		instanceData.m_effectID = m_effectID;
		strcpy(instanceData.m_materialName, GetName().c_str());
		instanceData.m_srcMaterialUniqueID = GetUniqueID();
	}


	re::BufferInput Material::CreateInstancedBuffer(
		re::Buffer::StagingPool bufferAlloc, 
		std::vector<MaterialInstanceRenderData const*> const& instanceData)
	{
		SEAssert(!instanceData.empty(), "Instance data is empty");

		switch (EffectIDToEffectMaterial(instanceData.front()->m_effectID))
		{
		case gr::Material::EffectMaterial::GLTF_PBRMetallicRoughness:
		{
			return gr::Material_GLTF::CreateInstancedBuffer(bufferAlloc, instanceData);
		}
		break;
		default:
			SEAssertF("Invalid material type");
		}
		return re::BufferInput();
	}


	re::BufferInput Material::ReserveInstancedBuffer(EffectID matEffectID, uint32_t maxInstances)
	{
		switch (EffectIDToEffectMaterial(matEffectID))
		{
		case gr::Material::EffectMaterial::GLTF_PBRMetallicRoughness:
		{
			return re::BufferInput(
				InstancedPBRMetallicRoughnessData::s_shaderName,
				re::Buffer::CreateUncommittedArray<InstancedPBRMetallicRoughnessData>(
					InstancedPBRMetallicRoughnessData::s_shaderName,
					re::Buffer::BufferParams{
						.m_stagingPool = re::Buffer::StagingPool::Permanent,
						.m_memPoolPreference = re::Buffer::UploadHeap,
						.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
						.m_usageMask = re::Buffer::Structured,
						.m_arraySize = maxInstances,
					}));
		}
		break;
		default: SEAssertF("Invalid material name. Material names and Effect names must be the same to be associated "
			"via an Effect Buffers definition");
		}
		return re::BufferInput(); // This should never happen
	}


	void Material::CommitMaterialInstanceData(
		re::Buffer* buffer, MaterialInstanceRenderData const* instanceData, uint32_t baseOffset)
	{
		SEAssert(instanceData, "Instance data is null");
		SEAssert(baseOffset < buffer->GetArraySize(), "Base offset is OOB");
		SEAssert(buffer->GetAllocationType() == re::Buffer::StagingPool::Permanent,
			"Only mutable buffers can be partially updated");

		switch (EffectIDToEffectMaterial(instanceData->m_effectID))
		{
		case gr::Material::EffectMaterial::GLTF_PBRMetallicRoughness:
		{
			gr::Material_GLTF::CommitMaterialInstanceData(buffer, instanceData, baseOffset);
		}
		break;
		default:
			SEAssertF("Invalid material type");
		}
	}


	bool Material::ShowImGuiWindow(MaterialInstanceRenderData& instanceData)
	{
		bool isDirty = false;

		ImGui::Text("Source material name: \"%s\"", instanceData.m_materialName);
		ImGui::Text("Source material Type: %s",
			gr::Material::k_effectMaterialNames[EffectIDToEffectMaterial(instanceData.m_effectID)]);
		ImGui::Text(std::format("Source material UniqueID: {}", instanceData.m_srcMaterialUniqueID).c_str());

		if (ImGui::CollapsingHeader(std::format("Textures##{}\"",
			instanceData.m_srcMaterialUniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(instanceData.m_textures.size()); slotIdx++)
			{
				constexpr char const* k_emptyTexName = "<empty>";
				const bool hasTex = instanceData.m_textures[slotIdx] != nullptr;

				ImGui::BeginDisabled(!hasTex);
				if (ImGui::CollapsingHeader(std::format("Slot {}: {}{}{}##{}",
					slotIdx,
					hasTex ? "\"" : "",
					hasTex ? instanceData.m_shaderSamplerNames[slotIdx] : k_emptyTexName,
					hasTex ? "\"" : "",
					instanceData.m_srcMaterialUniqueID).c_str(),
					ImGuiTreeNodeFlags_None))
				{
					instanceData.m_textures[slotIdx]->ShowImGuiWindow();
				}
				ImGui::EndDisabled();
			}
			ImGui::Unindent();
		}

		// Material configuration:
		isDirty |= util::ShowBasicComboBox(
			std::format("Alpha mode##{}", util::PtrToID(&instanceData)).c_str(),
			k_alphaModeNames,
			_countof(k_alphaModeNames),
			instanceData.m_alphaMode);

		isDirty |= ImGui::Checkbox(
			std::format("Double sided?##{}", util::PtrToID(&instanceData)).c_str(), &instanceData.m_isDoubleSided);

		isDirty |= ImGui::Checkbox(
			std::format("Casts shadows?##{}", util::PtrToID(&instanceData)).c_str(), &instanceData.m_isShadowCaster);

		switch (EffectIDToEffectMaterial(instanceData.m_effectID))
		{
		case gr::Material::EffectMaterial::GLTF_PBRMetallicRoughness:
		{
			isDirty |= gr::Material_GLTF::ShowImGuiWindow(instanceData);
		}
		break;
		default: SEAssertF("Invalid type");
		}

		return isDirty;
	}
}

