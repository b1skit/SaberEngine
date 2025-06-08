// © 2023 Adam Badke. All rights reserved.
#include "Private/EnumTypes.h"
#include "Private/Material_GLTF_PBRMetallicRoughness.h"
#include "Private/Sampler.h"

#include "Core/Assert.h"

#include "Core/Util/ImGuiUtils.h"

#include "Private/Renderer/Shaders/Common/MaterialParams.h"


namespace gr
{
	PBRMetallicRoughnessData Material_GLTF_PBRMetallicRoughness::GetPBRMetallicRoughnessParamsData() const
	{
		return PBRMetallicRoughnessData
		{
			.g_baseColorFactor = m_baseColorFactor,

			.g_metRoughNmlOccScales = glm::vec4(
				m_metallicFactor,
				m_roughnessFactor,
				m_normalScale,
				m_occlusionStrength),

			.g_emissiveFactorStrength = glm::vec4(m_emissiveFactor.rgb, m_emissiveStrength),

			.g_f0AlphaCutoff = glm::vec4(
				m_f0.rgb,
				m_alphaMode == Material::AlphaMode::Opaque ? 0.f : m_alphaCutoff),

			.g_uvChannelIndexes0 = glm::uvec4(
				m_texSlots[BaseColor].m_uvChannelIdx,
				m_texSlots[MetallicRoughness].m_uvChannelIdx,
				m_texSlots[Normal].m_uvChannelIdx,
				m_texSlots[Occlusion].m_uvChannelIdx),

			.g_uvChannelIndexes1 = glm::uvec4(
				m_texSlots[TextureSlotIdx::Emissive].m_uvChannelIdx,
				m_materialID,
				0,
				0),

			// DX12 only:
			.g_bindlessTextureIndexes0 = glm::uvec4(
				m_texSlots[BaseColor].m_texture ?
					m_texSlots[BaseColor].m_texture->GetBindlessResourceHandle(re::ViewType::SRV)
					: INVALID_RESOURCE_IDX,
				m_texSlots[MetallicRoughness].m_texture?
					m_texSlots[MetallicRoughness].m_texture->GetBindlessResourceHandle(re::ViewType::SRV)
					: INVALID_RESOURCE_IDX,
				m_texSlots[Normal].m_texture?
					m_texSlots[Normal].m_texture->GetBindlessResourceHandle(re::ViewType::SRV)
					: INVALID_RESOURCE_IDX,
				m_texSlots[Occlusion].m_texture ?
					m_texSlots[Occlusion].m_texture->GetBindlessResourceHandle(re::ViewType::SRV)
					: INVALID_RESOURCE_IDX),

			.g_bindlessTextureIndexes1 = glm::uvec4(
				m_texSlots[Emissive].m_texture ? 
					m_texSlots[Emissive].m_texture->GetBindlessResourceHandle(re::ViewType::SRV)
					: INVALID_RESOURCE_IDX,
				0,
				0,
				0),
		};

		SEStaticAssert(sizeof(PBRMetallicRoughnessData) <= gr::Material::k_paramDataBlockByteSize,
			"PBRMetallicRoughnessData is too large to fit in "
			"gr::Material::MaterialInstanceRenderData::m_materialParamData. Consider increasing "
			"gr::Material::k_paramDataBlockByteSize");
	}


