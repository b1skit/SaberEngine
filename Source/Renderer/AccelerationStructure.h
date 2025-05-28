// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "BufferView.h"
#include "Effect.h"
#include "RenderObjectIDs.h"
#include "VertexStream.h"

#include "Core/Assert.h"
#include "Core/InvPtr.h"

#include "Core/Interfaces/IPlatformObject.h"
#include "Core/Interfaces/INamedObject.h"

#include "Generated/DrawStyles.h"


namespace re
{
	class AccelerationStructure;
	class Buffer;


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
		struct Geometry
		{
			Geometry(gr::RenderDataID);

			void SetVertexPositions(re::VertexBufferInput const& positions);
			re::VertexBufferInput const& GetVertexPositions() const;

			void SetVertexIndices(core::InvPtr<gr::VertexStream> const& indices);
			core::InvPtr<gr::VertexStream> const& GetVertexIndices() const;

			void SetGeometryFlags(GeometryFlags geometryFlags);
			GeometryFlags GetGeometryFlags() const;
			
			gr::RenderDataID GetRenderDataID() const;

			void SetEffectID(EffectID effectID);
			EffectID GetEffectID() const;
			
			void SetDrawstyleBits(effect::drawstyle::Bitmask drawstyleBits);
			effect::drawstyle::Bitmask GetDrawstyleBits() const;
			
			void RegisterResource(core::InvPtr<gr::VertexStream> const&);
			void RegisterResource(re::VertexBufferInput const&);

			// Note: For gr::VertexStream::Type::Index, setIdx 0 = 16 bit, setIdx 1 = 32 bit
			ResourceHandle GetResourceHandle(gr::VertexStream::Type, uint8_t setIdx = 0) const;


		private:
			re::VertexBufferInput m_positions; // Respects buffer overrides
			core::InvPtr<gr::VertexStream> m_indices; // Can be null/invalid

			// We pack the VertexStreamMetadata the same way as vertex streams in MeshPrimitive::RenderData: 
			// Streams of the same type are packed contiguously, in monotonically-increasing set order. Stream types are
			// packed in the same order as the gr::VertexStream types are declared
			struct VertexStreamMetadata
			{
				ResourceHandle m_resourceHandle = INVALID_RESOURCE_IDX;
				gr::VertexStream::Type m_streamType = gr::VertexStream::Type::Type_Count;
				uint8_t m_setIndex = 0;
			};
			std::array<VertexStreamMetadata, gr::VertexStream::k_maxVertexStreams> m_vertexStreamMetadata{};

			// SaberEngine supports 16 and 32 bit uint index streams, we abuse the set index here to differentiate them:
			VertexStreamMetadata m_indexStream16BitMetadata{}; // setIdx = 0
			VertexStreamMetadata m_indexStream32BitMetadata{}; // setIdx = 1

			void RegisterResourceInternal(ResourceHandle resolvedResourceHandle, gr::VertexStream::Type, re::DataType);

			GeometryFlags m_geometryFlags = GeometryFlags::GeometryFlags_None;

			gr::RenderDataID m_renderDataID = gr::k_invalidRenderDataID;

			// Effect ID and material drawstyle bits allow us to resolve a Technique from BLAS geometry
			EffectID m_effectID;
			effect::drawstyle::Bitmask m_drawstyleBits = 0;


		private:
			Geometry() = delete;
		};


	public:
		struct IASParams
		{
			virtual ~IASParams() = 0;

			BuildFlags m_buildFlags = BuildFlags_None;
		};
		struct BLASParams : public virtual IASParams
		{
			// 3x4 row-major world matrix: Applied to all BLAS geometry
			glm::mat3x4 m_blasWorldMatrix = glm::mat3x4(1.f);

			std::vector<Geometry> m_geometry;

			std::shared_ptr<re::Buffer> m_transform; // Buffer of mat3x4 in row-major order. Indexes correspond with m_geometry

			InclusionMask m_instanceMask = InstanceInclusionMask_Always; // Visibility mask: 0 = ignored, 1 = visible
			InstanceFlags m_instanceFlags = InstanceFlags::InstanceFlags_None;
		};
		struct TLASParams : public virtual IASParams
		{
			std::vector<std::shared_ptr<re::AccelerationStructure>> m_blasInstances;

			ResourceHandle GetResourceHandle() const;
			re::BufferInput const& GetBindlessVertexStreamLUT() const;


		private: // Populated internally:
			friend class AccelerationStructure;
			re::BufferInput m_bindlessResourceLUT; // BLAS instances -> bindless resource LUT

			ResourceHandle m_srvTLASResourceHandle = INVALID_RESOURCE_IDX;
		};


	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual void Destroy() override = 0;

