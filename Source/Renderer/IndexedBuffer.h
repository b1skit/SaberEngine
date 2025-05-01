// © 2025 Adam Badke.All rights reserved.
#pragma once
#include "Buffer.h"
#include "RenderDataManager.h"
#include "RenderObjectIDs.h"
#include "TransformRenderData.h"

#include "Core/Assert.h"
#include "Core/Logger.h"
#include "Core/ProfilingMarkers.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/HashUtils.h"
#include "Core/Util/MathUtils.h"

#include "Core/Util/HashKey.h"
#include "Core/Util/ThreadProtector.h"


namespace gr
{
	class IndexedBufferManager final
	{
	public:
		typedef uint32_t IndexType;


	public:
		class IIndexedBuffer
		{
		public:
			template<typename LUTBuffer>
			inline void AddLUTDataWriterCallback(void(*writeLUTDataCallback)(IndexType lutIdx, void* dst))
			{
				AddLUTDataWriterCallbackInternal(std::type_index(typeid(LUTBuffer)), writeLUTDataCallback);
			}


		private:
			virtual void AddLUTDataWriterCallbackInternal(
				std::type_index, void(*writeLUTDataCallback)(IndexType lutIdx, void* dst)) = 0;
		};


	private:
		static constexpr IndexType k_invalidIdx = std::numeric_limits<IndexType>::max();


		class IIndexedBufferInternal : public virtual IIndexedBuffer
		{
		public:
			IIndexedBufferInternal(IndexedBufferManager* ibm)
				: m_indexedBufferManager(ibm),
				m_threadProtector(false)
			{}

			virtual ~IIndexedBufferInternal() = default;


		public:
			virtual void Destroy() = 0;
			virtual void UpdateBuffer(gr::RenderDataManager const&) = 0;
			virtual std::shared_ptr<re::Buffer> GetBuffer() const = 0;
			virtual re::BufferInput GetBufferInput(char const* shaderName) const = 0;

			
		private:
			virtual IndexType GetIndex(gr::RenderDataManager const&, IDType) const = 0;


		public:
			template<typename LUTBuffer>
			void WriteLUTData(gr::RenderDataManager const& renderData, IDType id, LUTBuffer* dst) const
			{
				// Note: May be invalid if ID is not associated with RenderData of the managed type
				const IndexType lutIdx = GetIndex(renderData, id);

				// Lock the thread protector now that we've got the index:
				util::ScopedThreadProtector lock(m_threadProtector);

				const std::type_index typeIdx = std::type_index(typeid(LUTBuffer));
				SEAssert(m_writeLUTDataCallbacks.contains(typeIdx), "No registered LUT writer for this type");

				// Execute the callback:
				m_writeLUTDataCallbacks.at(typeIdx)(lutIdx, dst);
			}


		private:
			void AddLUTDataWriterCallbackInternal(
				std::type_index typeIdx,
				void(*writeLUTDataCallback)(IndexType lutIdx, void* dst)) override
			{
				util::ScopedThreadProtector lock(m_threadProtector);

				SEAssert(!m_writeLUTDataCallbacks.contains(typeIdx), "Callback already added for the given type");
				m_writeLUTDataCallbacks.emplace(typeIdx, writeLUTDataCallback);

				m_indexedBufferManager->RegisterLUTWriter(typeIdx, this);
			}


		private:
			std::unordered_map<std::type_index, void(*)(IndexType lutIdx, void* dst)> m_writeLUTDataCallbacks;
			IndexedBufferManager* m_indexedBufferManager;


		protected:
			mutable util::ThreadProtector m_threadProtector;
		};


		template<typename RenderDataType, typename BufferDataType>
		class TypedIndexedBuffer final : public virtual IIndexedBufferInternal
		{
		public:
			TypedIndexedBuffer(
				IndexedBufferManager* ibm,
				BufferDataType(*createBufferData)(RenderDataType const&),
				char const* bufferName,
				re::Buffer::MemoryPoolPreference memPoolPreference,
				re::Buffer::AccessMask accessMask,
				RenderObjectFeature featureBits = RenderObjectFeature::None);

			TypedIndexedBuffer(TypedIndexedBuffer&&) noexcept = default;
			TypedIndexedBuffer& operator=(TypedIndexedBuffer&&) = default;

			~TypedIndexedBuffer() = default;

			void Clear();
			void Destroy() override;


