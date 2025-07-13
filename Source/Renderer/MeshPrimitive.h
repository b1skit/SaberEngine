// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "AccelerationStructure.h"
#include "Buffer.h"
#include "RasterState.h"
#include "RenderObjectIDs.h"
#include "VertexStream.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/IPlatformObject.h"
#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"


namespace
{
	template<typename T>
	struct MeshPrimitiveFromCGLTF;
}

namespace core
{
	class Inventory; 
}

namespace gr
{


	class MeshPrimitive final : 
		public virtual core::INamedObject, 
		public virtual core::IHashedDataObject, 
		public virtual core::IUniqueID
	{
	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual ~PlatObj() = default;
		};


	public:
		struct MeshPrimitiveParams final
		{
			re::RasterState::PrimitiveTopology m_primitiveTopology = 
				re::RasterState::PrimitiveTopology::TriangleList;
		};

		struct MeshVertexStream final
		{
			core::InvPtr<re::VertexStream> m_vertexStream;
			uint8_t m_setIdx = 0; // Index of m_vertexStream, w.r.t other streams of the same type. Used for sorting
		};

		struct MeshVertexStreamComparisonData final
		{
			re::VertexStream::Type m_streamType;
			uint8_t m_setIdx;
		};
		struct MeshVertexStreamComparator final
		{
			bool operator()(MeshVertexStream const&, MeshVertexStream const&);
			bool operator()(MeshVertexStream const&, MeshVertexStreamComparisonData const&);
		};


	public:
		struct PackingMetadata final
		{
			uint8_t m_firstByteOffset;	// No. bytes from the start of the packing to 1st byte of this displacement
			uint8_t m_byteStride;		// No. bytes in 1 displacement (e.g. float3 = 12)
			uint8_t m_numComponents;	// No. components in 1 displacement (e.g. float3 = 3)
		};

		struct MorphTargetMetadata final
		{
			uint8_t m_maxMorphTargets;	// A vertex may either have 0 or m_maxMorphTargets, exactly
			uint32_t m_morphByteStride;	// Total bytes for all interleaved displacements for 1 vertex (e.g. from Vn to Vn+1)

			std::array<PackingMetadata, re::VertexStream::k_maxVertexStreams> m_perStreamMetadata;
		};

	public:
		struct RenderData final
		{
			MeshPrimitiveParams m_meshPrimitiveParams;

			std::array<core::InvPtr<re::VertexStream>, re::VertexStream::k_maxVertexStreams> m_vertexStreams;
			uint8_t m_numVertexStreams;

			core::InvPtr<re::VertexStream> m_indexStream;

			bool m_hasMorphTargets;
			std::shared_ptr<re::Buffer> m_interleavedMorphData;
			MorphTargetMetadata m_morphTargetMetadata;

			bool m_meshHasSkinning;

			uint64_t m_dataHash;

			gr::RenderDataID m_owningMeshRenderDataID; // Access owning MeshConcept's MeshMorphRenderData/SkinningRenderData


			// Helper: Get a specific vertex stream packed into a MeshPrimitive::RenderData.
			// If the setIdx index < 0, the first matching type is returned
			static core::InvPtr<re::VertexStream> GetVertexStreamFromRenderData(
				gr::MeshPrimitive::RenderData const&, re::VertexStream::Type, int8_t setIdx = 0);

			// Helper: Registers all resources types on the MeshPrimitive RenderData with an AccelerationStructure
			static void RegisterGeometryResources(
				gr::MeshPrimitive::RenderData const&, re::AccelerationStructure::Geometry&);
		};


		struct MeshMorphRenderData final
		{
			std::vector<float> m_morphTargetWeights;
		};


		struct SkinningRenderData final
		{
			std::vector<glm::mat4> m_jointTransforms;
			std::vector<glm::mat4> m_transposeInvJointTransforms;
		};


