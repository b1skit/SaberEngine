// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Material.h"
#include "Material_GLTF.h"
#include "ParameterBlock.h"
#include "Shader_Platform.h"
#include "Shader.h"
#include "Texture.h"

using re::Shader;
using re::Texture;
using re::Sampler;
using re::ParameterBlock;

using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;
using std::unordered_map;
using std::make_unique;
using std::make_shared;
using std::vector;
using glm::vec4;
using glm::vec3;


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


	Material::Material(string const& name, MaterialType materialType)
		: NamedObject(name)
		, m_materialType(materialType)
		, m_matParams(nullptr)
		, m_matParamsIsDirty(true)
	{
	}


	std::shared_ptr<re::Texture> const Material::GetTexture(std::string const& samplerName) const
	{
		auto const& index = m_namesToSlotIndex.find(samplerName);

		SEAssert("Invalid sampler name",
			index != m_namesToSlotIndex.end() && 
			(uint32_t)index->second < (uint32_t)m_texSlots.size());

		return m_texSlots[index->second].m_texture;
	}


	void Material::ShowImGuiWindow()
	{
		ImGui::Text("Name: \"%s\"", GetName().c_str());
		ImGui::Text("Type: %s", MaterialTypeToCStr(m_materialType));

		if (ImGui::CollapsingHeader(std::format("Textures##{}\"", GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(m_texSlots.size()); slotIdx++)
			{
				if (ImGui::CollapsingHeader(std::format("Slot {}: \"{}\"##{}", 
					slotIdx, 
					m_texSlots[slotIdx].m_shaderSamplerName,
					GetUniqueID()).c_str(), 
					ImGuiTreeNodeFlags_None))
				{
					if (m_texSlots[slotIdx].m_texture)
					{
						m_texSlots[slotIdx].m_texture->ShowImGuiWindow();
					}
					else
					{
						ImGui::Text("<empty>");
					}
				}
			}
			ImGui::Unindent();
		}

		ImGui::Text("Alpha mode: %s", AlphaModeToCStr(m_alphaMode));
		m_matParamsIsDirty |= ImGui::SliderFloat("Alpha cutoff", &m_alphaCutoff, 0.f, 1.f, "%.4f");
		ImGui::Text("Double sided mode: %s", DoubleSidedModeToCStr(m_doubleSidedMode));
	}
}

