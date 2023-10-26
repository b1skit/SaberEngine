// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Material.h"


namespace gr
{
	class Material_GLTF : public virtual Material
	{
	public:
		std::shared_ptr<re::ParameterBlock> const GetParameterBlock() override;

		void ShowImGuiWindow() override;


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
		float m_emissiveStrength;


	protected: // Use the gr::Material::Create factory
		friend std::shared_ptr<gr::Material> gr::Material::Create(std::string const&, MaterialType);
		Material_GLTF(std::string const& name);


	private:
		void CreateUpdateParameterBlock() override;
	};


	inline void Material_GLTF::SetEmissiveFactor(glm::vec3 const& emissiveFactor)
	{
		m_emissiveFactor = emissiveFactor;
		m_matParamsIsDirty = true;
	}


	inline void Material_GLTF::SetNormalScale(float normalScale)
	{
		m_normalScale = normalScale;
		m_matParamsIsDirty = true;
	}


	inline void Material_GLTF::SetOcclusionStrength(float occlusionStrength)
	{
		m_occlusionStrength = occlusionStrength;
		m_matParamsIsDirty = true;
	}


	inline void Material_GLTF::SetBaseColorFactor(glm::vec4 const& baseColorFactor)
	{
		m_baseColorFactor = baseColorFactor;
		m_matParamsIsDirty = true;
	}


	inline void Material_GLTF::SetMetallicFactor(float metallicFactor)
	{
		m_metallicFactor = metallicFactor;
		m_matParamsIsDirty = true;
	}


	inline void Material_GLTF::SetRoughnessFactor(float roughnessFactor)
	{
		m_roughnessFactor = roughnessFactor;
		m_matParamsIsDirty = true;
	}


	inline void Material_GLTF::SetF0(glm::vec3 f0)
	{
		m_f0 = f0;
		m_matParamsIsDirty = true;
	}


	inline void Material_GLTF::SetEmissiveStrength(float emissiveStrength)
	{
		m_emissiveStrength = emissiveStrength;
		m_matParamsIsDirty = true;
	}
}

