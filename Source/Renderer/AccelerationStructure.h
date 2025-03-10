// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BufferView.h"
#include "Effect.h"
#include "VertexStream.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/IPlatformParams.h"
#include "Core/Interfaces/INamedObject.h"


namespace re
{
	class AccelerationStructure;


	struct ASInput
	{
		ASInput(char const* shaderName, std::shared_ptr<re::AccelerationStructure> const& as) // TLAS shader use
			: m_shaderName(shaderName)
			, m_accelerationStructure(as) 
		{}

		ASInput(std::shared_ptr<re::AccelerationStructure> const& as) // TLAS/BLAS Updates
			: ASInput("<Unnamed ASInput>", as)
		{}

		ASInput() = default;
		~ASInput() = default;
		ASInput(ASInput const&) = default;
		ASInput(ASInput&&) = default;
		ASInput& operator=(ASInput const&) = default;
		ASInput& operator=(ASInput&&) = default;


	public:
		std::string m_shaderName;
		std::shared_ptr<re::AccelerationStructure> m_accelerationStructure;
	};


	class AccelerationStructure : public virtual core::INamedObject
	{
	public:
		enum class Type : bool
		{
			TLAS,
			BLAS,
		};
		enum GeometryFlags : uint8_t
		{
			GeometryFlags_None			= 0,
			Opaque						= 1 << 0,
			NoDuplicateAnyHitInvocation = 1 << 1, // Guarantee the any hit shader will be executed exactly once
		};
		enum BuildFlags : uint8_t // Subset of D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS
		{
			BuildFlags_None = 0,
			AllowUpdate		= 1 << 0,
			AllowCompaction = 1 << 1,
			PreferFastTrace = 1 << 2,
			PreferFastBuild = 1 << 3,
			MinimizeMemory	= 1 << 4,
		};
		enum InstanceFlags : uint8_t
		{
			InstanceFlags_None				= 0,
			TriangleCullDisable				= 1 << 0,
			TriangleFrontCounterClockwise	= 1 << 1,
			ForceOpaque						= 1 << 2,
			ForceNonOpaque					= 1 << 3,
		};
		enum InclusionMask : uint8_t // Maximum 8 bits
		{
			// Acceleration structures register hits when the logical AND of the TraceRay() InstanceInclusionMask and
			// geometry InstanceMask is non-zero (i.e. if ANY bit matches)

			AlphaMode_Opaque	= 1 << 0,
			AlphaMode_Mask		= 1 << 1,
			AlphaMode_Blend		= 1 << 2,
			SingleSided			= 1 << 3,
			DoubleSided			= 1 << 4,
			NoShadow			= 1 << 5,
			ShadowCaster		= 1 << 6,

			InstanceInclusionMask_Always = 0xFF,
		};


	public:
		struct IASParams
		{
			virtual ~IASParams() = 0;

			BuildFlags m_buildFlags;
		};
		struct BLASParams : public virtual IASParams
		{
			// 3x4 row-major world matrix: Applied to all BLAS geometry
			glm::mat3x4 m_blasWorldMatrix = glm::mat3x4(1.f);

			struct Geometry
			{
				re::VertexBufferInput m_positions; // Respects buffer overrides
				core::InvPtr<gr::VertexStream> m_indices; // Can be null/invalid

				GeometryFlags m_geometryFlags = GeometryFlags::GeometryFlags_None;
				
				// Effect ID and material drawstyle bits allow us to resolve a Technique from BLAS geometry
				EffectID m_effectID;
				effect::drawstyle::Bitmask m_materialDrawstyleBits;
			};
			std::vector<Geometry> m_geometry;
			std::shared_ptr<re::Buffer> m_transform; // Buffer of mat3x4 in row-major order. Indexes correspond with m_geometry

			InclusionMask m_instanceMask = InstanceInclusionMask_Always; // Visibility mask: 0 = ignored, 1 = visible
			InstanceFlags m_instanceFlags = InstanceFlags::InstanceFlags_None;
		};
		struct TLASParams : public virtual IASParams
		{
			std::vector<std::shared_ptr<re::AccelerationStructure>> m_blasInstances;
		};


	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual void Destroy() override = 0;

			bool m_isBuilt = false; // true after first build recorded to a command list
		};


	public:
		static std::shared_ptr<AccelerationStructure> CreateBLAS(
			char const* name,
			std::unique_ptr<BLASParams>&& blasCreateParams);

		static std::shared_ptr<AccelerationStructure> CreateTLAS(
			char const* name,
			std::unique_ptr<TLASParams>&& blasCreateParams);


	public:
		AccelerationStructure() = default;
		AccelerationStructure(AccelerationStructure&&) noexcept = default;
		AccelerationStructure& operator=(AccelerationStructure&&) noexcept = default;

		~AccelerationStructure();


	public:
		void Create();
		void Destroy();

		PlatformParams* GetPlatformParams() const;


	public:
		IASParams const* GetASParams() const;
		void UpdateASParams(std::unique_ptr<IASParams>&&); // Update the ASParams (e.g. when updating/refitting an AS)

		Type GetType() const;


	private:
		AccelerationStructure(char const* name, Type, std::unique_ptr<IASParams>&&); // Use Create() instead


	private:
		std::unique_ptr<PlatformParams> m_platformParams;
		std::unique_ptr<IASParams> m_asParams;
		Type m_type;


	private: // No copies allowed
		AccelerationStructure(AccelerationStructure const&) = delete;
		AccelerationStructure& operator=(AccelerationStructure const&) = delete;
	};


	inline re::AccelerationStructure::IASParams::~IASParams() {} // Pure virtual: Must provide an impl


	inline AccelerationStructure::PlatformParams* AccelerationStructure::GetPlatformParams() const
	{
		return m_platformParams.get();
	}


	inline AccelerationStructure::IASParams const* AccelerationStructure::GetASParams() const
	{
		return m_asParams.get();
	}


	inline void AccelerationStructure::UpdateASParams(std::unique_ptr<IASParams>&& asParams)
	{
		SEAssert(m_platformParams->m_isBuilt, "Setting ASParams on an AS that has not yet been built. This is unexpected");

		m_asParams = std::move(asParams);
	}


	inline AccelerationStructure::Type AccelerationStructure::GetType() const
	{
		return m_type;
	}
}