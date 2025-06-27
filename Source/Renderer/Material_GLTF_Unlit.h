// � 2025 Adam Badke. All rights reserved.
#pragma once
#include "Material.h"


struct UnlitData;

namespace gr
{
	class Material_GLTF_Unlit final : public virtual Material
	{
	public:
		enum TextureSlotIdx : uint8_t
		{
			BaseColor = 0,

			TextureSlotIdx_Count
		};

		static bool FilterRenderData(MaterialInstanceRenderData const*);

	public:
		Material_GLTF_Unlit(std::string const& name);

		~Material_GLTF_Unlit() = default;

		void Destroy() override;


	public:
		void SetBaseColorFactor(glm::vec4 const&);


	public:
		static bool ShowImGuiWindow(MaterialInstanceRenderData&); // Returns true if data was modified


	private:
		void PackMaterialParamsData(std::span<std::byte> data) const override;


	private:
		UnlitData GetUnlitData() const;


	private:
		// GLTF Unlit properties:
		glm::vec4 m_baseColorFactor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	};


	inline bool Material_GLTF_Unlit::FilterRenderData(MaterialInstanceRenderData const* renderData)
	{
		SEAssert(renderData, "Render data pointer is null");
		return gr::Material::EffectIDToMaterialID(renderData->m_effectID) == gr::Material::GLTF_Unlit;
	}


	inline void Material_GLTF_Unlit::SetBaseColorFactor(glm::vec4 const& baseColorFactor)
	{
		m_baseColorFactor = baseColorFactor;
	}
}