			bool m_isBuilt = false; // true after first build recorded to a command list
		};


	public:
		static std::shared_ptr<AccelerationStructure> CreateBLAS(char const* name, std::unique_ptr<BLASParams>&&);
		static std::shared_ptr<AccelerationStructure> CreateTLAS(char const* name, std::unique_ptr<TLASParams>&&);


	public:
		AccelerationStructure() = default;
		AccelerationStructure(AccelerationStructure&&) noexcept = default;
		AccelerationStructure& operator=(AccelerationStructure&&) noexcept = default;

		~AccelerationStructure();


	public:
		void Destroy();

		PlatObj* GetPlatformObject() const;


	public:
		IASParams const* GetASParams() const;
		void UpdateASParams(std::unique_ptr<IASParams>&&); // Update the ASParams (e.g. when updating/refitting an AS)

		Type GetType() const;

		ResourceHandle GetResourceHandle() const;
		re::BufferInput const& GetBindlessVertexStreamLUT() const;


	private:
		AccelerationStructure(char const* name, Type, std::unique_ptr<IASParams>&&); // Use Create() instead


	private:
		std::unique_ptr<PlatObj> m_platObj;
		std::unique_ptr<IASParams> m_asParams;
		Type m_type;


	private: // No copies allowed
		AccelerationStructure(AccelerationStructure const&) = delete;
		AccelerationStructure& operator=(AccelerationStructure const&) = delete;
	};


	inline re::AccelerationStructure::IASParams::~IASParams() {} // Pure virtual: Must provide an impl


	inline AccelerationStructure::PlatObj* AccelerationStructure::GetPlatformObject() const
	{
		return m_platObj.get();
	}


	inline AccelerationStructure::IASParams const* AccelerationStructure::GetASParams() const
	{
		return m_asParams.get();
	}


	inline void AccelerationStructure::UpdateASParams(std::unique_ptr<IASParams>&& asParams)
	{
		SEAssert(m_platObj->m_isBuilt, "Setting ASParams on an AS that has not yet been built. This is unexpected");

		m_asParams = std::move(asParams);
	}


	inline AccelerationStructure::Type AccelerationStructure::GetType() const
	{
		return m_type;
	}


	inline ResourceHandle AccelerationStructure::GetResourceHandle() const
	{
		SEAssert(m_type == re::AccelerationStructure::Type::TLAS, "Only a TLAS has a bindless resource handle");
		
		re::AccelerationStructure::TLASParams* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams*>(m_asParams.get());
		SEAssert(tlasParams, "Failed to cast to TLASParams");

		return tlasParams->GetResourceHandle();
	}


	inline re::BufferInput const& AccelerationStructure::GetBindlessVertexStreamLUT() const
	{
		SEAssert(m_type == re::AccelerationStructure::Type::TLAS, "Only a TLAS has a bindless resource handle");

		re::AccelerationStructure::TLASParams* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams*>(m_asParams.get());
		SEAssert(tlasParams, "Failed to cast to TLASParams");

		return tlasParams->GetBindlessVertexStreamLUT();
	}


	// ---


	inline AccelerationStructure::Geometry::Geometry(gr::RenderDataID renderDataID)
		: m_renderDataID(renderDataID)
	{
	}


	inline void AccelerationStructure::Geometry::SetVertexPositions(re::VertexBufferInput const& positions)
	{
		m_positions = positions;
		RegisterResource(m_positions);
	}


	inline re::VertexBufferInput const& AccelerationStructure::Geometry::GetVertexPositions() const
	{
		return m_positions;
	}


	inline void AccelerationStructure::Geometry::SetVertexIndices(core::InvPtr<gr::VertexStream> const& indices)
	{
		m_indices = indices;

		if (m_indices)
		{
			RegisterResource(m_indices);
		}
	}


	inline core::InvPtr<gr::VertexStream> const& AccelerationStructure::Geometry::GetVertexIndices() const
	{
		return m_indices;
	}


	inline void AccelerationStructure::Geometry::SetGeometryFlags(GeometryFlags geometryFlags)
	{
		m_geometryFlags = geometryFlags;
	}


	inline AccelerationStructure::GeometryFlags AccelerationStructure::Geometry::GetGeometryFlags() const
	{
		return m_geometryFlags;
	}


	inline gr::RenderDataID AccelerationStructure::Geometry::GetRenderDataID() const
	{
		SEAssert(m_renderDataID != gr::k_invalidRenderDataID, "Invalid RenderDataID");
		return m_renderDataID;
	}


	inline void AccelerationStructure::Geometry::SetEffectID(EffectID effectID)
	{
		m_effectID = effectID;
	}


	inline EffectID AccelerationStructure::Geometry::GetEffectID() const
	{
		return m_effectID;
	}


	inline void AccelerationStructure::Geometry::SetDrawstyleBits(effect::drawstyle::Bitmask drawstyleBits)
	{
		m_drawstyleBits = drawstyleBits;
	}


	inline effect::drawstyle::Bitmask AccelerationStructure::Geometry::GetDrawstyleBits() const
	{
		return m_drawstyleBits;
	}
}