// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "NamedObject.h"
#include "ParameterBlock.h"


namespace re
{
	class Texture;
	class Sampler;
	class Shader;
}

namespace gr
{
	class Material : public virtual en::NamedObject
	{
	public:
		enum class MaterialType
		{
			GLTF_PBRMetallicRoughness, // GLTF 2.0's PBR metallic-roughness material model
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


	public:
		struct TextureSlotDesc
		{
			std::shared_ptr<re::Texture> m_texture = nullptr;
			std::shared_ptr<re::Sampler> m_samplerObject = nullptr; // eg. Sampler object from the sampler library
			std::string m_shaderSamplerName;
		};


	public:
		static constexpr uint8_t k_numTexInputs = 8;
		static constexpr size_t k_shaderSamplerNameLength = 64; // Arbitrary: Includes null terminator
		static constexpr size_t k_paramDataBlockByteSize = 64; // Arbitrary: Max current material size

		// Material render data:
		struct MaterialInstanceData
		{
			std::array<re::Texture const*, gr::Material::k_numTexInputs> m_textures;
			std::array<re::Sampler const*, gr::Material::k_numTexInputs> m_samplers;
			char m_shaderSamplerNames[gr::Material::k_numTexInputs][gr::Material::k_shaderSamplerNameLength];

			AlphaMode m_alphaMode;
			float m_alphaCutoff;
			DoubleSidedMode m_doubleSidedMode;

			// Material implementations pack their specific parameter data into this block of bytes
			std::array<uint8_t, gr::Material::k_paramDataBlockByteSize> m_materialParamData;

			// Material metadata:
			gr::Material::MaterialType m_type;
			char m_materialName[k_shaderSamplerNameLength];
			uint64_t m_materialUniqueID;
		};

	public:
		static std::shared_ptr<re::ParameterBlock> CreateInstancedParameterBlock(
			re::ParameterBlock::PBType,
			std::vector<MaterialInstanceData const*> const&);

		// Convenience helper: Partially update elements of an already committed (mutable) parameter block
		static void CommitMaterialInstanceData(
			re::ParameterBlock*, MaterialInstanceData const*, uint32_t baseOffset);

		static bool ShowImGuiWindow(MaterialInstanceData&); // Returns true if data was modified


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

		void PackMaterialInstanceData(MaterialInstanceData&) const;


	private:
		void PackMaterialInstanceTextureSlotDescs(re::Texture const**, re::Sampler const**, char[][k_shaderSamplerNameLength]) const;
		
		virtual void PackMaterialInstanceData(void*, size_t maxSize) const = 0;


	protected:
		const MaterialType m_materialType;


	protected: // Must be populated by the child class:
		std::vector<TextureSlotDesc> m_texSlots; // Vector index == shader binding index
		std::unordered_map<std::string, uint32_t> m_namesToSlotIndex;


	protected:
		AlphaMode m_alphaMode = AlphaMode::Opaque;
		float m_alphaCutoff = 0.5f;
		DoubleSidedMode m_doubleSidedMode = DoubleSidedMode::SingleSided;


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
	}


	inline void Material::SetAlphaCutoff(float alphaCutoff)
	{
		m_alphaCutoff = alphaCutoff;
	}


	inline void Material::SetDoubleSidedMode(DoubleSidedMode doubleSidedMode)
	{
		SEAssert(doubleSidedMode == DoubleSidedMode::SingleSided, "TODO: Support other sided modes");

		m_doubleSidedMode = doubleSidedMode;
	}


	inline Material::MaterialType Material::GetMaterialType() const
	{
		return m_materialType;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Material::~Material() {};
}


