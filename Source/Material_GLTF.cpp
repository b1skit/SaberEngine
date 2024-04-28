// © 2023 Adam Badke. All rights reserved.
#include "Core\Assert.h"
#include "Core\Util\CastUtils.h"
#include "Material_GLTF.h"
#include "Sampler.h"


namespace gr
{
	InstancedPBRMetallicRoughnessData Material_GLTF::GetPBRMetallicRoughnessParamsData() const
	{
		return InstancedPBRMetallicRoughnessData
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
		, INamedObject(name)
	{
		// TODO: Texture names should align with those in the GLTF documentation

		m_texSlots =
		{
			{nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "MatAlbedo" },
			{nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "MatMetallicRoughness" }, // G = roughness, B = metalness. R & A are unused.
			{nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "MatNormal" },
			{nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "MatOcclusion" },
			{nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "MatEmissive" },
		};

		// Build a map from shader sampler name, to texture slot index:
		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			m_namesToSlotIndex.insert({ m_texSlots[i].m_shaderSamplerName, (uint32_t)i });
		}
	}


	void Material_GLTF::PackMaterialInstanceData(void* dst, size_t maxSize) const
	{
		SEAssert(maxSize <= sizeof(InstancedPBRMetallicRoughnessData), "Not enough space to pack material instance data");

		InstancedPBRMetallicRoughnessData const& materialInstanceData = GetPBRMetallicRoughnessParamsData();
		memcpy(dst, &materialInstanceData, sizeof(InstancedPBRMetallicRoughnessData));
	}


	std::shared_ptr<re::Buffer> Material_GLTF::CreateInstancedBuffer(
		re::Buffer::Type bufferType,
		std::vector<MaterialInstanceData const*> const& instanceData)
	{
		const uint32_t numInstances = util::CheckedCast<uint32_t>(instanceData.size());

		std::vector<InstancedPBRMetallicRoughnessData> instancedMaterialData;
		instancedMaterialData.reserve(numInstances);

		for (size_t matIdx = 0; matIdx < numInstances; matIdx++)
		{
			SEAssert(instanceData[matIdx]->m_type == gr::Material::MaterialType::GLTF_PBRMetallicRoughness,
				"Incorrect material type found. All instanceData entries must have the same type");

			InstancedPBRMetallicRoughnessData& instancedEntry = instancedMaterialData.emplace_back();

			memcpy(&instancedEntry, 
				&instanceData[matIdx]->m_materialParamData, 
				sizeof(InstancedPBRMetallicRoughnessData));
		}

		std::shared_ptr<re::Buffer> instancedMaterialParams = re::Buffer::CreateArray(
			InstancedPBRMetallicRoughnessData::s_shaderName,
			instancedMaterialData.data(),
			numInstances,
			bufferType);

		return instancedMaterialParams;
	}


	void Material_GLTF::CommitMaterialInstanceData(
		re::Buffer* buffer, MaterialInstanceData const* instanceData, uint32_t baseOffset)
	{
		SEAssert(instanceData->m_type == gr::Material::MaterialType::GLTF_PBRMetallicRoughness,
			"Incorrect material type found. All instanceData entries must have the same type");

		// We commit single elements for now as we need to access each element's material param data. This isn't ideal,
		// but it avoids copying the data into a temporary location and materials are typically updated infrequently
		buffer->Commit(
			reinterpret_cast<InstancedPBRMetallicRoughnessData const*>(instanceData->m_materialParamData.data()),
			baseOffset++,
			1);
	}


	bool Material_GLTF::ShowImGuiWindow(MaterialInstanceData& instanceData)
	{
		bool isDirty = false;

		if (ImGui::CollapsingHeader(std::format("Material_GLTF: {}##{}", 
			instanceData.m_materialName, instanceData.m_materialUniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			InstancedPBRMetallicRoughnessData* matData =
				reinterpret_cast<InstancedPBRMetallicRoughnessData*>(instanceData.m_materialParamData.data());
			
			isDirty |= ImGui::ColorEdit3(
				std::format("Base color factor##{}", instanceData.m_materialUniqueID).c_str(),
				&matData->g_baseColorFactor.r, ImGuiColorEditFlags_Float);

			isDirty |= ImGui::SliderFloat(std::format("Metallic factor##{}", instanceData.m_materialUniqueID).c_str(), 
				&matData->g_metallicFactor, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::SliderFloat(std::format("Roughness factor##{}", instanceData.m_materialUniqueID).c_str(),
				&matData->g_roughnessFactor, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::SliderFloat(std::format("Normal scale##{}", instanceData.m_materialUniqueID).c_str(),
				&matData->g_normalScale, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::SliderFloat(std::format("Occlusion strength##{}", instanceData.m_materialUniqueID).c_str(),
				&matData->g_occlusionStrength, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::ColorEdit3(std::format("Emissive factor##{}", "", instanceData.m_materialUniqueID).c_str(),
				&matData->g_emissiveFactorStrength.r, ImGuiColorEditFlags_Float);

			isDirty |= ImGui::SliderFloat(std::format("Emissive strength##{}", instanceData.m_materialUniqueID).c_str(),
				&matData->g_emissiveFactorStrength.w, 0.f, 1000.f, "%0.3f");

			isDirty |= ImGui::ColorEdit3(std::format("F0##{}", instanceData.m_materialUniqueID).c_str(), 
				&matData->g_f0.r, ImGuiColorEditFlags_Float);

			ImGui::Unindent();
		}

		return isDirty;
	}
}
