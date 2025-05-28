// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Material.h"
#include "Texture.h"


struct PBRMetallicRoughnessData;

namespace
{
	template<typename T>
	struct MaterialLoadContext_GLTF_PBRMetallicRoughness;

	template<typename T>
	struct DefaultMaterialLoadContext_GLTF_PBRMetallicRoughness;
}

namespace gr
{
	class Material_GLTF_PBRMetallicRoughness final : public virtual Material
	{
	public:
		enum TextureSlotIdx : uint8_t
		{
			BaseColor			= 0,
			MetallicRoughness	= 1,
			Normal				= 2,
			Occlusion			= 3,
			Emissive			= 4,

			TextureSlotIdx_Count
		};


		static constexpr re::Texture::Format GetDefaultTextureFormat(gr::Material_GLTF_PBRMetallicRoughness::TextureSlotIdx);
		static constexpr re::Texture::ColorSpace GetDefaultTextureColorSpace(gr::Material_GLTF_PBRMetallicRoughness::TextureSlotIdx);

		static bool FilterRenderData(MaterialInstanceRenderData const*);

		static bool ShowImGuiWindow(MaterialInstanceRenderData&); // Returns true if data was modified


	public:
		void Destroy();


	public:
		// Base GLTF material properties:
		void SetEmissiveFactor(glm::vec3 const&);
		void SetNormalScale(float normalScale);
		void SetOcclusionStrength(float occlusionStrength);

		// GLTF PBR Metallic Roughness properties:
		void SetBaseColorFactor(glm::vec4 const&);
		void SetMetallicFactor(float metallicFactor);
		void SetRoughnessFactor(float roughnessFactor);

		// Non-standard GLTF properties:
		void SetF0(glm::vec3 f0);
		void SetEmissiveStrength(float emissiveStrength);


	private:
		// Combined properties of a base GLTF material, and the PBR metallic-roughness parameters
		// https ://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-material
		// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-material-pbrmetallicroughness
		
		// Base GLTF material properties:
		glm::vec3 m_emissiveFactor = glm::vec3(0.f, 0.f, 0.f);
		float m_normalScale = 1.f;
		float m_occlusionStrength = 1.f;


		// GLTF PBR Metallic Roughness properties:
		glm::vec4 m_baseColorFactor = glm::vec4(1.f, 1.f, 1.f, 1.f);
		float m_metallicFactor = 1.f;
		float m_roughnessFactor = 1.f;

		// Non-standard GLTF properties:
		glm::vec3 m_f0 = glm::vec3(0.04, 0.04f, 0.04f);
		float m_emissiveStrength = 0.f;


	private:
		void PackMaterialParamsData(void*, size_t maxSize) const override;


		template<typename T>
		friend struct MaterialLoadContext_GLTF_PBRMetallicRoughness;

		template<typename T>
		friend struct DefaultMaterialLoadContext_GLTF_PBRMetallicRoughness;

	private:
		Material_GLTF_PBRMetallicRoughness(std::string const& name);


	private:
		PBRMetallicRoughnessData GetPBRMetallicRoughnessParamsData() const;
	};


	inline constexpr re::Texture::Format Material_GLTF_PBRMetallicRoughness::GetDefaultTextureFormat(
		gr::Material_GLTF_PBRMetallicRoughness::TextureSlotIdx slotIdx)
	{
		switch (slotIdx)
		{
		case gr::Material_GLTF_PBRMetallicRoughness::BaseColor:
		case gr::Material_GLTF_PBRMetallicRoughness::MetallicRoughness:
		case gr::Material_GLTF_PBRMetallicRoughness::Normal:
		case gr::Material_GLTF_PBRMetallicRoughness::Occlusion:
		case gr::Material_GLTF_PBRMetallicRoughness::Emissive:
		case gr::Material_GLTF_PBRMetallicRoughness::TextureSlotIdx_Count:
		default:
			return re::Texture::Format::RGBA8_UNORM;
		}
	}


	inline constexpr re::Texture::ColorSpace Material_GLTF_PBRMetallicRoughness::GetDefaultTextureColorSpace(
		gr::Material_GLTF_PBRMetallicRoughness::TextureSlotIdx slotIdx)
	{
		switch (slotIdx)
		{
		case gr::Material_GLTF_PBRMetallicRoughness::BaseColor: return re::Texture::ColorSpace::sRGB; break;
		case gr::Material_GLTF_PBRMetallicRoughness::MetallicRoughness: return re::Texture::ColorSpace::Linear;
		case gr::Material_GLTF_PBRMetallicRoughness::Normal: return re::Texture::ColorSpace::Linear; break;
		case gr::Material_GLTF_PBRMetallicRoughness::Occlusion: re::Texture::ColorSpace::Linear; break;
		case gr::Material_GLTF_PBRMetallicRoughness::Emissive: return re::Texture::ColorSpace::sRGB; break; // GLTF spec: Must be converted to linear before use
		}
		return re::Texture::ColorSpace::Linear; // This should never happen
	}


	inline bool Material_GLTF_PBRMetallicRoughness::FilterRenderData(MaterialInstanceRenderData const* renderData)
	{
		SEAssert(renderData, "Render data pointer is null");
		return gr::Material::EffectIDToMaterialID(renderData->m_effectID) == gr::Material::GLTF_PBRMetallicRoughness;
	}


	inline void Material_GLTF_PBRMetallicRoughness::SetEmissiveFactor(glm::vec3 const& emissiveFactor)
	{
		m_emissiveFactor = emissiveFactor;
	}


	inline void Material_GLTF_PBRMetallicRoughness::SetNormalScale(float normalScale)
	{
		m_normalScale = normalScale;
	}


	inline void Material_GLTF_PBRMetallicRoughness::SetOcclusionStrength(float occlusionStrength)
	{
		m_occlusionStrength = occlusionStrength;
	}


	inline void Material_GLTF_PBRMetallicRoughness::SetBaseColorFactor(glm::vec4 const& baseColorFactor)
	{
		m_baseColorFactor = baseColorFactor;
	}


	inline void Material_GLTF_PBRMetallicRoughness::SetMetallicFactor(float metallicFactor)
	{
		m_metallicFactor = metallicFactor;
	}


	inline void Material_GLTF_PBRMetallicRoughness::SetRoughnessFactor(float roughnessFactor)
	{
		m_roughnessFactor = roughnessFactor;
	}


	inline void Material_GLTF_PBRMetallicRoughness::SetF0(glm::vec3 f0)
	{
		m_f0 = f0;
	}


	inline void Material_GLTF_PBRMetallicRoughness::SetEmissiveStrength(float emissiveStrength)
	{
		m_emissiveStrength = emissiveStrength;
	}
}

