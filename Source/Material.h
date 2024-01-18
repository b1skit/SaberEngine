// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "NamedObject.h"


namespace re
{
	class ParameterBlock;
	class Texture;
	class Sampler;
	class Shader;
}

namespace gr
{
	class Material : public virtual en::NamedObject
	{
	public:
		struct TextureSlotDesc
		{
			std::shared_ptr<re::Texture> m_texture = nullptr;
			std::shared_ptr<re::Sampler> m_samplerObject = nullptr; // eg. Sampler object from the sampler library
			std::string m_shaderSamplerName;
		};


	public:
		enum class MaterialType
		{
			GLTF_PBRMetallicRoughness, // A metallic-roughness material model from PBR methodology
		};
		enum class AlphaMode
		{
			Opaque,
			Clip,
			AlphaBlended
		};
		enum class DoubleSidedMode
		{
			SingleSided,
			DoubleSided
		};

		struct RenderData
		{
			// For now, just point to the scene data...
			gr::Material const* m_material;
		};


	public:
		[[nodiscard]] static std::shared_ptr<gr::Material> Create(std::string const& name, MaterialType);

		template <typename T>
		T GetAs(); // Get the Material as a dynamic cast to a derrived type

		Material(Material&&) = default;
		Material& operator=(Material&&) = default;

		virtual ~Material() = 0;

		void SetTexture(uint32_t slotIndex, std::shared_ptr<re::Texture>);
		re::Texture const* GetTexture(uint32_t slotIndex) const;
		re::Texture const* GetTexture(std::string const& samplerName) const;
		std::vector<TextureSlotDesc> const& GetTexureSlotDescs() const;

		void SetAlphaMode(AlphaMode);
		void SetAlphaCutoff(float alphaCutoff);
		void SetDoubleSidedMode(DoubleSidedMode);

		MaterialType GetMaterialType() const;

		static std::shared_ptr<re::ParameterBlock> CreateParameterBlock(gr::Material const*);

		bool IsDirty() const;
		void MarkClean();


		virtual void ShowImGuiWindow();


	protected:
		const MaterialType m_materialType;


	protected: // Must be populated by the child class:
		std::vector<TextureSlotDesc> m_texSlots; // Vector index == shader binding index
		std::unordered_map<std::string, uint32_t> m_namesToSlotIndex;


	protected:
		AlphaMode m_alphaMode = AlphaMode::Opaque;
		float m_alphaCutoff = 0.5f;
		DoubleSidedMode m_doubleSidedMode = DoubleSidedMode::SingleSided;

		bool m_isDirty;


	protected:
		Material(std::string const& name, MaterialType); // Use the CreateMaterial 


	private:
		Material() = delete;
		Material(Material const&) = delete;
		Material& operator=(Material const&) = delete;
	};


	template <typename T>
	inline T Material::GetAs()
	{
		return dynamic_cast<T>(this);
	}


	inline void Material::SetTexture(uint32_t slotIndex, std::shared_ptr<re::Texture> texture)
	{
		SEAssert(slotIndex < m_texSlots.size(), "Out of bounds slot index");
		m_texSlots[slotIndex].m_texture = texture;
	}


	inline re::Texture const* Material::GetTexture(uint32_t slotIndex) const
	{
		return m_texSlots[slotIndex].m_texture.get();
	}


	inline std::vector<Material::TextureSlotDesc> const& Material::GetTexureSlotDescs() const
	{
		return m_texSlots;
	}


	inline void Material::SetAlphaMode(AlphaMode alphaMode)
	{
		SEAssert(alphaMode == AlphaMode::Opaque, "TODO: Support other alpha modes");

		m_alphaMode = alphaMode;
		m_isDirty = true;
	}


	inline void Material::SetAlphaCutoff(float alphaCutoff)
	{
		m_alphaCutoff = alphaCutoff;
		m_isDirty = true;
	}


	inline void Material::SetDoubleSidedMode(DoubleSidedMode doubleSidedMode)
	{
		SEAssert(doubleSidedMode == DoubleSidedMode::SingleSided, "TODO: Support other sided modes");

		m_doubleSidedMode = doubleSidedMode;
		m_isDirty = true;
	}


	inline Material::MaterialType Material::GetMaterialType() const
	{
		return m_materialType;
	}


	inline bool Material::IsDirty() const
	{
		return m_isDirty;
	}


	inline void Material::MarkClean()
	{
		m_isDirty = false;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Material::~Material() {};
}