	public:
		[[nodiscard]] static core::InvPtr<MeshPrimitive> Create(
			std::string const& name,
			core::InvPtr<re::VertexStream> const& indexStream,
			std::vector<MeshVertexStream>&& vertexStreams,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		[[nodiscard]] static core::InvPtr<MeshPrimitive> Create(
			std::string const& name,
			std::vector<std::array<re::VertexStream::CreateParams, re::VertexStream::Type::Type_Count>>&&,
			gr::MeshPrimitive::MeshPrimitiveParams const&);

		MeshPrimitive(MeshPrimitive&& rhs) noexcept = default;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) noexcept = default;
		~MeshPrimitive() = default;

		void Destroy();
		
		MeshPrimitiveParams const& GetMeshParams() const;

		core::InvPtr<re::VertexStream> const& GetIndexStream() const;
		
		core::InvPtr<re::VertexStream> const& GetVertexStream(re::VertexStream::Type, uint8_t srcTypeIdx) const;
		std::vector<MeshVertexStream> const& GetVertexStreams() const;

		bool HasMorphTargets() const;
		std::shared_ptr<re::Buffer> GetInterleavedMorphDataBuffer() const;
		MorphTargetMetadata const& GetMorphTargetMetadata() const;

		void ShowImGuiWindow() const;


	private:		
		MeshPrimitiveParams m_params;

		core::InvPtr<re::VertexStream> m_indexStream;
		std::vector<MeshVertexStream> m_vertexStreams;	

		std::shared_ptr<re::Buffer> m_interleavedMorphData;
		MorphTargetMetadata m_interleavedMorphMetadata;


		void ComputeDataHash() override;


	private: // Private ctor: Use the Create factory instead
		MeshPrimitive(char const* name,
			core::InvPtr<re::VertexStream> const& indexStream,
			std::vector<MeshVertexStream>&& vertexStreams,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		MeshPrimitive(char const* name,
			std::vector<std::array<re::VertexStream::CreateParams, re::VertexStream::Type::Type_Count>>&&,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);


	private: // No copying allowed
		MeshPrimitive() = delete;
		MeshPrimitive(MeshPrimitive const& rhs) = delete;
		MeshPrimitive& operator=(MeshPrimitive const& rhs) = delete;


	private:
		template<typename T>
		friend struct MeshPrimitiveFromCGLTF;
	};


	inline MeshPrimitive::MeshPrimitiveParams const& MeshPrimitive::GetMeshParams() const
	{
		return m_params;
	}


	inline core::InvPtr<re::VertexStream> const& MeshPrimitive::GetIndexStream() const
	{
		return m_indexStream;
	}


	inline std::vector<MeshPrimitive::MeshVertexStream> const& MeshPrimitive::GetVertexStreams() const
	{
		return m_vertexStreams;
	}


	inline bool MeshPrimitive::HasMorphTargets() const
	{
		return m_interleavedMorphData != nullptr;
	}


	inline std::shared_ptr<re::Buffer> MeshPrimitive::GetInterleavedMorphDataBuffer() const
	{
		return m_interleavedMorphData;
	}


	inline MeshPrimitive::MorphTargetMetadata const& MeshPrimitive::GetMorphTargetMetadata() const
	{
		return m_interleavedMorphMetadata;
	}


	inline bool MeshPrimitive::MeshVertexStreamComparator::operator()(
		gr::MeshPrimitive::MeshVertexStream const& a, gr::MeshPrimitive::MeshVertexStream const& b)
	{
		if (a.m_vertexStream->GetType() == b.m_vertexStream->GetType())
		{
			return a.m_setIdx < b.m_setIdx;
		}
		return a.m_vertexStream->GetType() < b.m_vertexStream->GetType();
	}


	inline bool MeshPrimitive::MeshVertexStreamComparator::operator()(
		MeshPrimitive::MeshVertexStream const& a, MeshPrimitive::MeshVertexStreamComparisonData const& b)
	{
		if (a.m_vertexStream->GetType() == b.m_streamType)
		{
			return a.m_setIdx < b.m_setIdx;
		}
		return a.m_vertexStream->GetType() < b.m_streamType;
	}
}