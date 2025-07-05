// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "EnumTypes.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IPlatformObject.h"
#include "Core/Interfaces/IUniqueID.h"

#include "Renderer/Shaders/Common/ResourceCommon.h"


namespace re
{
	class BufferAllocator;


	class Buffer : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		struct BufferParams;


		enum class StagingPool : uint8_t
		{
			Permanent,	// Mutable: Can be modified, and is re-buffered when modification is detected
			Temporary,	// Immutable: Temporary staging memory for permanent/single frame buffers initialized once
			None,		// GPU-only buffers

			StagingPool_Invalid
		};

		enum Usage : uint8_t
		{
			Constant		= 1 << 0,
			Structured		= 1 << 1,
			Raw				= 1 << 2, // 16B aligned data (E.g. Vertex/index buffers, byte address buffers, etc)

			Invalid			= 0,
			None			= 0, // Convenience/readability: For when no extra usage bits are needed
		};
		static bool HasUsageBit(Usage usageBit, Usage usageMask);
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
		static bool HasAccessBit(Access accessBit, Access accessMask);
		static bool HasAccessBit(Access, re::Buffer::BufferParams const&);
		static bool HasAccessBit(Access, re::Buffer const&);

		struct BufferParams final
		{
			re::Lifetime m_lifetime = re::Lifetime::Permanent;
			StagingPool m_stagingPool = StagingPool::StagingPool_Invalid;
			MemoryPoolPreference m_memPoolPreference = MemoryPoolPreference::DefaultHeap;
			Access m_accessMask = Access::GPURead;
			Usage m_usageMask = Usage::Invalid;

			// Array size != 1 is only valid for Usage types with operator[] (e.g Structured, Raw)
			uint32_t m_arraySize = 1; // Must be 1 for constant buffers
		};


	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual ~PlatObj() = default;
			virtual void Destroy() override = 0;

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

		// Create a buffer with no CPU-side staging data
		[[nodiscard]] static std::shared_ptr<re::Buffer> CreateUnstaged(
			std::string const& bufferName, uint32_t numBytes, BufferParams const&);


	public:
		Buffer(Buffer&&) noexcept = default;
		Buffer& operator=(Buffer&&) noexcept = default;
		~Buffer();


	public:
		template <typename T>
		void Commit(T const& data); // Commit *updated* data
		
		template <typename T>
		void Commit(T const* data, uint32_t baseIdx, uint32_t numElements); // Recommit mutable array data (only)
	
		void const* GetData() const;
		void GetDataAndSize(void const** out_data, uint32_t* out_numBytes) const;
		uint32_t GetTotalBytes() const;
		uint32_t GetStride() const;
		StagingPool GetStagingPool() const;
		Usage GetUsageMask() const;
		re::Lifetime GetLifetime() const;

		uint32_t GetArraySize() const; // Instanced buffers: How many instances of data does the buffer hold?

		BufferParams const& GetBufferParams() const;

		inline PlatObj* GetPlatformObject() const { return m_platObj.get(); }
		void SetPlatformObject(std::unique_ptr<PlatObj> platObj) { m_platObj = std::move(platObj); }

		// Bindless:
	public:
		ResourceHandle GetResourceHandle(re::ViewType) const;


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

		std::unique_ptr<PlatObj> m_platObj;
		
		ResourceHandle m_cbvResourceHandle;
		ResourceHandle m_srvResourceHandle;

		bool m_isCurrentlyMapped;

#if defined(_DEBUG)
		uint64_t m_creationFrameNum; // What frame was this buffer created on?
#endif

	private:
		// Use the factory Create() method instead
		Buffer(size_t typeIDHashCode, std::string const& bufferName, BufferParams const&, uint32_t dataByteSize);

		static void Register(
			std::shared_ptr<re::Buffer> const& newBuffer, uint32_t numBytes, uint64_t typeIDHash);

		static void RegisterAndCommit(
			std::shared_ptr<re::Buffer> const& newBuffer, void const* data, uint32_t numBytes, uint64_t typeIDHash);
		
		void CommitInternal(void const* data, uint64_t typeIDHash);

		void CommitMutableInternal(void const* data, uint32_t baseOffset, uint32_t numBytes, uint64_t typeIDHash); // Partial

	protected:
		friend class BufferAllocator;
		static BufferAllocator* s_bufferAllocator;


