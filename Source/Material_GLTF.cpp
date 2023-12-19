// © 2023 Adam Badke. All rights reserved.
#include "Material_GLTF.h"


namespace gr
{
	Material_GLTF::RenderData Material_GLTF::GetRenderData(gr::Material_GLTF& material)
	{
		return Material_GLTF::RenderData
		{
			.m_pbrMetallicRoughnessParams = material.GetPBRMetallicRoughnessParamsData()
		};
	}


	Material_GLTF::PBRMetallicRoughnessParams Material_GLTF::GetPBRMetallicRoughnessParamsData() const
	{
		return Material_GLTF::PBRMetallicRoughnessParams{

			.g_baseColorFactor = m_baseColorFactor,

			.g_metallicFactor = m_metallicFactor,
			.g_roughnessFactor = m_roughnessFactor,
			.g_normalScale = m_normalScale,
			.g_occlusionStrength = m_occlusionStrength,

			.g_emissiveFactorStrength = glm::vec4(m_emissiveFactor.rgb, m_emissiveStrength),

			.g_f0 = glm::vec4(m_f0.rgb, 0.f)
		};
	}


	void Material_GLTF::CreateUpdateParameterBlock()
	{
		if (m_matParamsIsDirty || !m_matParams)
		{
			PBRMetallicRoughnessParams const& pbrMetallicRoughnessParams = GetPBRMetallicRoughnessParamsData();

			if (!m_matParams)
			{
				m_matParams = re::ParameterBlock::Create(
					PBRMetallicRoughnessParams::s_shaderName,
					pbrMetallicRoughnessParams,
					re::ParameterBlock::PBType::Mutable);
			}
			else
			{
				m_matParams->Commit(pbrMetallicRoughnessParams);
			}
			m_matParamsIsDirty = false;
		}
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

		CreateUpdateParameterBlock();
	}


	std::shared_ptr<re::ParameterBlock> const Material_GLTF::GetParameterBlock()
	{
		CreateUpdateParameterBlock();

		return m_matParams;
	}


	void Material_GLTF::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(std::format("{}##{}", GetName(), GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			Material::ShowImGuiWindow();

			m_matParamsIsDirty |= ImGui::ColorEdit3(std::format("Base color factor##{}", "", GetUniqueID()).c_str(), &m_baseColorFactor.r, ImGuiColorEditFlags_Float);

			m_matParamsIsDirty |= ImGui::SliderFloat(std::format("Metallic factor##{}", GetUniqueID()).c_str(), &m_metallicFactor, 0.f, 1.f, "%0.3f");
			m_matParamsIsDirty |= ImGui::SliderFloat(std::format("Roughness factor##{}", GetUniqueID()).c_str(), &m_roughnessFactor, 0.f, 1.f, "%0.3f");
			m_matParamsIsDirty |= ImGui::SliderFloat(std::format("Normal scale##{}", GetUniqueID()).c_str(), &m_normalScale, 0.f, 1.f, "%0.3f");
			m_matParamsIsDirty |= ImGui::SliderFloat(std::format("Occlusion strength##{}", GetUniqueID()).c_str(), &m_occlusionStrength, 0.f, 1.f, "%0.3f");

			m_matParamsIsDirty |= ImGui::ColorEdit3(std::format("Emissive factor##{}", "", GetUniqueID()).c_str(), &m_emissiveFactor.r, ImGuiColorEditFlags_Float);
			m_matParamsIsDirty |= ImGui::SliderFloat(std::format("Emissive strength##{}", GetUniqueID()).c_str(), &m_emissiveStrength, 0.f, 1000.f, "%0.3f");

			m_matParamsIsDirty |= ImGui::ColorEdit3(std::format("F0##{}", GetUniqueID()).c_str(), &m_f0.r, ImGuiColorEditFlags_Float);
			ImGui::Unindent();
		}
	}
}
