// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "Material.h"
#include "Material_GLTF.h"
#include "Sampler.h"
#include "SysInfo_Platform.h"
#include "Texture.h"

#include "Core/Assert.h"
#include "Core/Util/ImGuiUtils.h"


namespace gr
{
	std::shared_ptr<gr::Material> Material::Create(std::string const& name, MaterialEffect materialType)
	{
		std::shared_ptr<gr::Material> newMat;

		switch (materialType)
		{
		case gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness:
		{
			newMat.reset(new Material_GLTF(name));
		}
		break;
		default:
			SEAssertF("Invalid material type");
		}
		return newMat;
	}


	Material::Material(std::string const& name, MaterialEffect materialEffect)
		: INamedObject(name)
		, m_materialEffect(materialEffect)
		, m_effectID(effect::Effect::ComputeEffectID(k_materialEffectNames[materialEffect]))
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
		re::Texture const** textures, re::Sampler const** samplers, char shaderNames[][k_shaderSamplerNameLength]) const
	{
		SEAssert(m_texSlots.size() <= k_numTexInputs, "Too many texture slot descriptions");

		// Populate the texture/sampler data:
		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			textures[i] = m_texSlots[i].m_texture.get();
			samplers[i] = m_texSlots[i].m_samplerObject.get();

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
		instanceData.m_matEffect = m_materialEffect;
		instanceData.m_materialEffectID = m_effectID;
		strcpy(instanceData.m_materialName, GetName().c_str());
		instanceData.m_srcMaterialUniqueID = GetUniqueID();
	}


	re::BufferInput Material::CreateInstancedBuffer(
		re::Buffer::AllocationType bufferAlloc, 
		std::vector<MaterialInstanceRenderData const*> const& instanceData)
	{
		SEAssert(!instanceData.empty(), "Instance data is empty");

		const gr::Material::MaterialEffect materialType = instanceData.front()->m_matEffect;
		switch (materialType)
		{
		case gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness:
		{
			return gr::Material_GLTF::CreateInstancedBuffer(bufferAlloc, instanceData);
		}
		break;
		default:
			SEAssertF("Invalid material type");
		}
		return re::BufferInput();
	}


	re::BufferInput Material::ReserveInstancedBuffer(MaterialEffect matEffect, uint32_t maxInstances)
	{
		switch (matEffect)
		{
		case gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness:
		{
			return re::BufferInput(
				InstancedPBRMetallicRoughnessData::s_shaderName,
				re::Buffer::CreateUncommittedArray<InstancedPBRMetallicRoughnessData>(
					InstancedPBRMetallicRoughnessData::s_shaderName,
					re::Buffer::BufferParams{
						.m_allocationType = re::Buffer::AllocationType::Mutable,
						.m_memPoolPreference = re::Buffer::MemoryPoolPreference::Upload,
						.m_usageMask = re::Buffer::Usage::GPURead | re::Buffer::Usage::CPUWrite,
						.m_type = re::Buffer::Type::Structured,
						.m_arraySize = maxInstances,
					}));
		}
		break;
		default: SEAssertF("Invalid material type");
		}
		return re::BufferInput(); // This should never happen
	}


	void Material::CommitMaterialInstanceData(
		re::Buffer* buffer, MaterialInstanceRenderData const* instanceData, uint32_t baseOffset)
	{
		SEAssert(instanceData, "Instance data is null");
		SEAssert(baseOffset < buffer->GetArraySize(), "Base offset is OOB");
		SEAssert(buffer->GetAllocationType() == re::Buffer::AllocationType::Mutable,
			"Only mutable buffers can be partially updated");

		const gr::Material::MaterialEffect materialType = instanceData->m_matEffect;
		switch (materialType)
		{
		case gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness:
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
		ImGui::Text("Source material Type: %s", gr::Material::k_materialEffectNames[instanceData.m_matEffect]);
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

		switch (instanceData.m_matEffect)
		{
		case gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness:
		{
			isDirty |= gr::Material_GLTF::ShowImGuiWindow(instanceData);
		}
		break;
		default: SEAssertF("Invalid type");
		}

		return isDirty;
	}
}

