// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "Material.h"
#include "Material_GLTF.h"
#include "Sampler.h"
#include "SysInfo_Platform.h"
#include "Texture.h"

#include "Core\Assert.h"
#include "Core\Util\ImGuiUtils.h"


namespace
{
	constexpr char const* MaterialTypeToCStr(gr::Material::MaterialType matType)
	{
		switch (matType)
		{
		case gr::Material::MaterialType::GLTF_PBRMetallicRoughness: return "GLTF_PBRMetallicRoughness";
		default:
		{
			SEAssertF("Invalid material type");
			return "INVALID MATERIAL TYPE";
		}
		}
	}
}

namespace gr
{
	std::shared_ptr<gr::Material> Material::Create(std::string const& name, MaterialType materialType)
	{
		std::shared_ptr<gr::Material> newMat;

		switch (materialType)
		{
		case gr::Material::MaterialType::GLTF_PBRMetallicRoughness:
		{
			newMat.reset(new Material_GLTF(name));
		}
		break;
		default:
			SEAssertF("Invalid material type");
		}
		return newMat;
	}


	Material::Material(std::string const& name, MaterialType materialType)
		: INamedObject(name)
		, m_materialType(materialType)
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


	void Material::InitializeMaterialInstanceData(MaterialInstanceData& instanceData) const
	{
		// Zero out the instance data struct:
		memset(&instanceData, 0, sizeof(gr::Material::MaterialInstanceData));

		PackMaterialInstanceTextureSlotDescs(
			instanceData.m_textures.data(), instanceData.m_samplers.data(), instanceData.m_shaderSamplerNames);

		// Pipeline configuration flags:
		instanceData.m_alphaMode = m_alphaMode;
		instanceData.m_isDoubleSided = m_isDoubleSided;
		instanceData.m_isShadowCaster = m_isShadowCaster;

		// GPU data:
		PackMaterialParamsData(instanceData.m_materialParamData.data(), instanceData.m_materialParamData.size());

		// Metadata:
		instanceData.m_type = m_materialType;
		strcpy(instanceData.m_materialName, GetName().c_str());
		instanceData.m_srcMaterialUniqueID = GetUniqueID();
	}


	std::shared_ptr<re::Buffer> Material::CreateInstancedBuffer(
		re::Buffer::Type bufferType, 
		std::vector<MaterialInstanceData const*> const& instanceData)
	{
		SEAssert(!instanceData.empty(), "Instance data is empty");

		const gr::Material::MaterialType materialType = instanceData.front()->m_type;
		switch (materialType)
		{
		case gr::Material::MaterialType::GLTF_PBRMetallicRoughness:
		{
			return gr::Material_GLTF::CreateInstancedBuffer(bufferType, instanceData);
		}
		break;
		default:
			SEAssertF("Invalid material type");
		}
		return nullptr;
	}


	std::shared_ptr<re::Buffer> Material::ReserveInstancedBuffer(MaterialType matType, uint32_t maxInstances)
	{
		switch (matType)
		{
		case gr::Material::MaterialType::GLTF_PBRMetallicRoughness:
		{
			return re::Buffer::CreateUncommittedArray<InstancedPBRMetallicRoughnessData>(
				InstancedPBRMetallicRoughnessData::s_shaderName,
				maxInstances,
				re::Buffer::Type::Mutable);
		}
		break;
		default: SEAssertF("Invalid material type");
		}
		return nullptr;
	}


	void Material::CommitMaterialInstanceData(
		re::Buffer* buffer, MaterialInstanceData const* instanceData, uint32_t baseOffset)
	{
		SEAssert(instanceData, "Instance data is null");
		SEAssert(baseOffset < buffer->GetNumElements(), "Base offset is OOB");
		SEAssert(buffer->GetType() == re::Buffer::Type::Mutable,
			"Only mutable buffers can be partially updated");

		const gr::Material::MaterialType materialType = instanceData->m_type;
		switch (materialType)
		{
		case gr::Material::MaterialType::GLTF_PBRMetallicRoughness:
		{
			gr::Material_GLTF::CommitMaterialInstanceData(buffer, instanceData, baseOffset);
		}
		break;
		default:
			SEAssertF("Invalid material type");
		}
	}


	bool Material::ShowImGuiWindow(MaterialInstanceData& instanceData)
	{
		bool isDirty = false;

		ImGui::Text("Source material name: \"%s\"", instanceData.m_materialName);
		ImGui::Text("Source material Type: %s", MaterialTypeToCStr(instanceData.m_type));
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

		switch (instanceData.m_type)
		{
		case gr::Material::MaterialType::GLTF_PBRMetallicRoughness:
		{
			isDirty |= gr::Material_GLTF::ShowImGuiWindow(instanceData);
		}
		break;
		default: SEAssertF("Invalid type");
		}

		return isDirty;
	}
}

