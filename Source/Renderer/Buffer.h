// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "EnumTypes.h"
#include "VertexStream.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IPlatformParams.h"
#include "Core/Interfaces/IUniqueID.h"


namespace re
{
	/*******************************************************************************************************************
	* Buffers have 2 allocation types:
	* 1) Mutable:		Can be modified, and are rebuffered when modification is detected
	* 2) Immutable:		Buffered once at creation, and cannot be modified
	*
	* Buffers have 2 lifetime scopes:
	* 1) Permanent:		Allocated once, and held for the lifetime of their scope
	* 2) Single frame:	Allocated and destroyed within a single frame
	*					-> Single frame buffers are immutable once they are committed
	*
	* The union of these gives us Permanent Mutable, Permanent Immutable, & SingleFrame Immutable Buffer types
	*******************************************************************************************************************/

	class Buffer : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		struct BufferParams;


		enum AllocationType : uint8_t
		{
			Mutable,		// Permanent, N frames of buffers allocated to allow updates
			Immutable,		// Permanent, can only be modified by the GPU
			SingleFrame,	// Single frame, immutable once committed by the CPU

			AllocationType_Invalid
		};

		enum Usage : uint8_t
		{
			Constant		= 1 << 0,
			Structured		= 1 << 1,
			VertexStream	= 1 << 2, // Vertex/index buffers

			Invalid			= 0
		};
		using UsageMask = uint8_t;
		static bool HasUsageBit(Usage, UsageMask);
		static bool HasUsageBit(Usage, re::Buffer::BufferParams const&);
		static bool HasUsageBit(Usage, re::Buffer const&);


		enum MemoryPoolPreference : uint8_t
		{
			DefaultHeap,	// Prefer L1/VRAM. No CPU access
			UploadHeap,		// Prefor L0/SysMem. Intended for CPU -> GPU communication
		};

		enum Access : uint8_t
		{
			GPURead		= 1 << 0,	// Default
			GPUWrite	= 1 << 1,	// Default heap & Buffer::AllocationType::Immutable only (DX12: UAV, OpenGL: SSBO)
			CPURead		= 1 << 2,	// CPU readback from the GPU
			CPUWrite	= 1 << 3,	// CPU-mappable for writing. Upload heap only
			//ReBAR		= 1 << 4,	// TODO
		};
		using AccessMask = uint8_t;
		static bool HasAccessBit(Access, AccessMask);
		static bool HasAccessBit(Access, re::Buffer::BufferParams const&);
		static bool HasAccessBit(Access, re::Buffer const&);


		struct BufferParams
		{
			AllocationType m_allocationType = AllocationType::AllocationType_Invalid;
			MemoryPoolPreference m_memPoolPreference = MemoryPoolPreference::DefaultHeap;
			AccessMask m_accessMask = Access::GPURead;
			UsageMask m_usageMask = Usage::Invalid;

			uint32_t m_arraySize = 1; // Must be 1 for constant buffers, and vertex/index streams

