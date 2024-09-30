// © 2023 Adam Badke. All rights reserved.
#include "BufferInput.h"
#include "Material_GLTF.h"
#include "Sampler.h"

#include "Core/Assert.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/ImGuiUtils.h"


namespace gr
{
	InstancedPBRMetallicRoughnessData Material_GLTF::GetPBRMetallicRoughnessParamsData() const
	{
		return InstancedPBRMetallicRoughnessData
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
				m_texSlots[TextureSlotIdx::BaseColor].m_uvChannelIdx,
				m_texSlots[TextureSlotIdx::MetallicRoughness].m_uvChannelIdx,
				m_texSlots[TextureSlotIdx::Normal].m_uvChannelIdx,
				m_texSlots[TextureSlotIdx::Occlusion].m_uvChannelIdx),

			.g_uvChannelIndexes1 = glm::uvec4(
				m_texSlots[TextureSlotIdx::Emissive].m_uvChannelIdx,
				0,
				0,
				0),
		};

		SEStaticAssert(sizeof(InstancedPBRMetallicRoughnessData) <= gr::Material::k_paramDataBlockByteSize,
			"InstancedPBRMetallicRoughnessData is too large to fit in "
			"gr::Material::MaterialInstanceRenderData::m_materialParamData. Consider increasing "
			"gr::Material::k_paramDataBlockByteSize");
	}


	Material_GLTF::Material_GLTF(std::string const& name)
		: Material(name, gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness)
		, INamedObject(name)
	{
		// GLTF defaults:
		m_alphaMode = AlphaMode::Opaque;
		m_alphaCutoff = 0.5f;
		m_isDoubleSided = false;

		m_isShadowCaster = true;

		m_texSlots.resize(TextureSlotIdx::TextureSlotIdx_Count);

		m_texSlots[TextureSlotIdx::BaseColor] = { nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "BaseColorTex", 0 };
		m_texSlots[TextureSlotIdx::MetallicRoughness] = { nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "MetallicRoughnessTex", 0 }; // G = roughness, B = metalness. R & A are unused.
		m_texSlots[TextureSlotIdx::Normal] = { nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "NormalTex", 0 };
		m_texSlots[TextureSlotIdx::Occlusion] = { nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "OcclusionTex", 0 };
		m_texSlots[TextureSlotIdx::Emissive] = { nullptr, re::Sampler::GetSampler("WrapAnisotropic"), "EmissiveTex", 0 };

		// Build a map from shader sampler name, to texture slot index:
		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			m_namesToSlotIndex.insert({ m_texSlots[i].m_shaderSamplerName, (uint32_t)i });
		}
	}


	void Material_GLTF::PackMaterialParamsData(void* dst, size_t maxSize) const
	{
		SEAssert(maxSize <= sizeof(InstancedPBRMetallicRoughnessData), "Not enough space to pack material instance data");

		InstancedPBRMetallicRoughnessData const& materialParamData = GetPBRMetallicRoughnessParamsData();
		memcpy(dst, &materialParamData, sizeof(InstancedPBRMetallicRoughnessData));
	}


	re::BufferInput Material_GLTF::CreateInstancedBuffer(
		re::Buffer::AllocationType bufferAlloc,
		std::vector<MaterialInstanceRenderData const*> const& instanceData)
	{
		const uint32_t numInstances = util::CheckedCast<uint32_t>(instanceData.size());

		std::vector<InstancedPBRMetallicRoughnessData> instancedMaterialData;
		instancedMaterialData.reserve(numInstances);

		for (size_t matIdx = 0; matIdx < numInstances; matIdx++)
		{
			SEAssert(instanceData[matIdx]->m_matEffect == gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness,
				"Incorrect material type found. All instanceData entries must have the same type");

			InstancedPBRMetallicRoughnessData& instancedEntry = instancedMaterialData.emplace_back();

			memcpy(&instancedEntry, 
				&instanceData[matIdx]->m_materialParamData, 
				sizeof(InstancedPBRMetallicRoughnessData));
		}

		return re::BufferInput(
			InstancedPBRMetallicRoughnessData::s_shaderName,
			re::Buffer::CreateArray(
				k_materialBufferName, // Name of the buffer: Used to map Effects to Buffers
				instancedMaterialData.data(),
				re::Buffer::BufferParams{
					.m_allocationType = bufferAlloc,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::Upload,
					.m_usageMask = re::Buffer::Usage::GPURead | re::Buffer::Usage::CPUWrite,
					.m_type = re::Buffer::Type::Structured,
					.m_arraySize = numInstances,
				}));
	}


	void Material_GLTF::CommitMaterialInstanceData(
		re::Buffer* buffer, MaterialInstanceRenderData const* instanceData, uint32_t baseOffset)
	{
		SEAssert(instanceData->m_matEffect == gr::Material::MaterialEffect::GLTF_PBRMetallicRoughness,
			"Incorrect material type found. All instanceData entries must have the same type");

		// We commit single elements for now as we need to access each element's material param data. This isn't ideal,
		// but it avoids copying the data into a temporary location and materials are typically updated infrequently
		buffer->Commit(
			reinterpret_cast<InstancedPBRMetallicRoughnessData const*>(instanceData->m_materialParamData.data()),
			baseOffset,
			1);
	}


	bool Material_GLTF::ShowImGuiWindow(MaterialInstanceRenderData& instanceData)
	{
		bool isDirty = false;

		if (ImGui::CollapsingHeader(std::format("Material_GLTF: {}##{}", 
			instanceData.m_materialName, util::PtrToID(&instanceData)).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			InstancedPBRMetallicRoughnessData* matData =
				reinterpret_cast<InstancedPBRMetallicRoughnessData*>(instanceData.m_materialParamData.data());
			
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

			// gr::Material: This is a Material instance, so we're modifying the data that will be sent to our buffers
			{
				const bool isOpaque = instanceData.m_alphaMode == Material::AlphaMode::Opaque;

				ImGui::BeginDisabled(isOpaque);
				isDirty |= ImGui::SliderFloat(
					std::format("Alpha cutoff##{}", util::PtrToID(&instanceData)).c_str(),
					&matData->g_f0AlphaCutoff.w,
					0.f,
					1.f,
					"%.4f");
				ImGui::EndDisabled();
			}

			ImGui::Unindent();
		}

		return isDirty;
	}
}