		public:
			void UpdateBuffer(gr::RenderDataManager const&) override;
			std::shared_ptr<re::Buffer> GetBuffer() const override;
			re::BufferInput GetBufferInput(char const* shaderName) const override;
			IndexType GetIndex(gr::RenderDataManager const&, IDType id) const override;


		private:
			std::unordered_map<IDType, IndexType> m_idToBufferIdx;

			// We use a priority queue to ensure that indexes closest to 0 are reused first, to keep packing as tight
			std::priority_queue<IndexType, std::vector<IndexType>, std::greater<IndexType>> m_freeIndexes;

			std::string m_bufferName; // Note: Used for ID/lookup - Is not the shader name
			std::shared_ptr<re::Buffer> m_buffer;

			BufferDataType(*m_createBufferData)(RenderDataType const&);

			RenderObjectFeature m_featureBits;

			// Buffer create params:
			re::Buffer::MemoryPoolPreference m_memPoolPreference;
			re::Buffer::AccessMask m_accessMask;

			static constexpr uint32_t k_arraySizeAlignment = 16; // Buffer sizes are rounded up to nearest multiple
			static constexpr float k_shrinkFactor = 2.f; // How much smaller before shrinking the Buffer?


		private: // No copies allowed:
			TypedIndexedBuffer(TypedIndexedBuffer const&) = delete;
			TypedIndexedBuffer& operator=(TypedIndexedBuffer const&) = delete;
		};


	public:
		IndexedBufferManager(gr::RenderDataManager const& renderData);
		~IndexedBufferManager();

		IndexedBufferManager(IndexedBufferManager&&) = default;
		IndexedBufferManager& operator=(IndexedBufferManager&&) = default;

		void Destroy();


	public:
		void Update(); // Must be called at the beginning of each frame


	public:
		template<typename RenderDataType, typename BufferDataType>
		IIndexedBuffer* AddIndexedBuffer(
			char const* bufferName,
			BufferDataType(*createBufferData)(RenderDataType const&),
			re::Buffer::MemoryPoolPreference,
			RenderObjectFeature featureBits = RenderObjectFeature::None);

		template<typename LUTBuffer>
		re::BufferInput GetLUTBufferInput(std::ranges::range auto&& renderDataIDs);

		re::BufferInput GetIndexedBufferInput(util::HashKey bufferNameHash, char const* shaderName) const;
		re::BufferInput GetIndexedBufferInput(char const* bufferName, char const* shaderName) const;


	private:
		void RegisterLUTWriter(std::type_index typeIdx, IIndexedBufferInternal* indexedBuffer);

		template<typename LUTBuffer>
		std::shared_ptr<re::Buffer> GetLUTBuffer(std::ranges::range auto&& renderDataIDs, IndexType& baseIdxOut);


	private:
		std::vector<std::unique_ptr<IIndexedBufferInternal>> m_indexedBuffers;
		std::multimap<std::type_index, IIndexedBufferInternal*> m_lutWritingBuffers; // LUTBuffer type -> writers
		std::unordered_map<util::HashKey, IIndexedBufferInternal*> m_bufferNameHashToIndexedBuffer;

		gr::RenderDataManager const& m_renderData;

		// We sub-allocate out of permanent Buffer(s); If we outgrow it we create a new, larger Buffer and allow the old
		// one to go out of scope via deferred deletion		
		struct LUTMetadata
		{
			std::unordered_map<util::HashKey, re::BufferInput> m_LUTBufferInputs;
			std::shared_ptr<re::Buffer> m_LUTBuffer;
			IndexType m_firstFreeBaseIdx = 0;
		};
		std::unordered_map<std::type_index, LUTMetadata> m_LUTTypeToLUTMetadata; // <LUTBuffer> -> LUTMetadata
		std::mutex m_LUTTypeToLUTMetadataMutex;

		// Map RenderDataID -> re::BufferInput entries, so we can destroy (potentially) stale BufferInputs
		std::unordered_multimap<IDType, std::pair<LUTMetadata*, util::HashKey>> m_IDToBufferInputs;

		static constexpr uint32_t k_defaultLUTBufferArraySize = 512;
		static constexpr float k_LUTBufferGrowthFactor = 2.f;

		util::ThreadProtector m_threadProtector;


	private:
		IndexedBufferManager(IndexedBufferManager const&) = delete;
		IndexedBufferManager& operator=(IndexedBufferManager const&) = delete;
	};


	// ---


