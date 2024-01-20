// © 2023 Adam Badke. All rights reserved.
#include "Material_GLTF.h"
#include "ParameterBlock.h"
#include "Sampler.h"


namespace gr
{
	Material_GLTF::PBRMetallicRoughnessParams Material_GLTF::GetPBRMetallicRoughnessParamsData() const
	{
		return Material_GLTF::PBRMetallicRoughnessParams
		{
			.g_baseColorFactor = m_baseColorFactor,

			.g_metallicFactor = m_metallicFactor,
			.g_roughnessFactor = m_roughnessFactor,
			.g_normalScale = m_normalScale,
			.g_occlusionStrength = m_occlusionStrength,

			.g_emissiveFactorStrength = glm::vec4(m_emissiveFactor.rgb, m_emissiveStrength),

			.g_f0 = glm::vec4(m_f0.rgb, 0.f)
		};
	}


	Material_GLTF::Material_GLTF(std::string const& name)
		: Material(name, gr::Material::MaterialType::GLTF_PBRMetallicRoughness)
		, NamedObject(name)
	{
		// TODO: Texture names should align with those in the GLTF documentation

		m_texSlots =
		{
			{nullptr, re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear), "MatAlbedo" },
			{nullptr, re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear), "MatMetallicRoughness" }, // G = roughness, B = metalness. R & A are unused.
			{nullptr, re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear), "MatNormal" },
			{nullptr, re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear), "MatOcclusion" },
			{nullptr, re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear), "MatEmissive" },
		};

		// Build a map from shader sampler name, to texture slot index:
		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			m_namesToSlotIndex.insert({ m_texSlots[i].m_shaderSamplerName, (uint32_t)i });
		}
	}


	void Material_GLTF::PackMaterialInstanceData(void* dst, size_t maxSize) const
	{
		SEAssert(maxSize <= sizeof(PBRMetallicRoughnessParams), "Not enough space to pack material instance data");

		PBRMetallicRoughnessParams const& materialInstanceData = GetPBRMetallicRoughnessParamsData();
		memcpy(dst, &materialInstanceData, sizeof(PBRMetallicRoughnessParams));
	}


	std::shared_ptr<re::ParameterBlock> Material_GLTF::CreateParameterBlock(
		re::ParameterBlock::PBType pbType, MaterialInstanceData const& instanceData)
	{
		SEAssert(instanceData.m_type == gr::Material::MaterialType::GLTF_PBRMetallicRoughness,
			"Incorrect material type");
		
		return re::ParameterBlock::Create(
			PBRMetallicRoughnessParams::s_shaderName,
			*reinterpret_cast<PBRMetallicRoughnessParams const*>(instanceData.m_materialParamData.data()),
			pbType);
	}


	bool Material_GLTF::ShowImGuiWindow(MaterialInstanceData& instanceData)
	{
		bool isDirty = false;

		if (ImGui::CollapsingHeader(std::format("Material_GLTF: {}##{}", 
			instanceData.m_materialName, instanceData.m_uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			PBRMetallicRoughnessParams* matData =
				reinterpret_cast<PBRMetallicRoughnessParams*>(instanceData.m_materialParamData.data());
			
			isDirty |= ImGui::ColorEdit3(
				std::format("Base color factor##{}", instanceData.m_uniqueID).c_str(),
				&matData->g_baseColorFactor.r, ImGuiColorEditFlags_Float);

			isDirty |= ImGui::SliderFloat(std::format("Metallic factor##{}", instanceData.m_uniqueID).c_str(), 
				&matData->g_metallicFactor, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::SliderFloat(std::format("Roughness factor##{}", instanceData.m_uniqueID).c_str(),
				&matData->g_roughnessFactor, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::SliderFloat(std::format("Normal scale##{}", instanceData.m_uniqueID).c_str(),
				&matData->g_normalScale, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::SliderFloat(std::format("Occlusion strength##{}", instanceData.m_uniqueID).c_str(),
				&matData->g_occlusionStrength, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::ColorEdit3(std::format("Emissive factor##{}", "", instanceData.m_uniqueID).c_str(),
				&matData->g_emissiveFactorStrength.r, ImGuiColorEditFlags_Float);

			isDirty |= ImGui::SliderFloat(std::format("Emissive strength##{}", instanceData.m_uniqueID).c_str(),
				&matData->g_emissiveFactorStrength.w, 0.f, 1000.f, "%0.3f");

			isDirty |= ImGui::ColorEdit3(std::format("F0##{}", instanceData.m_uniqueID).c_str(), 
				&matData->g_f0.r, ImGuiColorEditFlags_Float);

			ImGui::Unindent();
		}

		return isDirty;
	}
}