	private:
		Buffer() = delete;
		Buffer(Buffer const&) = delete;
		Buffer& operator=(Buffer const&) = delete;
	};


	inline bool Buffer::HasUsageBit(Usage usageBit, Usage usageMask)
	{
		return (usageBit & usageMask);
	}


	inline bool Buffer::HasUsageBit(Usage usageBit, re::Buffer::BufferParams const& bufferParams)
	{
		return HasUsageBit(usageBit, bufferParams.m_usageMask);
	}


	inline bool Buffer::HasUsageBit(Usage usageBit, re::Buffer const& buffer)
	{
		return HasUsageBit(usageBit, buffer.GetBufferParams());
	}


	inline bool Buffer::HasAccessBit(Access accessBit, Access accessMask)
	{
		return (accessBit & accessMask);
	}


	inline bool Buffer::HasAccessBit(Access accessBit, re::Buffer::BufferParams const& bufferParams)
	{
		return HasAccessBit(accessBit, bufferParams.m_accessMask);
	}


	inline bool Buffer::HasAccessBit(Access accessBit, re::Buffer const& buffer)
	{
		return HasAccessBit(accessBit, buffer.GetBufferParams());
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
		SEAssert(bufferParams.m_stagingPool != re::Buffer::StagingPool::None, 
			"Buffer specifies no CPU-side staging, but staging data received. Is this the correct create function?");

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
		SEAssert(bufferParams.m_stagingPool != re::Buffer::StagingPool::None,
			"Buffer specifies no CPU-side staging, but staging data received. Is this the correct create function?");

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
		SEAssert(bufferParams.m_stagingPool != re::Buffer::StagingPool::None,
			"Buffer specifies no CPU-side staging, but staging data received. Is this the correct create function?");

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
		SEAssert(bufferParams.m_stagingPool != re::Buffer::StagingPool::None,
			"Buffer specifies no CPU-side staging, but staging data received. Is this the correct create function?");

		const uint32_t dataByteSize = sizeof(T) * bufferParams.m_arraySize;

		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new Buffer(typeid(T).hash_code(), bufferName, bufferParams, dataByteSize));

		Register(newBuffer, dataByteSize, typeid(T).hash_code());
		
		return newBuffer;
	}


	inline std::shared_ptr<re::Buffer> Buffer::Create(
		std::string const& bufferName, void const* data, uint32_t numBytes, BufferParams const& bufferParams)
	{
		SEAssert(bufferParams.m_stagingPool == re::Buffer::StagingPool::Temporary,
			"Invalid staging pool: It's (currently) not possible to Stage() via a nullptr");

		const uint64_t voidHashCode = typeid(void const*).hash_code();

		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new Buffer(voidHashCode, bufferName, bufferParams, numBytes));

		RegisterAndCommit(newBuffer, data, numBytes, voidHashCode);

		return newBuffer;
	}


	inline std::shared_ptr<re::Buffer> Buffer::CreateUnstaged(
		std::string const& bufferName, uint32_t numBytes, BufferParams const& bufferParams)
	{
		SEAssert(bufferParams.m_stagingPool == re::Buffer::StagingPool::None,
			"Invalid staging pool for a GPU-only buffer");

		const uint64_t voidHashCode = typeid(void const*).hash_code();

		std::shared_ptr<re::Buffer> newBuffer;
		newBuffer.reset(new Buffer(voidHashCode, bufferName, bufferParams, numBytes));

		RegisterAndCommit(newBuffer, nullptr, numBytes, voidHashCode);

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
		SEAssert(data && numElements > 0, "Cannot commit zero elements");

		const uint32_t dstBaseByteOffset = baseIdx * sizeof(T);
		const uint32_t numBytes = numElements * sizeof(T);

		CommitMutableInternal(data, dstBaseByteOffset, numBytes, typeid(T).hash_code());
	}


	inline uint32_t Buffer::GetTotalBytes() const
	{
		return m_dataByteSize;
	}


	inline uint32_t Buffer::GetStride() const
	{
		return m_dataByteSize / m_bufferParams.m_arraySize;
	}


	inline Buffer::StagingPool Buffer::GetStagingPool() const
	{
		return m_bufferParams.m_stagingPool;
	}


	inline Buffer::Usage Buffer::GetUsageMask() const
	{
		return m_bufferParams.m_usageMask;
	}


	inline re::Lifetime Buffer::GetLifetime() const
	{
		return m_bufferParams.m_lifetime;
	}


	inline uint32_t Buffer::GetArraySize() const
	{
		return m_bufferParams.m_arraySize;
	}


	inline Buffer::BufferParams const& Buffer::GetBufferParams() const
	{
		return m_bufferParams;
	}


	inline ResourceHandle Buffer::GetResourceHandle(re::ViewType viewType) const
	{
		switch (viewType)
		{
		case re::ViewType::CBV:
		{
			return m_cbvResourceHandle;
		}
		break;
		case re::ViewType::SRV:
		{
			return m_srvResourceHandle;
		}
		break;
		case re::ViewType::UAV:
		default: SEAssertF("Invalid view type");
		}
		return INVALID_RESOURCE_IDX; // This should never happen
	}
}


namespace
{
	inline re::Buffer::Usage operator|(re::Buffer::Usage lhs, re::Buffer::Usage rhs)
	{
		return static_cast<re::Buffer::Usage>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
	}
	inline re::Buffer::Usage& operator|=(re::Buffer::Usage& lhs, re::Buffer::Usage rhs)
	{
		return lhs = lhs | rhs;
	};
	inline re::Buffer::Usage operator&(re::Buffer::Usage lhs, re::Buffer::Usage rhs)
	{
		return static_cast<re::Buffer::Usage>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
	}
	inline re::Buffer::Usage& operator&=(re::Buffer::Usage& lhs, re::Buffer::Usage rhs)
	{
		return lhs = lhs & rhs;
	};


	// ---


	inline re::Buffer::Access operator|(re::Buffer::Access lhs, re::Buffer::Access rhs)
	{
		return static_cast<re::Buffer::Access>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
	}
	inline re::Buffer::Access& operator|=(re::Buffer::Access& lhs, re::Buffer::Access rhs)
	{
		return lhs = lhs | rhs;
	};
	inline re::Buffer::Access operator&(re::Buffer::Access lhs, re::Buffer::Access rhs)
	{
		return static_cast<re::Buffer::Access>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
	}
	inline re::Buffer::Access& operator&=(re::Buffer::Access& lhs, re::Buffer::Access rhs)
	{
		return lhs = lhs & rhs;
	};
}