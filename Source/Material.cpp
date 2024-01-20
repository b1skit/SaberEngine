// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Material.h"
#include "Material_GLTF.h"
#include "ParameterBlock.h"
#include "Sampler.h"
#include "SysInfo_Platform.h"
#include "Texture.h"


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


	constexpr char const* AlphaModeToCStr(gr::Material::AlphaMode alphaMode)
	{
		switch (alphaMode)
		{
		case gr::Material::AlphaMode::Opaque: return "Opaque";
		case gr::Material::AlphaMode::Clip: return "Clip";
		case gr::Material::AlphaMode::AlphaBlended: return "Alpha blended";
		default:
		{
			SEAssertF("Invalid alpha mode type");
			return "INVALID ALPHA MODE";
		}
		}
	}


	constexpr char const* DoubleSidedModeToCStr(gr::Material::DoubleSidedMode doubleSidedMode)
	{
		switch (doubleSidedMode)
		{
		case gr::Material::DoubleSidedMode::SingleSided: return "Single sided";
		case gr::Material::DoubleSidedMode::DoubleSided: return "Double sided";
		default:
		{
			SEAssertF("Invalid double sided mode");
			return "INVALID DOUBLE SIDED MODE";
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
		: NamedObject(name)
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


	void Material::PackMaterialInstanceData(MaterialInstanceData& instanceData) const
	{
		// Zero out the instance data struct:
		memset(&instanceData, 0, sizeof(gr::Material::MaterialInstanceData));

		PackMaterialInstanceTextureSlotDescs(
			instanceData.m_textures.data(), instanceData.m_samplers.data(), instanceData.m_shaderSamplerNames);

		instanceData.m_alphaMode = m_alphaMode;
		instanceData.m_alphaCutoff = m_alphaCutoff;
		instanceData.m_doubleSidedMode = m_doubleSidedMode;

		PackMaterialInstanceData(&instanceData.m_materialParamData, instanceData.m_materialParamData.size());

		// Metadata:
		instanceData.m_type = m_materialType;
		strcpy(instanceData.m_materialName, GetName().c_str());
		instanceData.m_uniqueID = GetUniqueID();
	}


	std::shared_ptr<re::ParameterBlock> Material::CreateParameterBlock(
		re::ParameterBlock::PBType pbType, MaterialInstanceData const& instanceData)
	{
		switch (instanceData.m_type)
		{
		case gr::Material::MaterialType::GLTF_PBRMetallicRoughness:
		{
			return gr::Material_GLTF::CreateParameterBlock(pbType, instanceData);
		}
		break;
		default:
			SEAssertF("Invalid material type");
		}
		return nullptr;
	}


	bool Material::ShowImGuiWindow(MaterialInstanceData& instanceData)
	{
		bool isDirty = false;
		
		ImGui::Text("Name: \"%s\"", instanceData.m_materialName);
		ImGui::Text("Type: %s", MaterialTypeToCStr(instanceData.m_type));

		if (ImGui::CollapsingHeader(std::format("Textures##{}\"", 
			instanceData.m_uniqueID).c_str(), ImGuiTreeNodeFlags_None))
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
					instanceData.m_uniqueID).c_str(),
					ImGuiTreeNodeFlags_None))
				{
					instanceData.m_textures[slotIdx]->ShowImGuiWindow();
				}
				ImGui::EndDisabled();
			}
			ImGui::Unindent();
		}

		ImGui::Text("Alpha mode: %s", AlphaModeToCStr(instanceData.m_alphaMode));
		isDirty |= ImGui::SliderFloat("Alpha cutoff", &instanceData.m_alphaCutoff, 0.f, 1.f, "%.4f");
		ImGui::Text("Double sided mode: %s", DoubleSidedModeToCStr(instanceData.m_doubleSidedMode));


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

