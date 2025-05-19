// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Material.h"


struct UnlitData;

namespace gr
{
	class Material_GLTF_Unlit final : public virtual Material
	{
	public:
		Material_GLTF_Unlit(std::string const& name);

		~Material_GLTF_Unlit() = default;

		void Destroy() override;


	public:
		void SetBaseColorFactor(glm::vec4 const&);


	public:
		[[nodiscard]] static re::BufferInput CreateInstancedBuffer(
			re::Buffer::StagingPool, std::vector<MaterialInstanceRenderData const*> const&);

		static void CommitMaterialInstanceData(re::Buffer*, MaterialInstanceRenderData const*, uint32_t baseOffset);

		static bool ShowImGuiWindow(MaterialInstanceRenderData&); // Returns true if data was modified


	private:
		void PackMaterialParamsData(void*, size_t maxSize) const override;


	private:
		UnlitData GetUnlitData() const;


	private:
		// GLTF Unlit properties:
		glm::vec4 m_baseColorFactor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	};


	inline void Material_GLTF_Unlit::SetBaseColorFactor(glm::vec4 const& baseColorFactor)
	{
		m_baseColorFactor = baseColorFactor;
	}
}