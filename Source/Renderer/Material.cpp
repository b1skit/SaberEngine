// � 2022 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "Buffer.h"
#include "Material.h"
#include "Material_GLTF_PBRMetallicRoughness.h"
#include "Material_GLTF_Unlit.h"
#include "RenderManager.h"
#include "SysInfo_Platform.h"
#include "Texture.h"

#include "Core/Assert.h"
#include "Core/Util/ImGuiUtils.h"


namespace gr
{
	Material::MaterialID Material::EffectIDToMaterialID(EffectID effectID)
	{
		util::CHashKey matEffectHashKey =
			util::CHashKey::Create(re::RenderManager::Get()->GetEffectDB().GetEffect(effectID)->GetName());

		switch (matEffectHashKey.GetHash())
		{
		case util::CHashKey(k_materialNames[GLTF_PBRMetallicRoughness]): return GLTF_PBRMetallicRoughness;
		case util::CHashKey(k_materialNames[GLTF_Unlit]): return GLTF_Unlit;
		default: SEAssertF("Invalid EffectID. Material names and Effect names must be the same to be associated via an "
			"Effect Buffers definition");
		}
		SEStaticAssert(MaterialID_Count == 2, "Number of materials has changed. This must be updated");

		return MaterialID_Count; // This should never happen
	}


	Material::Material(std::string const& name, MaterialID materialID)
		: INamedObject(name)
		, m_materialID(materialID)
		, m_effectID(effect::Effect::ComputeEffectID(k_materialNames[materialID]))
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


	core::InvPtr<re::Texture> Material::GetTexture(std::string const& samplerName) const
	{
		auto const& index = m_namesToSlotIndex.find(samplerName);

		SEAssert(index != m_namesToSlotIndex.end() && 
			(uint32_t)index->second < (uint32_t)m_texSlots.size(),
			"Invalid sampler name");

		return m_texSlots[index->second].m_texture;
	}