			struct StreamType
			{
				re::VertexStream::Type m_type;
				re::DataType m_dataType;
				bool m_isNormalized;
				uint8_t m_stride;
			} m_vertexStreamParams{};
		};


	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_isCommitted = false; // Has an initial data commitment been made?
			bool m_isCreated = false; // Has the buffer been created at the API level?
		};


	public: // Buffer factories:

		// Create any type of buffer:
		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::Buffer> Create(
			std::string const& bufferName, T const* dataArray, BufferParams const&);

		// Create a read-only buffer for a single data object (eg. stage buffer)
		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::Buffer> Create(
			std::string const& bufferName, T const& data, BufferParams const&);

		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::Buffer> CreateUncommitted(
			std::string const& bufferName, BufferParams const&);

		// Create a read-only buffer for an array of several objects of the same type (eg. instanced mesh matrices)
		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::Buffer> CreateArray(
			std::string const& bufferName, T const* dataArray, BufferParams const&);

		// Create a read-only buffer, but defer the initial commit
		template<typename T>
		[[nodiscard]] static std::shared_ptr<re::Buffer> CreateUncommittedArray(
			std::string const& bufferName, BufferParams const&);

		// Create a buffer with void type. Useful for when the contents are not known (e.g. VertexStreams).
		// Risky - this intentionally avoids type checking
		[[nodiscard]] static std::shared_ptr<re::Buffer> Create(
			std::string const& bufferName, void const* dataArray, uint32_t numBytes, BufferParams const&);

	public:
		Buffer(Buffer&&) noexcept = default;
		Buffer& operator=(Buffer&&) noexcept = default;
		~Buffer();

		void Destroy();


	public:
		template <typename T>
		void Commit(T const& data); // Commit *updated* data
		
		template <typename T>
		void Commit(T const* data, uint32_t baseIdx, uint32_t numElements); // Recommit mutable array data (only)
	
		void const* GetData() const;
		void GetDataAndSize(void const** out_data, uint32_t* out_numBytes) const;
		uint32_t GetTotalBytes() const;
		uint32_t GetStride() const;
		AllocationType GetAllocationType() const;
		uint8_t GetUsageMask() const;

		uint32_t GetArraySize() const; // Instanced buffers: How many instances of data does the buffer hold?

		BufferParams const& GetBufferParams() const;

		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<PlatformParams> params) { m_platformParams = std::move(params); }


		// CPU readback:
	public:
		// Mapped data is always read back from the final results written during the previous frame. When there are > 2
		// frames in flight (which is possible in DX12), the immediately previous frame can be read by specifying a 
		// frame latency of 1, at the cost of increasing the chance the CPU will be blocked until the GPU to finishes.

		// Sentinel value: Use the default (i.e. max) frame latency when performing GPU readback. This is the most
		// performant, but the data accessed is (numFramesInFlight - 1) frames old
		static constexpr uint8_t k_maxFrameLatency = std::numeric_limits<uint8_t>::max();
		
		// This function may return nullptr if no mapped data exists (e.g. current frame # < frameLatency). If so, 
		// unmapping should not be performed.
		void const* MapCPUReadback(uint8_t frameLatency = k_maxFrameLatency);

		// The resource must be unmapped in the same frame it was mapped in
		void UnmapCPUReadback();


	private:		
		const uint64_t m_typeIDHash; // Hash of the typeid(T) at Create: Used to verify committed data types don't change
		const uint32_t m_dataByteSize;

		const BufferParams m_bufferParams;		

		std::unique_ptr<PlatformParams> m_platformParams;
		
		bool m_isCurrentlyMapped;


	private:
		// Use the factory Create() method instead
		Buffer(size_t typeIDHashCode, std::string const& bufferName, BufferParams const&, uint32_t dataByteSize);

		static void Register(
			std::shared_ptr<re::Buffer> newBuffer, uint32_t numBytes, uint64_t typeIDHash);

		static void RegisterAndCommit(
			std::shared_ptr<re::Buffer> newBuffer, void const* data, uint32_t numBytes, uint64_t typeIDHash);
		
		void CommitInternal(void const* data, uint64_t typeIDHash);

		void CommitInternal(void const* data, uint32_t baseOffset, uint32_t numBytes, uint64_t typeIDHash); // Partial


	private:
		Buffer() = delete;
		Buffer(Buffer const&) = delete;
		Buffer& operator=(Buffer const&) = delete;
	};


	inline bool Buffer::HasUsageBit(Usage usage, UsageMask usageMask)
	{
		return (usageMask & usage);
	}


	inline bool Buffer::HasUsageBit(Usage usage, re::Buffer::BufferParams const& bufferParams)
	{
		return HasUsageBit(usage, bufferParams.m_usageMask);
	}


	inline bool Buffer::HasUsageBit(Usage usage, re::Buffer const& buffer)
	{
		return HasUsageBit(usage, buffer.GetBufferParams());
	}


	inline bool Buffer::HasAccessBit(Access accessBits, AccessMask accessMask)
	{
		return (accessBits & accessMask);
	}


	inline bool Buffer::HasAccessBit(Access access, re::Buffer::BufferParams const& bufferParams)
	{
		return HasAccessBit(access, bufferParams.m_accessMask);
	}


	inline bool Buffer::HasAccessBit(Access access, re::Buffer const& buffer)
	{
		return HasAccessBit(access, buffer.GetBufferParams());
	}


	// Create any type of buffer:
	template<typename T>
	std::shared_ptr<re::Buffer> Buffer::Create(
		std::string const& bufferName, T const* dataArray, BufferParams const& bufferParams)
	{
		const uint32_t dataByteSize = sizeof(T) * bufferParams.m_arraySize;

		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new Buffer(typeid(T).hash_code(), bufferName, bufferParams, dataByteSize));

		RegisterAndCommit(newBuffer, dataArray, dataByteSize, typeid(T).hash_code());

		return newBuffer;
	}


	// Create a buffer for a single data object (eg. stage buffer)
	template<typename T>
	std::shared_ptr<re::Buffer> Buffer::Create(
		std::string const& bufferName, T const& data, BufferParams const& bufferParams)
	{
		const uint32_t dataByteSize = sizeof(T);

		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new re::Buffer(typeid(T).hash_code(), bufferName, bufferParams, dataByteSize));

		RegisterAndCommit(newBuffer, &data, dataByteSize, typeid(T).hash_code());

		return newBuffer;
	}


	template<typename T>
	std::shared_ptr<re::Buffer> Buffer::CreateUncommitted(
		std::string const& bufferName, BufferParams const& bufferParams)
	{
		const uint32_t dataByteSize = sizeof(T);

		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new re::Buffer(typeid(T).hash_code(), bufferName, bufferParams, dataByteSize));

		Register(newBuffer, dataByteSize, typeid(T).hash_code());

		return newBuffer;
	}


	// Create a buffer for an array of several objects of the same type (eg. instanced mesh matrices)
	template<typename T>
	std::shared_ptr<re::Buffer> Buffer::CreateArray(
		std::string const& bufferName, T const* dataArray, BufferParams const& bufferParams)
	{
		SEAssert(HasUsageBit(Usage::Structured, bufferParams), "Unexpected data type for a buffer array");

		const uint32_t dataByteSize = sizeof(T) * bufferParams.m_arraySize;

		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new Buffer(typeid(T).hash_code(), bufferName, bufferParams, dataByteSize));

		RegisterAndCommit(newBuffer, dataArray, dataByteSize, typeid(T).hash_code());

		return newBuffer;
	}


	template<typename T>
	std::shared_ptr<re::Buffer> Buffer::CreateUncommittedArray(
		std::string const& bufferName, BufferParams const& bufferParams)
	{
		const uint32_t dataByteSize = sizeof(T) * bufferParams.m_arraySize;

		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new Buffer(typeid(T).hash_code(), bufferName, bufferParams, dataByteSize));

		Register(newBuffer, dataByteSize, typeid(T).hash_code());
		
		return newBuffer;
	}


	inline std::shared_ptr<re::Buffer> Buffer::Create(
		std::string const& bufferName, void const* data, uint32_t numBytes, BufferParams const& bufferParams)
	{
		SEAssert(bufferParams.m_allocationType == re::Buffer::Immutable || 
			bufferParams.m_allocationType == re::Buffer::SingleFrame,
			"Invalid AllocationType type: It's (currently) not possible to Commit() via a nullptr");

		const uint64_t voidHashCode = typeid(void const*).hash_code();

		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new Buffer(voidHashCode, bufferName, bufferParams, numBytes));

		RegisterAndCommit(newBuffer, data, numBytes, voidHashCode);

		return newBuffer;
	}


	template <typename T>
	void Buffer::Commit(T const& data) // Commit *updated* data
	{
		CommitInternal(&data, typeid(T).hash_code());
	}


	template <typename T>
	void Buffer::Commit(T const* data, uint32_t baseIdx, uint32_t numElements)
	{
		const uint32_t dstBaseByteOffset = baseIdx * sizeof(T);
		const uint32_t numBytes = numElements * sizeof(T);

		CommitInternal(data, dstBaseByteOffset, numBytes, typeid(T).hash_code());
	}


	inline uint32_t Buffer::GetTotalBytes() const
	{
		return m_dataByteSize;
	}


	inline uint32_t Buffer::GetStride() const
	{
		return m_dataByteSize / m_bufferParams.m_arraySize;
	}


	inline Buffer::AllocationType Buffer::GetAllocationType() const
	{
		return m_bufferParams.m_allocationType;
	}


	inline uint8_t Buffer::GetUsageMask() const
	{
		return m_bufferParams.m_usageMask;
	}


	inline uint32_t Buffer::GetArraySize() const
	{
		return m_bufferParams.m_arraySize;
	}


	inline Buffer::BufferParams const& Buffer::GetBufferParams() const
	{
		return m_bufferParams;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Buffer::PlatformParams::~PlatformParams() {};
}