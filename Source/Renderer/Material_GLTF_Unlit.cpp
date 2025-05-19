// © 2025 Adam Badke. All rights reserved.
#include "Material_GLTF_Unlit.h"

#include "Core/Util/ImGuiUtils.h"

#include "Shaders/Common/MaterialParams.h"


namespace gr
{
	Material_GLTF_Unlit::Material_GLTF_Unlit(std::string const& name)
		: Material(name, gr::Material::MaterialID::GLTF_Unlit)
		, INamedObject(name)
	{
		m_alphaMode = AlphaMode::Opaque;
		m_alphaCutoff = 0.5f;
		m_isDoubleSided = false;
		m_isShadowCaster = false; // Assume no shadows

		m_texSlots.resize(1);

		core::InvPtr<re::Sampler> const& clampPointSampler = re::Sampler::GetSampler("ClampMinMagMipPoint");

		m_texSlots[0] = { nullptr, clampPointSampler, "BaseColorTex", 0 };
	}


	void Material_GLTF_Unlit::Destroy()
	{
		m_baseColorFactor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	}


	re::BufferInput Material_GLTF_Unlit::CreateInstancedBuffer(
		re::Buffer::StagingPool stagingPool,
		std::vector<MaterialInstanceRenderData const*> const& instanceData)
	{
		const uint32_t numInstances = util::CheckedCast<uint32_t>(instanceData.size());

		std::vector<UnlitData> instancedMaterialData;
		instancedMaterialData.reserve(numInstances);

		for (size_t matIdx = 0; matIdx < numInstances; matIdx++)
		{
			SEAssert(instanceData[matIdx]->m_effectID == EffectID("GLTF_PBRMetallicRoughness"),
				"Incorrect material EffectID found. All instanceData entries must have the same type");

			UnlitData& instancedEntry = instancedMaterialData.emplace_back();

			memcpy(&instancedEntry,
				&instanceData[matIdx]->m_materialParamData,
				sizeof(UnlitData));
		}

		// Note: Material Buffer names are used to associate Effects with Buffers when building batches
		char const* bufferName = gr::Material::k_materialNames[gr::Material::MaterialID::GLTF_Unlit];

		return re::BufferInput(
			UnlitData::s_shaderName,
			re::Buffer::CreateArray(
				bufferName,
				instancedMaterialData.data(),
				re::Buffer::BufferParams{
					.m_stagingPool = stagingPool,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Structured,
					.m_arraySize = numInstances,
				}));
	}


	void Material_GLTF_Unlit::CommitMaterialInstanceData(
		re::Buffer* buffer, MaterialInstanceRenderData const* instanceData, uint32_t baseOffset)
	{
		SEAssert(instanceData->m_effectID == EffectID("GLTF_Unlit"),
			"Incorrect material EffectID found. All instanceData entries must have the same type");

		// We commit single elements for now as we need to access each element's material param data. This isn't ideal,
		// but it avoids copying the data into a temporary location and materials are typically updated infrequently
		UnlitData const* matData = reinterpret_cast<UnlitData const*>(instanceData->m_materialParamData.data());

		buffer->Commit(matData, baseOffset, 1);
	}


	void Material_GLTF_Unlit::PackMaterialParamsData(void* dst, size_t maxSize) const
	{
		SEAssert(sizeof(UnlitData) <= maxSize, "Not enough space to pack material instance data");

		UnlitData* typedDst = static_cast<UnlitData*>(dst);
		*typedDst = GetUnlitData();
	}


	UnlitData Material_GLTF_Unlit::GetUnlitData() const
	{
		return UnlitData{ 
			.g_baseColorFactor = m_baseColorFactor,
			.g_alphaCutuff = glm::vec4(
				m_alphaCutoff,
				0,
				0,
				0),
			.g_uvChannelIndexes0 = glm::uvec4(
				m_texSlots[0].m_uvChannelIdx,
				m_materialID,
				0,
				0),
		};

		SEStaticAssert(sizeof(UnlitData) <= gr::Material::k_paramDataBlockByteSize,
			"UnlitData is too large to fit in gr::Material::MaterialInstanceRenderData::m_materialParamData. Consider "
			"increasing gr::Material::k_paramDataBlockByteSize");
	}


	bool Material_GLTF_Unlit::ShowImGuiWindow(MaterialInstanceRenderData& instanceData)
	{
		bool isDirty = false;

		if (ImGui::CollapsingHeader(std::format("Material_GLTF_Unlit: {}##{}",
			instanceData.m_materialName, util::PtrToID(&instanceData)).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			UnlitData* matData = reinterpret_cast<UnlitData*>(instanceData.m_materialParamData.data());

			isDirty |= ImGui::ColorEdit3(
				std::format("Base color factor##{}", util::PtrToID(&instanceData)).c_str(),
				&matData->g_baseColorFactor.r, ImGuiColorEditFlags_Float);

			// gr::Material: This is a Material instance, so we're modifying the data that will be sent to our buffers
			{
				// Alpha-blended materials render their shadows using alpha clipping, if enabled
				const bool showAlphaCutoff =
					instanceData.m_alphaMode == Material::AlphaMode::Mask ||
					(instanceData.m_alphaMode == Material::AlphaMode::Blend && instanceData.m_isShadowCaster);

				ImGui::BeginDisabled(!showAlphaCutoff);
				isDirty |= ImGui::SliderFloat(
					std::format("Alpha cutoff##{}", util::PtrToID(&instanceData)).c_str(),
					&matData->g_alphaCutuff.x,
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
}