	Material_GLTF_PBRMetallicRoughness::Material_GLTF_PBRMetallicRoughness(std::string const& name)
		: Material(name, gr::Material::MaterialID::GLTF_PBRMetallicRoughness)
		, INamedObject(name)
	{
		// GLTF defaults:
		m_alphaMode = AlphaMode::Opaque;
		m_alphaCutoff = 0.5f;
		m_isDoubleSided = false;
		m_isShadowCaster = true;

		m_texSlots.resize(TextureSlotIdx::TextureSlotIdx_Count);

		core::InvPtr<re::Sampler> const& wrapAnisoSampler = re::Sampler::GetSampler("WrapAnisotropic");

		m_texSlots[TextureSlotIdx::BaseColor] = { nullptr, wrapAnisoSampler, "BaseColorTex", 0 };
		m_texSlots[TextureSlotIdx::MetallicRoughness] = { nullptr, wrapAnisoSampler, "MetallicRoughnessTex", 0 }; // G = roughness, B = metalness. R & A are unused.
		m_texSlots[TextureSlotIdx::Normal] = { nullptr, wrapAnisoSampler, "NormalTex", 0 };
		m_texSlots[TextureSlotIdx::Occlusion] = { nullptr, wrapAnisoSampler, "OcclusionTex", 0 };
		m_texSlots[TextureSlotIdx::Emissive] = { nullptr, wrapAnisoSampler, "EmissiveTex", 0 };

		// Build a map from shader sampler name, to texture slot index:
		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			m_namesToSlotIndex.insert({ m_texSlots[i].m_shaderSamplerName, (uint32_t)i });
		}
	}


	void Material_GLTF_PBRMetallicRoughness::PackMaterialParamsData(void* dst, size_t maxSize) const
	{
		SEAssert(sizeof(PBRMetallicRoughnessData) <= maxSize, "Not enough space to pack material instance data");

		PBRMetallicRoughnessData* typedDst = static_cast<PBRMetallicRoughnessData*>(dst);
		*typedDst = GetPBRMetallicRoughnessParamsData();
	}


	bool Material_GLTF_PBRMetallicRoughness::ShowImGuiWindow(MaterialInstanceRenderData& instanceData)
	{
		bool isDirty = false;

		if (ImGui::CollapsingHeader(std::format("Material_GLTF_PBRMetallicRoughness: {}##{}", 
			instanceData.m_materialName, util::PtrToID(&instanceData)).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			PBRMetallicRoughnessData* matData =
				reinterpret_cast<PBRMetallicRoughnessData*>(instanceData.m_materialParamData.data());
			
			isDirty |= ImGui::ColorEdit3(
				std::format("Base color factor##{}", util::PtrToID(&instanceData)).c_str(),
				&matData->g_baseColorFactor.r, ImGuiColorEditFlags_Float);

			isDirty |= ImGui::SliderFloat(std::format("Metallic factor##{}", util::PtrToID(&instanceData)).c_str(), 
				&matData->g_metRoughNmlOccScales.x, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::SliderFloat(std::format("Roughness factor##{}", util::PtrToID(&instanceData)).c_str(),
				&matData->g_metRoughNmlOccScales.y, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::SliderFloat(std::format("Normal scale##{}", util::PtrToID(&instanceData)).c_str(),
				&matData->g_metRoughNmlOccScales.z, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::SliderFloat(std::format("Occlusion strength##{}", util::PtrToID(&instanceData)).c_str(),
				&matData->g_metRoughNmlOccScales.w, 0.f, 1.f, "%0.3f");

			isDirty |= ImGui::ColorEdit3(std::format("Emissive factor##{}", "", util::PtrToID(&instanceData)).c_str(),
				&matData->g_emissiveFactorStrength.r, ImGuiColorEditFlags_Float);

			isDirty |= ImGui::SliderFloat(std::format("Emissive strength##{}", util::PtrToID(&instanceData)).c_str(),
				&matData->g_emissiveFactorStrength.w, 0.f, 1000.f, "%0.3f");

			isDirty |= ImGui::ColorEdit3(std::format("F0##{}", util::PtrToID(&instanceData)).c_str(), 
				&matData->g_f0AlphaCutoff.r, ImGuiColorEditFlags_Float);

			// gr::Material: This is a Material instance, so we're modifying the data that will be sent to our GPU buffers
			{
				// Alpha-blended materials render their shadows using alpha clipping, if enabled
				const bool showAlphaCutoff = 
					instanceData.m_alphaMode == Material::AlphaMode::Mask ||
					(instanceData.m_alphaMode == Material::AlphaMode::Blend && instanceData.m_isShadowCaster);

				ImGui::BeginDisabled(!showAlphaCutoff);
				isDirty |= ImGui::SliderFloat(
					std::format("Alpha cutoff##{}", util::PtrToID(&instanceData)).c_str(),
					&matData->g_f0AlphaCutoff.w,
					0.f,
					1.f,
					"%.4f");
				ImGui::EndDisabled();

				ImGui::SetItemTooltip("Alpha clipped or alpha blended materials only.\n"
					"Alpha-blended materials render shadows using alpha clipping");
			}
			

			ImGui::Unindent();
		}

		return isDirty;
	}


	void Material_GLTF_PBRMetallicRoughness::Destroy()
	{
		m_texSlots.clear();
		m_namesToSlotIndex.clear();
	}
}
