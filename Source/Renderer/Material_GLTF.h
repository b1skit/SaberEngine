// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "Material.h"
#include "Texture.h"

#include "Shaders/Common/MaterialParams.h"


namespace
{
	template<typename T>
	struct MaterialLoadContext_GLTF;

	template<typename T>
	struct DefaultMaterialLoadContext_GLTF;
}

namespace re
{
	class BufferInput;
}

namespace gr
{
	class Material_GLTF : public virtual Material
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


		static constexpr re::Texture::Format GetDefaultTextureFormat(gr::Material_GLTF::TextureSlotIdx);
		static constexpr re::Texture::ColorSpace GetDefaultTextureColorSpace(gr::Material_GLTF::TextureSlotIdx);

		[[nodiscard]] static re::BufferInput CreateInstancedBuffer(
			re::Buffer::StagingPool, std::vector<MaterialInstanceRenderData const*> const&);
		
		static void CommitMaterialInstanceData(re::Buffer*, MaterialInstanceRenderData const*, uint32_t baseOffset);

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
		friend struct MaterialLoadContext_GLTF;

		template<typename T>
		friend struct DefaultMaterialLoadContext_GLTF;

	private:
		Material_GLTF(std::string const& name);


	private:
		PBRMetallicRoughnessData GetPBRMetallicRoughnessParamsData() const;
	};


	inline constexpr re::Texture::Format Material_GLTF::GetDefaultTextureFormat(
		gr::Material_GLTF::TextureSlotIdx slotIdx)
	{
		switch (slotIdx)
		{
		case gr::Material_GLTF::BaseColor:
		case gr::Material_GLTF::MetallicRoughness:
		case gr::Material_GLTF::Normal:
		case gr::Material_GLTF::Occlusion:
		case gr::Material_GLTF::Emissive:
		case gr::Material_GLTF::TextureSlotIdx_Count:
		default:
			return re::Texture::Format::RGBA8_UNORM;
		}
	}


	inline constexpr re::Texture::ColorSpace Material_GLTF::GetDefaultTextureColorSpace(
		gr::Material_GLTF::TextureSlotIdx slotIdx)
	{
		switch (slotIdx)
		{
		case gr::Material_GLTF::BaseColor: return re::Texture::ColorSpace::sRGB; break;
		case gr::Material_GLTF::MetallicRoughness: return re::Texture::ColorSpace::Linear;
		case gr::Material_GLTF::Normal: return re::Texture::ColorSpace::Linear; break;
		case gr::Material_GLTF::Occlusion: re::Texture::ColorSpace::Linear; break;
		case gr::Material_GLTF::Emissive: return re::Texture::ColorSpace::sRGB; break; // GLTF spec: Must be converted to linear before use
		}
		return re::Texture::ColorSpace::Linear; // This should never happen
	}


	inline void Material_GLTF::SetEmissiveFactor(glm::vec3 const& emissiveFactor)
	{
		m_emissiveFactor = emissiveFactor;
	}


	inline void Material_GLTF::SetNormalScale(float normalScale)
	{
		m_normalScale = normalScale;
	}


	inline void Material_GLTF::SetOcclusionStrength(float occlusionStrength)
	{
		m_occlusionStrength = occlusionStrength;
	}


	inline void Material_GLTF::SetBaseColorFactor(glm::vec4 const& baseColorFactor)
	{
		m_baseColorFactor = baseColorFactor;
	}


	inline void Material_GLTF::SetMetallicFactor(float metallicFactor)
	{
		m_metallicFactor = metallicFactor;
	}


	inline void Material_GLTF::SetRoughnessFactor(float roughnessFactor)
	{
		m_roughnessFactor = roughnessFactor;
	}


	inline void Material_GLTF::SetF0(glm::vec3 f0)
	{
		m_f0 = f0;
	}


	inline void Material_GLTF::SetEmissiveStrength(float emissiveStrength)
	{
		m_emissiveStrength = emissiveStrength;
	}
}

