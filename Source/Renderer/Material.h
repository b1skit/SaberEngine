// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "BufferInput.h"
#include "Effect.h"

#include "Core/Interfaces/INamedObject.h"


namespace re
{
	class Texture;
	class Sampler;
}

namespace gr
{
	class Material : public virtual core::INamedObject
	{
	public:
		enum MaterialEffect : uint32_t
		{
			GLTF_PBRMetallicRoughness, // GLTF 2.0's PBR metallic-roughness material model

			MaterialEffect_Count
		};
		SEStaticAssert(MaterialEffect_Count < std::numeric_limits<uint32_t>::max(), "Too many material types");

		static constexpr char const* k_materialEffectNames[] =
		{
			ENUM_TO_STR(GLTF_PBRMetallicRoughness)
		};
		SEStaticAssert(_countof(k_materialEffectNames) == MaterialEffect_Count, "Names and enum are out of sync");


	public:
		enum class AlphaMode : uint8_t
		{
			Opaque,
			Mask,
			Blend,
			AlphaMode_Count
		};
		static constexpr char const* k_alphaModeNames[] =
		{
			"Opaque",
			"Mask",
			"Blend"
		};
		SEStaticAssert(_countof(k_alphaModeNames) == static_cast<uint8_t>(gr::Material::AlphaMode::AlphaMode_Count),
			"Alpha modes and names are out of sync");


	public:
		struct TextureSlotDesc
		{
			std::shared_ptr<re::Texture> m_texture = nullptr;
			std::shared_ptr<re::Sampler> m_samplerObject = nullptr; // eg. Sampler object from the sampler library
			std::string m_shaderSamplerName;
			uint8_t m_uvChannelIdx = 0;
		};


	public:
		static constexpr uint8_t k_numTexInputs = 8;
		static constexpr size_t k_shaderSamplerNameLength = 64; // Arbitrary: Includes null terminator
		static constexpr size_t k_paramDataBlockByteSize = 96; // Arbitrary: Max current material size

		// Material render data:
		struct MaterialInstanceRenderData
		{
			std::array<re::Texture const*, gr::Material::k_numTexInputs> m_textures;
			std::array<re::Sampler const*, gr::Material::k_numTexInputs> m_samplers;
			char m_shaderSamplerNames[gr::Material::k_numTexInputs][gr::Material::k_shaderSamplerNameLength];

			// Material implementations must pack *all* buffer data into this block of bytes (i.e what the GPU consumes)
			std::array<uint8_t, gr::Material::k_paramDataBlockByteSize> m_materialParamData;

			// Material flags. Note: This data is NOT sent to the GPU
			AlphaMode m_alphaMode;
			bool m_isDoubleSided;
			bool m_isShadowCaster;

			// Material metadata:
			gr::Material::MaterialEffect m_matEffect;
			EffectID m_materialEffectID;
			char m_materialName[k_shaderSamplerNameLength];
			uint64_t m_srcMaterialUniqueID;
		};
		

	public:
		static re::BufferInput CreateInstancedBuffer(
			re::Buffer::AllocationType, std::vector<MaterialInstanceRenderData const*> const&);

		static re::BufferInput ReserveInstancedBuffer(MaterialEffect, uint32_t maxInstances);

		// Convenience helper: Partially update elements of an already committed (mutable) buffer
		static void CommitMaterialInstanceData(re::Buffer*, MaterialInstanceRenderData const*, uint32_t baseOffset);

		static bool ShowImGuiWindow(MaterialInstanceRenderData&); // Returns true if data was modified


	public:
		[[nodiscard]] static std::shared_ptr<gr::Material> Create(std::string const& name, MaterialEffect);

		template <typename T>
		T GetAs(); // Get the Material as a dynamic cast to a derrived type

		Material(Material&&) noexcept = default;
		Material& operator=(Material&&) noexcept = default;

		virtual ~Material() = 0;

		void SetTexture(uint32_t slotIndex, std::shared_ptr<re::Texture>, uint8_t uvChannelIdx);
		re::Texture const* GetTexture(uint32_t slotIndex) const;
		re::Texture const* GetTexture(std::string const& samplerName) const;
		std::vector<TextureSlotDesc> const& GetTexureSlotDescs() const;

		void SetAlphaMode(AlphaMode);
		void SetAlphaCutoff(float alphaCutoff);
		void SetDoubleSidedMode(bool);

		void SetShadowCastMode(bool);

		MaterialEffect GetMaterialType() const;
		EffectID GetMaterialEffectID() const;


		void InitializeMaterialInstanceData(MaterialInstanceRenderData&) const;


	private:
		void PackMaterialInstanceTextureSlotDescs(
			re::Texture const**, re::Sampler const**, char[][k_shaderSamplerNameLength]) const;
		
		virtual void PackMaterialParamsData(void*, size_t maxSize) const = 0;


	protected:
		const MaterialEffect m_materialEffect;
		const EffectID m_effectID;


	protected: // Must be populated by the child class:
		std::vector<TextureSlotDesc> m_texSlots; // Vector index == shader binding index
		std::unordered_map<std::string, uint32_t> m_namesToSlotIndex;


	protected:
		// Must be initialized with appropriate defaults by the child class:
		AlphaMode m_alphaMode;
		float m_alphaCutoff;
		bool m_isDoubleSided;
		bool m_isShadowCaster;


	protected:
		Material(std::string const& name, MaterialEffect); // Use the CreateMaterial 


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


	inline void Material::SetTexture(uint32_t slotIndex, std::shared_ptr<re::Texture> texture, uint8_t uvChannelIdx)
	{
		SEAssert(slotIndex < m_texSlots.size(), "Out of bounds slot index");
		SEAssert(uvChannelIdx <= 1, 
			"Only 2 UV channels are currently supported - Hitting this means shaders/effects must be updated");

		m_texSlots[slotIndex].m_texture = texture;
		m_texSlots[slotIndex].m_uvChannelIdx = uvChannelIdx;
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
		m_alphaMode = alphaMode;
	}


	inline void Material::SetAlphaCutoff(float alphaCutoff)
	{
		m_alphaCutoff = alphaCutoff;
	}


	inline void Material::SetDoubleSidedMode(bool isDoubleSided)
	{
		m_isDoubleSided = isDoubleSided;
	}


	inline void Material::SetShadowCastMode(bool isShadowCaster)
	{
		m_isShadowCaster = isShadowCaster;
	}


	inline Material::MaterialEffect Material::GetMaterialType() const
	{
		return m_materialEffect;
	}


	inline EffectID Material::GetMaterialEffectID() const
	{
		return m_effectID;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Material::~Material() {};
}