	void Material::PackMaterialInstanceTextureSlotDescs(
		core::InvPtr<re::Texture>* textures,
		core::InvPtr<re::Sampler>* samplers,
		char shaderNames[][k_shaderSamplerNameLength]) const
	{
		SEAssert(m_texSlots.size() <= k_numTexInputs, "Too many texture slot descriptions");

		// Populate the texture/sampler data:
		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			textures[i] = m_texSlots[i].m_texture;
			samplers[i] = m_texSlots[i].m_samplerObject;

			SEAssert(m_texSlots[i].m_shaderSamplerName.size() < k_shaderSamplerNameLength, 
				"Shader name is too long. Consider shortening it, or increasing k_shaderSamplerNameLength");

			strcpy(&shaderNames[i][0], m_texSlots[i].m_shaderSamplerName.c_str());
		}
	}


	void Material::InitializeMaterialInstanceData(MaterialInstanceRenderData& instanceData) const
	{
		// Reinitialize the instance data struct
		instanceData = {};

		PackMaterialInstanceTextureSlotDescs(
			instanceData.m_textures.data(), instanceData.m_samplers.data(), instanceData.m_shaderSamplerNames);

		// Pipeline configuration flags:
		instanceData.m_alphaMode = m_alphaMode;
		instanceData.m_isDoubleSided = m_isDoubleSided;
		instanceData.m_isShadowCaster = m_isShadowCaster;

		// GPU data:
		PackMaterialParamsData(std::span<std::byte>{
			reinterpret_cast<std::byte*>(instanceData.m_materialParamData.data()), 
			instanceData.m_materialParamData.size()
		});

		// Metadata:
		instanceData.m_effectID = m_effectID;
		strcpy(instanceData.m_materialName, GetName().c_str());
		instanceData.m_srcMaterialUniqueID = GetUniqueID();
	}


	void Material::MaterialInstanceRenderData::RegisterGeometryResources(
		MaterialInstanceRenderData const& materialInstanceRenderData, re::AccelerationStructure::Geometry& geometry)
	{
		geometry.SetGeometryFlags(materialInstanceRenderData.m_alphaMode == gr::Material::AlphaMode::Opaque ?
			re::AccelerationStructure::GeometryFlags::Opaque :
			re::AccelerationStructure::GeometryFlags::GeometryFlags_None);

		geometry.SetEffectID(materialInstanceRenderData.m_effectID);
		geometry.SetDrawstyleBits(
			gr::Material::MaterialInstanceRenderData::GetDrawstyleBits(&materialInstanceRenderData));
	}


	effect::drawstyle::Bitmask Material::MaterialInstanceRenderData::GetDrawstyleBits(
		gr::Material::MaterialInstanceRenderData const* materialInstanceData)
	{
		effect::drawstyle::Bitmask bitmask = 0;

		if (materialInstanceData)
		{
			// Alpha mode:
			switch (materialInstanceData->m_alphaMode)
			{
			case gr::Material::AlphaMode::Opaque:
			{
				bitmask |= effect::drawstyle::MaterialAlphaMode_Opaque;
			}
			break;
			case gr::Material::AlphaMode::Mask:
			{
				bitmask |= effect::drawstyle::MaterialAlphaMode_Clip;
			}
			break;
			case gr::Material::AlphaMode::Blend:
			{
				bitmask |= effect::drawstyle::MaterialAlphaMode_Blend;
			}
			break;
			default:
				SEAssertF("Invalid Material AlphaMode");
			}

			// Material sidedness:
			bitmask |= materialInstanceData->m_isDoubleSided ?
				effect::drawstyle::MaterialSidedness_Double : effect::drawstyle::MaterialSidedness_Single;
		}

		return bitmask;
	}


	uint8_t Material::MaterialInstanceRenderData::CreateInstanceInclusionMask(
		gr::Material::MaterialInstanceRenderData const* materialInstanceData)
	{
		uint8_t geoInstanceInclusionMask = 0;

		if (materialInstanceData)
		{
			// Alpha mode:
			switch (materialInstanceData->m_alphaMode)
			{
			case gr::Material::AlphaMode::Opaque:
			{
				geoInstanceInclusionMask |= re::AccelerationStructure::AlphaMode_Opaque;
			}
			break;
			case gr::Material::AlphaMode::Mask:
			{
				geoInstanceInclusionMask |= re::AccelerationStructure::AlphaMode_Mask;
			}
			break;
			case gr::Material::AlphaMode::Blend:
			{
				geoInstanceInclusionMask |= re::AccelerationStructure::AlphaMode_Blend;
			}
			break;
			default:
				SEAssertF("Invalid Material AlphaMode");
			}

			// Material sidedness:
			geoInstanceInclusionMask |= materialInstanceData->m_isDoubleSided ?
				re::AccelerationStructure::DoubleSided : re::AccelerationStructure::SingleSided;

			// Shadow casting:
			geoInstanceInclusionMask |= materialInstanceData->m_isShadowCaster ?
				re::AccelerationStructure::ShadowCaster : re::AccelerationStructure::NoShadow;
		}

		return geoInstanceInclusionMask;
	}


	bool Material::ShowImGuiWindow(MaterialInstanceRenderData& instanceData)
	{
		bool isDirty = false;

		ImGui::Text("Source material name: \"%s\"", instanceData.m_materialName);
		ImGui::Text("Source material Type: %s",
			gr::Material::k_materialNames[EffectIDToMaterialID(instanceData.m_effectID)]);
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
					re::Texture::ShowImGuiWindow(instanceData.m_textures[slotIdx]);
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

		switch (EffectIDToMaterialID(instanceData.m_effectID))
		{
		case gr::Material::MaterialID::GLTF_PBRMetallicRoughness:
		{
			isDirty |= gr::Material_GLTF_PBRMetallicRoughness::ShowImGuiWindow(instanceData);
		}
		break;
		case gr::Material::MaterialID::GLTF_Unlit:
		{
			isDirty |= gr::Material_GLTF_Unlit::ShowImGuiWindow(instanceData);
		}
		break;
		default: SEAssertF("Invalid type");
		}
		SEStaticAssert(MaterialID_Count == 2, "Number of materials has changed. This must be updated");

		return isDirty;
	}
}

