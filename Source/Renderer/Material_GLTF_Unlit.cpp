// © 2025 Adam Badke. All rights reserved.
#include "Material_GLTF_Unlit.h"
#include "EnumTypes.h"

#include "Core/Util/ImGuiUtils.h"

#include "Renderer/Shaders/Common/MaterialParams.h"


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

		m_texSlots.resize(TextureSlotIdx_Count);

		core::InvPtr<re::Sampler> const& clampPointSampler = re::Sampler::GetSampler("ClampMinMagMipPoint");

		m_texSlots[BaseColor] = { nullptr, clampPointSampler, "BaseColorTex", 0 };
	}


	void Material_GLTF_Unlit::Destroy()
	{
		m_baseColorFactor = glm::vec4(1.f, 1.f, 1.f, 1.f);
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
				m_texSlots[BaseColor].m_uvChannelIdx,
				m_materialID,
				0,
				0),
			.g_bindlessTextureIndexes0 = glm::uvec4(
				m_texSlots[BaseColor].m_texture->GetBindlessResourceHandle(re::ViewType::SRV),
				0,
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