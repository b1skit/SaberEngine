// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"

#include "Core\Interfaces\INamedObject.h"


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
		enum MaterialType : uint32_t
		{
			GLTF_PBRMetallicRoughness, // GLTF 2.0's PBR metallic-roughness material model

			MaterialType_Count
		};
		SEStaticAssert(MaterialType_Count < std::numeric_limits<uint32_t>::max(), "Too many material types");

		static constexpr char const* k_materialTypeNames[] =
		{
			ENUM_TO_STR(GLTF_PBRMetallicRoughness)
		};
		SEStaticAssert(_countof(k_materialTypeNames) == MaterialType_Count, "Names and enum are out of sync");


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
		};


	public:
		static constexpr uint8_t k_numTexInputs = 8;
		static constexpr size_t k_shaderSamplerNameLength = 64; // Arbitrary: Includes null terminator
		static constexpr size_t k_paramDataBlockByteSize = 80; // Arbitrary: Max current material size

		// Material render data:
		struct MaterialInstanceData
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
			gr::Material::MaterialType m_type;
			char m_materialName[k_shaderSamplerNameLength];
			uint64_t m_srcMaterialUniqueID;
		};
		

	public:
		static std::shared_ptr<re::Buffer> CreateInstancedBuffer(
			re::Buffer::Type,
			std::vector<MaterialInstanceData const*> const&);

		static std::shared_ptr<re::Buffer> ReserveInstancedBuffer(MaterialType, uint32_t maxInstances);

		// Convenience helper: Partially update elements of an already committed (mutable) buffer
		static void CommitMaterialInstanceData(re::Buffer*, MaterialInstanceData const*, uint32_t baseOffset);

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
		void SetDoubleSidedMode(bool);

		void SetShadowCastMode(bool);

		MaterialType GetMaterialType() const;

		void InitializeMaterialInstanceData(MaterialInstanceData&) const;


	private:
		void PackMaterialInstanceTextureSlotDescs(
			re::Texture const**, re::Sampler const**, char[][k_shaderSamplerNameLength]) const;
		
		virtual void PackMaterialParamsData(void*, size_t maxSize) const = 0;


	protected:
		const MaterialType m_materialType;


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


	inline void Material::SetShadowCastMode(bool castsShadow)
	{
		m_isShadowCaster = castsShadow;
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


	inline Material::MaterialType Material::GetMaterialType() const
	{
		return m_materialType;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Material::~Material() {};
}


