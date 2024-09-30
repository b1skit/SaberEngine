// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "Material.h"

#include "Shaders/Common/MaterialParams.h"


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

	public:
		// Name of the Buffer objects created here. Used to map Effects to Buffers
		// Note: This is not the shader name
		static constexpr char const* k_materialBufferName = "GLTF_PBRMetallicRoughness";


	public:
		static re::BufferInput CreateInstancedBuffer(
			re::Buffer::AllocationType, std::vector<MaterialInstanceRenderData const*> const&);
		
		static void CommitMaterialInstanceData(re::Buffer*, MaterialInstanceRenderData const*, uint32_t baseOffset);

		static bool ShowImGuiWindow(MaterialInstanceRenderData&); // Returns true if data was modified


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


	protected: // Use the gr::Material::Create factory
		friend std::shared_ptr<gr::Material> gr::Material::Create(std::string const&, MaterialEffect);
		Material_GLTF(std::string const& name);


	private:
		InstancedPBRMetallicRoughnessData GetPBRMetallicRoughnessParamsData() const;
	};


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