	template<typename RenderDataType, typename BufferDataType>
	IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::TypedIndexedBuffer(
		IndexedBufferManager* ibm,
		BufferDataType(*createBufferData)(RenderDataType const&),
		char const* bufferName,
		re::Buffer::MemoryPoolPreference memPoolPreference,
		re::Buffer::AccessMask accessMask,
		RenderObjectFeature featureBits /*= RenderObjectFeature::None*/)
		: IIndexedBufferInternal(ibm)
		, m_createBufferData(createBufferData)
		, m_bufferName(bufferName)
		, m_featureBits(featureBits)
		, m_memPoolPreference(memPoolPreference)
		, m_accessMask(accessMask)
	{
		SEAssert(m_createBufferData != nullptr, "Invalid Buffer creation callback");
	}


	template<typename RenderDataType, typename BufferDataType>
	void IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::Clear()
	{
		if (m_buffer)
		{
			m_idToBufferIdx.clear();
			m_freeIndexes = {};
			m_buffer = nullptr;
		}
	}

	template<typename RenderDataType, typename BufferDataType>
	void IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::Destroy()
	{
		util::ScopedThreadProtector lock(m_threadProtector);

		Clear();
	}


	template<typename RenderDataType, typename BufferDataType>
	void IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::UpdateBuffer(
		gr::RenderDataManager const& renderData)
	{
		SEBeginCPUEvent("IndexedBufferManager::TypedIndexedBuffer::UpdateBuffer");

		util::ScopedThreadProtector lock(m_threadProtector);

		if (renderData.HasAnyDirtyData<RenderDataType>() == false)
		{
			SEEndCPUEvent();
			return; // Early out if nothing has changed
		}

		const uint32_t numRenderDataElements = renderData.GetNumElementsOfType<RenderDataType>(m_featureBits);
		if (numRenderDataElements == 0)
		{
			Clear();
			return;
		}

		if (m_buffer == nullptr ||
			m_buffer->GetBufferParams().m_arraySize < numRenderDataElements ||
			m_buffer->GetBufferParams().m_arraySize <= (numRenderDataElements / k_shrinkFactor))
		{
			const uint32_t arraySize = util::RoundUpToNearestMultiple(numRenderDataElements, k_arraySizeAlignment);

			// If the Buffer already exists, we rely on the deferred delete to keep it in scope for any in-flight frames
			m_buffer = re::Buffer::CreateUncommittedArray<BufferDataType>(
				m_bufferName.c_str(),
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::Permanent,
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = m_memPoolPreference,
					.m_accessMask = m_accessMask,
					.m_usageMask = re::Buffer::Usage::Structured,
					.m_arraySize = arraySize,
				});

			// Re-populate the free index queue:
			m_freeIndexes = {};
			for (IndexType i = 0; i < m_buffer->GetArraySize(); ++i)
			{
				m_freeIndexes.emplace(i);
			}

			// Clear our index map: We'll re-populate it as we build our Buffer data
			m_idToBufferIdx.clear();

			// Build and commit our Buffer data:
			std::vector<BufferDataType> bufferData;
			bufferData.resize(m_buffer->GetArraySize());

			if constexpr (std::is_same_v<RenderDataType, gr::Transform::RenderData>)
			{
				// Transforms are treated as a special case by the RenderDataManager; We must do the same here
				for (gr::TransformID transformID : renderData.GetRegisteredTransformIDs())
				{
					SEAssert(!m_freeIndexes.empty(), "No more free indexes. This should not be possible");

					const IndexType currentBufferIdx = m_freeIndexes.top();
					m_freeIndexes.pop();

					bufferData[currentBufferIdx] =
						m_createBufferData(renderData.GetTransformDataFromTransformID(transformID));

					m_idToBufferIdx.emplace(transformID, currentBufferIdx);
				}
			}
			else
			{
				gr::ObjectAdapter<RenderDataType> objAdapter(renderData, m_featureBits);
				for (auto const& itr : objAdapter)
				{
					SEAssert(!m_freeIndexes.empty(), "No more free indexes. This should not be possible");

					const IndexType currentBufferIdx = m_freeIndexes.top();
					m_freeIndexes.pop();

					bufferData[currentBufferIdx] = m_createBufferData(itr->Get<RenderDataType>());

					m_idToBufferIdx.emplace(itr->GetRenderDataID(), currentBufferIdx);
				}
			}

			m_buffer->Commit(bufferData.data(), 0, util::CheckedCast<uint32_t>(bufferData.size()));
		}
		else // Update the existing buffer:
		{
			// Remove deleted RenderDataTypes:
			auto ProcessDeletedIDs = [this](std::vector<gr::IDType> const& deletedIDs)
				{
					for (gr::IDType deletedID : deletedIDs)
					{
						const IndexType deletedIdx = m_idToBufferIdx.at(deletedID);
						m_idToBufferIdx.erase(deletedID);
						m_freeIndexes.emplace(deletedIdx);
					}
				};

			if constexpr (std::is_same_v<RenderDataType, gr::Transform::RenderData>)
			{
				std::vector<gr::TransformID> const& deletedTransformIDs = renderData.GetDeletedTransformIDs();

				ProcessDeletedIDs(deletedTransformIDs);
			}
			else
			{
				std::vector<gr::RenderDataID> const* deletedRenderDataIDs = 
					renderData.GetIDsWithDeletedData<RenderDataType>();
				
				if (deletedRenderDataIDs)
				{
					ProcessDeletedIDs(*deletedRenderDataIDs);
				}
			}

			// Add/update new/dirty RenderDataTypes:
			auto ProcessDirtyIDs = [&renderData, this](std::vector<gr::IDType> const& dirtyIDs)
				{
					for (gr::IDType dirtyID : dirtyIDs)
					{
						IndexType bufferIdx = std::numeric_limits<IndexType>::max();

						auto itr = m_idToBufferIdx.find(dirtyID);
						if (itr == m_idToBufferIdx.end())
						{
							SEAssert(!m_freeIndexes.empty(), "No more free indexes. This should not be possible");

							bufferIdx = m_freeIndexes.top();
							m_freeIndexes.pop();

							m_idToBufferIdx.emplace(dirtyID, bufferIdx);
						}
						else
						{
							bufferIdx = itr->second;
						}

						RenderDataType const* data = nullptr;
						if constexpr (std::is_same_v<RenderDataType, gr::Transform::RenderData>)
						{
							data = &renderData.GetTransformDataFromTransformID(dirtyID);
						}
						else
						{
							data = &renderData.GetObjectData<RenderDataType>(dirtyID);
						}

						BufferDataType const& bufferData = m_createBufferData(*data);

						m_buffer->Commit(&bufferData, bufferIdx, 1);
					}
				};

			if constexpr (std::is_same_v<RenderDataType, gr::Transform::RenderData>)
			{
				std::vector<gr::TransformID> const& dirtyTransformIDs = renderData.GetIDsWithDirtyTransformData();
				ProcessDirtyIDs(dirtyTransformIDs);
			}
			else
			{
				std::vector<gr::RenderDataID> const& dirtyIDs = renderData.GetIDsWithAnyDirtyData<RenderDataType>();
				ProcessDirtyIDs(dirtyIDs);
			}
		}

