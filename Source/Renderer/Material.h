// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "AccelerationStructure.h"
#include "Effect.h"
#include "RenderObjectIDs.h"
#include "Sampler.h"
#include "Texture.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"


namespace gr
{
	class RenderDataManager;


	class Material : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		enum MaterialID : uint8_t
		{
			GLTF_Unlit					= 0, // GLTF 2.0: KHR_materials_unlit
			GLTF_PBRMetallicRoughness	= 1, // GLTF 2.0: PBR metallic-roughness material model

			MaterialID_Count
		};
		SEStaticAssert(MaterialID_Count < std::numeric_limits<uint8_t>::max(), "Too many MaterialIDs");

		// Note: Material Buffer names are used to associate Effects with Buffers (e.g. when building batches)
		static constexpr char const* k_materialNames[] =
		{
			ENUM_TO_STR(GLTF_Unlit),
			ENUM_TO_STR(GLTF_PBRMetallicRoughness),
		};
		SEStaticAssert(_countof(k_materialNames) == MaterialID_Count, "Names and enum are out of sync");

		static MaterialID EffectIDToMaterialID(EffectID);


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
			"Clip",
			"Blend"
		};
		SEStaticAssert(_countof(k_alphaModeNames) == static_cast<uint8_t>(gr::Material::AlphaMode::AlphaMode_Count),
			"Alpha modes and names are out of sync");


	public:
		struct TextureSlotDesc
		{
			core::InvPtr<re::Texture> m_texture;
			core::InvPtr<re::Sampler> m_samplerObject;
			std::string m_shaderSamplerName;
			uint8_t m_uvChannelIdx = 0;
		};


	public:
		static constexpr uint8_t k_numTexInputs = 8;
		static constexpr size_t k_shaderSamplerNameLength = 64; // Arbitrary: Includes null terminator
		static constexpr size_t k_paramDataBlockByteSize = 128; // Arbitrary: Max current material size

		// Material render data:
		struct MaterialInstanceRenderData
		{
			std::array<core::InvPtr<re::Texture>, gr::Material::k_numTexInputs> m_textures;
			std::array<core::InvPtr<re::Sampler>, gr::Material::k_numTexInputs> m_samplers;
			char m_shaderSamplerNames[gr::Material::k_numTexInputs][gr::Material::k_shaderSamplerNameLength];

			// Material implementations must pack *all* buffer data into this block of bytes (i.e what the GPU consumes)
			std::array<uint8_t, gr::Material::k_paramDataBlockByteSize> m_materialParamData;

			// Material flags. Note: This data is NOT sent to the GPU
			AlphaMode m_alphaMode = AlphaMode::AlphaMode_Count;
			bool m_isDoubleSided = false;
			bool m_isShadowCaster = true;

			// Material metadata:
			EffectID m_effectID;
			char m_materialName[k_shaderSamplerNameLength];
			uint64_t m_srcMaterialUniqueID;


			// Helpers: Get the drawstyle bits for a material instance:
			static effect::drawstyle::Bitmask GetDrawstyleBits(gr::Material::MaterialInstanceRenderData const*);

			// Helper: Create an 8-bit ray tracing acceleration structure geometry instance inclusion mask
			static uint8_t CreateInstanceInclusionMask(gr::Material::MaterialInstanceRenderData const*);

			// Helper: Registers all resources types on the MeshPrimitive RenderData with an AccelerationStructure
			static void RegisterGeometryResources(
				MaterialInstanceRenderData const&, re::AccelerationStructure::Geometry&);
		};

		template<typename T>
		static T GetInstancedMaterialData(
			gr::Material::MaterialInstanceRenderData const&, IDType, gr::RenderDataManager const&);


	public:
		static bool ShowImGuiWindow(MaterialInstanceRenderData&); // Returns true if data was modified


	public:
		template <typename T>
		T GetAs(); // Get the Material as a dynamic cast to a derrived type

		Material(Material&&) noexcept = default;
		Material& operator=(Material&&) noexcept = default;

		virtual ~Material() = default;

		virtual void Destroy() = 0;

		void SetTexture(uint32_t slotIndex, core::InvPtr<re::Texture> const&, uint8_t uvChannelIdx);
		core::InvPtr<re::Texture> GetTexture(uint32_t slotIndex) const;
		core::InvPtr<re::Texture> GetTexture(std::string const& samplerName) const;
		std::vector<TextureSlotDesc> const& GetTexureSlotDescs() const;

		void SetAlphaMode(AlphaMode);
		void SetAlphaCutoff(float alphaCutoff);
		void SetDoubleSidedMode(bool);

		void SetShadowCastMode(bool);

		MaterialID GetMaterialType() const;
		EffectID GetEffectID() const;


		void InitializeMaterialInstanceData(MaterialInstanceRenderData&) const;


	private:
		void PackMaterialInstanceTextureSlotDescs(
			core::InvPtr<re::Texture>*, core::InvPtr<re::Sampler>*, char[][k_shaderSamplerNameLength]) const;
		
		virtual void PackMaterialParamsData(void*, size_t maxSize) const = 0;


	protected:
		const MaterialID m_materialID;
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
		Material(std::string const& name, MaterialID); // Use Create()


	private:
		Material() = delete;
		Material(Material const&) = delete;
		Material& operator=(Material const&) = delete;
	};


	template<typename T>
	static T Material::GetInstancedMaterialData(
		gr::Material::MaterialInstanceRenderData const& materialInstanceData, IDType, gr::RenderDataManager const&)
	{
		SEAssert(sizeof(T) <= gr::Material::k_paramDataBlockByteSize, "Requested type is too large");
		// TODO: We should assert that T is indeed what is packed in m_materialParamData

		return *reinterpret_cast<T const*>(materialInstanceData.m_materialParamData.data());
	}


	template <typename T>
	inline T Material::GetAs()
	{
		SEAssert(dynamic_cast<T>(this) != nullptr, "dynamic_cast failed");
		return dynamic_cast<T>(this);
	}


	inline void Material::SetTexture(uint32_t slotIndex, core::InvPtr<re::Texture> const& texture, uint8_t uvChannelIdx)
	{
		SEAssert(slotIndex < m_texSlots.size(), "Out of bounds slot index");
		SEAssert(uvChannelIdx <= 1, 
			"Only 2 UV channels are currently supported - Hitting this means shaders/effects must be updated");

		m_texSlots[slotIndex].m_texture = texture;
		m_texSlots[slotIndex].m_uvChannelIdx = uvChannelIdx;
	}

	
	inline core::InvPtr<re::Texture> Material::GetTexture(uint32_t slotIndex) const
	{
		return m_texSlots[slotIndex].m_texture;
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


	inline Material::MaterialID Material::GetMaterialType() const
	{
		return m_materialID;
	}


	inline EffectID Material::GetEffectID() const
	{
		return m_effectID;
	}
}