		SEEndCPUEvent();
	}


	template<typename RenderDataType, typename BufferDataType>
	std::shared_ptr<re::Buffer> IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::GetBuffer() const
	{
		util::ScopedThreadProtector lock(m_threadProtector);

		return m_buffer;
	}


	template<typename RenderDataType, typename BufferDataType>
	re::BufferInput IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::GetBufferInput(
		char const* shaderName) const
	{
		return re::BufferInput(shaderName, GetBuffer());
	}


	template<typename RenderDataType, typename BufferDataType>
	IndexedBufferManager::IndexType IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::GetIndex(
		gr::RenderDataManager const& renderData, IDType id) const
	{
		util::ScopedThreadProtector lock(m_threadProtector);

		if constexpr (std::is_same_v<RenderDataType, gr::Transform::RenderData>)
		{
			// Transform buffers map TransformID -> buffer index, convert RenderDataID -> TransformID :
			id = renderData.GetTransformIDFromRenderDataID(id);
		}

		auto itr = m_idToBufferIdx.find(id);
		if (itr == m_idToBufferIdx.end())
		{
			return k_invalidIdx;
		}
		return itr->second;
	}


	// ---


	template<typename RenderDataType, typename BufferDataType>
	IndexedBufferManager::IIndexedBuffer* IndexedBufferManager::AddIndexedBuffer(
		char const* bufferName,
		BufferDataType(*createBufferData)(RenderDataType const&),
		re::Buffer::MemoryPoolPreference memPool, 
		RenderObjectFeature featureBits /*= RenderObjectFeature::None*/)
	{
		util::ScopedThreadProtector lock(m_threadProtector);

		re::Buffer::AccessMask access = re::Buffer::Access::GPURead;
		if (memPool == re::Buffer::MemoryPoolPreference::UploadHeap)
		{
			access |= re::Buffer::Access::CPUWrite;
		}

		auto const& itr = m_indexedBuffers.emplace_back(
			std::make_unique<TypedIndexedBuffer<RenderDataType, BufferDataType>>(
				this,
				createBufferData,
				bufferName,
				memPool,
				access,
				featureBits));

		m_bufferNameHashToIndexedBuffer.emplace(util::HashKey(bufferName), itr.get());

		return itr.get();
	}


	template<typename LUTBuffer>
	std::shared_ptr<re::Buffer> IndexedBufferManager::GetLUTBuffer(
		std::ranges::range auto&& renderDataIDs,
		IndexType& baseIdxOut)
	{
		SEBeginCPUEvent("IndexedBufferManager::GetLUTBuffer");

		util::ScopedThreadProtector lock(m_threadProtector);

		const std::type_index lutTypeIdx = std::type_index(typeid(LUTBuffer));

		SEAssert(m_lutWritingBuffers.contains(lutTypeIdx),
			"No indexed buffers have a registered LUT data writer of this type ");

		auto WriteLUTData = [this, &renderDataIDs, lutTypeIdx](std::span<LUTBuffer> lutBufferData)
			{
				auto const& entries = m_lutWritingBuffers.equal_range(lutTypeIdx);
				for (auto itr = entries.first; itr != entries.second; ++itr)
				{
					for (size_t writeIdx = 0; writeIdx < lutBufferData.size(); ++writeIdx)
					{
						itr->second->WriteLUTData<LUTBuffer>(
							m_renderData,
							renderDataIDs[writeIdx],
							&lutBufferData[writeIdx]);
					}
				}
			};

		SEAssert(m_LUTTypeToLUTMetadata.contains(lutTypeIdx), "No LUT buffer entry exists. It should have already been added");

		auto lutMetadataItr = m_LUTTypeToLUTMetadata.find(lutTypeIdx);
		if (lutMetadataItr->second.m_LUTBuffer == nullptr ||
			(lutMetadataItr->second.m_firstFreeBaseIdx + renderDataIDs.size()) >
			lutMetadataItr->second.m_LUTBuffer->GetArraySize())			
		{
			// Determine the array size:
			uint32_t arraySize = 0;
			if (lutMetadataItr->second.m_LUTBuffer == nullptr)
			{
				if (renderDataIDs.size() > k_defaultLUTBufferArraySize)
				{
					arraySize = util::RoundUpToNearestMultiple(
						util::CheckedCast<uint32_t>(renderDataIDs.size()), k_defaultLUTBufferArraySize);
				}
				else
				{
					arraySize = k_defaultLUTBufferArraySize;
				}				
			}
			else
			{
				arraySize = util::CheckedCast<uint32_t>(
					lutMetadataItr->second.m_LUTBuffer->GetArraySize() * k_LUTBufferGrowthFactor);
			}
			SEAssert(arraySize >= renderDataIDs.size(), "Array size is not big enough");

			LOG(std::format("{} indexed buffer LUT for type \"{}\", with {} elements",
				(arraySize == k_defaultLUTBufferArraySize ? "Creating" : "Recreating"),
				lutTypeIdx.name(),
				arraySize));
			
			// Create the initial buffer data:
			std::vector<LUTBuffer> lutBufferData;
			lutBufferData.resize(arraySize);

			// Populate the first entries with LUT data for our RenderDataIDs:
			WriteLUTData(std::span<LUTBuffer>(lutBufferData.begin(), renderDataIDs.size()));

			// Create the buffer:
			lutMetadataItr->second.m_LUTBuffer = re::Buffer::CreateArray(
				std::format("{}_ManagedLUT", LUTBuffer::s_shaderName),
				lutBufferData.data(),
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::Permanent,
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Structured,
					.m_arraySize = arraySize,
				});

			lutMetadataItr->second.m_LUTBufferInputs.clear(); // Clear the cached BufferInputs, as indexing may have changed

			// Set the initial the base index:
			baseIdxOut = 0;
			lutMetadataItr->second.m_firstFreeBaseIdx = util::CheckedCast<IndexType>(renderDataIDs.size());
		}
		else
		{
			// Populate buffer data for our current IDs only:
			std::vector<LUTBuffer> lutBufferData;
			lutBufferData.resize(renderDataIDs.size(), LUTBuffer{});

			// Record our current entries:
			WriteLUTData(std::span<LUTBuffer>(lutBufferData));

			// Commit the updated data:
			lutMetadataItr->second.m_LUTBuffer->Commit(
				lutBufferData.data(),
				lutMetadataItr->second.m_firstFreeBaseIdx,
				util::CheckedCast<uint32_t>(lutBufferData.size()));

			// Update the base index:
			baseIdxOut = lutMetadataItr->second.m_firstFreeBaseIdx;
			lutMetadataItr->second.m_firstFreeBaseIdx += util::CheckedCast<IndexType>(renderDataIDs.size());
		}

		return lutMetadataItr->second.m_LUTBuffer;
	}


	template<typename LUTBuffer>
	inline re::BufferInput IndexedBufferManager::GetLUTBufferInput(std::ranges::range auto&& renderDataIDs)
	{
		const std::type_index lutTypeIdx = std::type_index(typeid(LUTBuffer));

		// Hash the inputs so we can reuse Buffers/BufferInputs:
		util::HashKey lutHash = util::HashCStr(LUTBuffer::s_shaderName);
		util::AddDataToHash(lutHash, lutTypeIdx.hash_code());
		for (auto const& id : renderDataIDs)
		{
			util::AddDataToHash(lutHash, id);
		}
		util::AddDataToHash(lutHash, renderDataIDs.size());

		// Critical section: Get/create a LUT BufferInput
		{
			std::lock_guard<std::mutex> lock(m_LUTTypeToLUTMetadataMutex);

			// Try and return an existing BufferInput:
			auto metadataItr = m_LUTTypeToLUTMetadata.find(lutTypeIdx);
			if (metadataItr != m_LUTTypeToLUTMetadata.end())
			{
				auto bufferInputItr = metadataItr->second.m_LUTBufferInputs.find(lutHash);
				if (bufferInputItr != metadataItr->second.m_LUTBufferInputs.end())
				{
					return bufferInputItr->second;
				}
			}

			if (metadataItr == m_LUTTypeToLUTMetadata.end())
			{
				metadataItr = m_LUTTypeToLUTMetadata.emplace(lutTypeIdx, LUTMetadata{}).first;
			}

			IndexType firstElement = std::numeric_limits<IndexType>::max();

			std::shared_ptr<re::Buffer> const& lutBuffer = GetLUTBuffer<LUTBuffer>(renderDataIDs, firstElement);
			SEAssert(firstElement != std::numeric_limits<IndexType>::max(), "Failed to get a valid 1st element");

			auto const& bufferInputItr = metadataItr->second.m_LUTBufferInputs.emplace(
				lutHash,
				re::BufferInput(
					LUTBuffer::s_shaderName,
					lutBuffer,
					re::BufferView::BufferType{
						.m_firstElement = firstElement,
						.m_numElements = util::CheckedCast<uint32_t>(renderDataIDs.size()),
						.m_structuredByteStride = sizeof(LUTBuffer),
						.m_firstDestIdx = 0,
					})).first;

			// Map the RenderDataIDs to the BufferViews, so we can destroy the views if any data associated with the
			// RenderDataIDs is ever destroyed
			for (gr::RenderDataID renderDataID : renderDataIDs)
			{
				m_IDToBufferInputs.emplace(renderDataID,
					std::make_pair(&metadataItr->second, lutHash));
			}

			return bufferInputItr->second;
		}
	}


	inline re::BufferInput IndexedBufferManager::GetIndexedBufferInput(
		util::HashKey bufferNameHash,
		char const* shaderName) const
	{
		SEAssert(m_bufferNameHashToIndexedBuffer.contains(bufferNameHash), "No buffer with that name registered");

		return m_bufferNameHashToIndexedBuffer.at(bufferNameHash)->GetBufferInput(shaderName);
	}


	inline re::BufferInput IndexedBufferManager::GetIndexedBufferInput(
		char const* bufferName,
		char const* shaderName) const
	{
		return GetIndexedBufferInput(util::HashKey(bufferName), shaderName);
	}


	inline void IndexedBufferManager::RegisterLUTWriter(std::type_index typeIdx, IIndexedBufferInternal* indexedBuffer)
	{
		util::ScopedThreadProtector lock(m_threadProtector);

		m_lutWritingBuffers.emplace(typeIdx, indexedBuffer);
	}
}