// © 2025 Adam Badke.All rights reserved.
#pragma once
#include "Private/Buffer.h"
#include "Private/RenderDataManager.h"
#include "Private/RenderObjectIDs.h"
#include "Private/TransformRenderData.h"

#include "Core/Assert.h"
#include "Core/Logger.h"
#include "Core/ProfilingMarkers.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/MathUtils.h"

#include "Core/Util/HashKey.h"
#include "Core/Util/ThreadProtector.h"

#include "Private/Renderer/Shaders/Common/ResourceCommon.h"


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
			inline void AddLUTWriterCallback(void(*WriteLUTDataCallback)(IndexType lutIdx, void* dst))
			{
				AddLUTDataWriterCallbackInternal(std::type_index(typeid(LUTBuffer)), WriteLUTDataCallback);
			}

		private:
			virtual void AddLUTDataWriterCallbackInternal(
				std::type_index, void(*WriteLUTDataCallback)(IndexType lutIdx, void* dst)) = 0;
		};


	private:
		class IIndexedBufferInternal : public virtual IIndexedBuffer
		{
		public:
			IIndexedBufferInternal(IndexedBufferManager* ibm)
				: m_indexedBufferManager(ibm)
				, m_threadProtector(true)
			{}

			virtual ~IIndexedBufferInternal() = default;


		public:
			virtual void Destroy() = 0;
			virtual bool UpdateBuffer(gr::RenderDataManager const&) = 0; // Returns true if Buffer was reallocated
			virtual std::shared_ptr<re::Buffer> GetBuffer() const = 0;

			// Get a BufferInput for the entire managed array buffer:
			virtual re::BufferInput GetBufferInput(char const* shaderName) const = 0;

			// Get a BufferInput for a single element within the managed array buffer:
			virtual re::BufferInput GetBufferInput(gr::RenderDataManager const&, IDType, char const* shaderName) const = 0;

			
		public:
			virtual void ShowImGuiWindow() const = 0;


		private:
			virtual IndexType GetIndex(gr::RenderDataManager const&, IDType) const = 0;


		public:
			template<typename LUTBuffer>
			void WriteLUTData(gr::RenderDataManager const& renderData, IDType id, LUTBuffer* dst) const
			{
				// Note: May be invalid if ID is not associated with RenderData of the managed type
				const IndexType lutIdx = GetIndex(renderData, id);
				if (lutIdx == INVALID_RESOURCE_IDX)
				{
					return; // Do nothing
				}

				// Validate the thread protector now that we've got the index:
				m_threadProtector.ValidateThreadAccess();

				const std::type_index typeIdx = std::type_index(typeid(LUTBuffer));
				SEAssert(m_writeLUTDataCallbacks.contains(typeIdx), "No registered LUT writer for this type");

				// Execute the callback:
				m_writeLUTDataCallbacks.at(typeIdx)(lutIdx, dst);
			}


		private:
			void AddLUTDataWriterCallbackInternal(
				std::type_index typeIdx,
				void(*WriteLUTDataCallback)(IndexType lutIdx, void* dst)) override
			{
				util::ScopedThreadProtector lock(m_threadProtector);

				SEAssert(!m_writeLUTDataCallbacks.contains(typeIdx), "Callback already added for the given type");
				m_writeLUTDataCallbacks.emplace(typeIdx, WriteLUTDataCallback);

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
				BufferDataType(*createBufferData)(RenderDataType const&, IDType, gr::RenderDataManager const&),
				char const* bufferName,
				re::Buffer::MemoryPoolPreference memPoolPreference,
				re::Buffer::AccessMask accessMask,
				bool(*FilterCallback)(RenderDataType const*) = nullptr,
				RenderObjectFeature featureBits = RenderObjectFeature::None);

			TypedIndexedBuffer(TypedIndexedBuffer&&) noexcept = default;
			TypedIndexedBuffer& operator=(TypedIndexedBuffer&&) = default;

			~TypedIndexedBuffer() = default;

			bool Clear(); // Returns true if buffer was destroyed
			void Destroy() override;


		public:
			bool UpdateBuffer(gr::RenderDataManager const&) override;
			std::shared_ptr<re::Buffer> GetBuffer() const override;

			// Get a BufferInput for the entire managed array buffer:
			re::BufferInput GetBufferInput(char const* shaderName) const override;

			// Get a BufferInput for a single element within the managed array buffer:
			re::BufferInput GetBufferInput(gr::RenderDataManager const&, IDType, char const* shaderName) const override;


		public:
			void ShowImGuiWindow() const override;


		private:
			IndexType GetIndex(gr::RenderDataManager const&, IDType) const override;


		private:
			std::unordered_map<IDType, IndexType> m_idToBufferIdx;

			// We use a priority queue to ensure that indexes closest to 0 are reused first, to keep packing as tight
			std::priority_queue<IndexType, std::vector<IndexType>, std::greater<IndexType>> m_freeIndexes;

			std::string m_bufferName; // Note: Used for ID/lookup - Is not the shader name
			std::shared_ptr<re::Buffer> m_buffer;

			// We maintain a dummy buffer of a single element, to ensure there is something to return if there is no
			// render data
			std::shared_ptr<re::Buffer> m_dummyBuffer;

			BufferDataType(*m_createBufferData)(RenderDataType const&, IDType, gr::RenderDataManager const&);

			bool(*m_filterCallback)(RenderDataType const*); // If true, the RenderDataType should be included

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
			BufferDataType(*CreateBufferData)(RenderDataType const&, IDType, gr::RenderDataManager const&),
			re::Buffer::MemoryPoolPreference,
			bool(*FilterCallback)(RenderDataType const*) = nullptr, // Optional: All RenderDataType entries included if null
			RenderObjectFeature featureBits = RenderObjectFeature::None);

		// Get a LUT buffer completely auto-populated
		template<typename LUTBuffer>
		re::BufferInput GetLUTBufferInput(char const* shaderName, std::ranges::range auto const& renderDataIDs);

		// Build a LUT buffer using (partially) pre-populated initial data
		template<typename LUTBuffer>
		re::BufferInput GetLUTBufferInput(
			char const* shaderName, std::vector<LUTBuffer>&& LUTData, std::ranges::range auto const& renderDataIDs);

		// Get the data that *would* be populated in a managed LUT. This is intended for debug viewing only
		template<typename LUTBuffer>
		void GetLUTBufferData(std::vector<LUTBuffer>& LUTData, std::ranges::range auto const& renderDataIDs);

		// Get an entire managed array buffer:		
		re::BufferInput GetIndexedBufferInput(util::HashKey bufferNameHash, char const* shaderName) const;
		re::BufferInput GetIndexedBufferInput(char const* bufferName, char const* shaderName) const;

		// Get a BufferInput for a single element of a managed array buffer:
		re::BufferInput GetSingleElementBufferInput(IDType, util::HashKey bufferNameHash, char const* shaderName) const;
		re::BufferInput GetSingleElementBufferInput(IDType, char const* bufferName, char const* shaderName) const;

		std::shared_ptr<re::Buffer const> GetIndexedBuffer(util::HashKey bufferNameHash) const;
		std::shared_ptr<re::Buffer const> GetIndexedBuffer(char const* bufferName) const;

		// Populate the LUT data. This is and internal helper, but is publically exposed for debug output
		template<typename LUTBuffer>
		void PopulateLUTData(std::ranges::range auto&& renderDataIDs, std::span<LUTBuffer> lutBufferData);


	public:
		void ShowImGuiWindow() const;


	private:
		void RegisterLUTWriter(std::type_index typeIdx, IIndexedBufferInternal* indexedBuffer);

		template<typename LUTBuffer>
		std::shared_ptr<re::Buffer> GetLUTBuffer(std::ranges::range auto const& renderDataIDs, IndexType& baseIdxOut);

		template<typename LUTBuffer>
		std::shared_ptr<re::Buffer> GetLUTBuffer(
			std::vector<LUTBuffer>&& initialLUTData,
			std::ranges::range auto&& renderDataIDs,
			IndexType& baseIdxOut);


	private:
		std::vector<std::unique_ptr<IIndexedBufferInternal>> m_indexedBuffers;
		std::multimap<std::type_index, IIndexedBufferInternal*> m_lutWritingBuffers; // LUTBuffer type -> writers
		std::unordered_map<util::HashKey, IIndexedBufferInternal*> m_bufferNameHashToIndexedBuffer;

		gr::RenderDataManager const& m_renderData;

		// We sub-allocate out of permanent Buffer(s); If we outgrow it we create a new, larger Buffer and allow the old
		// one to go out of scope via deferred deletion		
		struct LUTMetadata
		{
			std::shared_ptr<re::Buffer> m_LUTBuffer;


		public:
			bool HasFreeBlock(uint32_t elementCount) const
			{
				return m_LUTBuffer != nullptr &&
					(m_baseIdx + elementCount <= m_LUTBuffer->GetArraySize());
			}

			void Update()
			{
				SEAssert(m_LUTBuffer != nullptr, "Trying to reset before a LUT Buffer has been created");

				constexpr uint8_t k_maxConsecutiveShrinkFrames = 120;

				const uint32_t arraySize = m_LUTBuffer->GetArraySize();
				const uint32_t freeElements = arraySize - m_baseIdx;

				const bool canShrink = arraySize > k_defaultLUTBufferArraySize &&
					(freeElements * k_LUTBufferGrowthFactor) > arraySize;

				if (canShrink)
				{
					if (m_numConsecutiveShrinkFrames++ > k_maxConsecutiveShrinkFrames)
					{
						m_mustShrink = true;
						m_numConsecutiveShrinkFrames = 0;
					}
				}
				else
				{
					m_mustShrink = false;
					m_numConsecutiveShrinkFrames = 0;
				}

				m_baseIdx = 0;
			}

			uint32_t Allocate(uint32_t numElements)
			{
				SEAssert(numElements > 0, "Invalid allocation amount");
				SEAssert(HasFreeBlock(numElements), "Trying to allocate a block but there is not enough room");

				uint32_t allocationBase = m_baseIdx;
				m_baseIdx += numElements;

				return allocationBase;
			}

			bool GetMustShrink() const
			{
				return m_mustShrink;
			}

			void MarkAsShrunk()
			{
				m_numConsecutiveShrinkFrames = 0;
				m_mustShrink = false;
			}


		private:
			uint32_t m_baseIdx = 0; // Reset each frame

			uint64_t m_numConsecutiveShrinkFrames = 0;
			bool m_mustShrink = false;
		};
		std::unordered_map<std::type_index, LUTMetadata> m_LUTTypeToLUTMetadata; // <LUTBuffer> -> LUTMetadata
		std::mutex m_LUTTypeToLUTMetadataMutex;

		static constexpr uint32_t k_defaultLUTBufferArraySize = 16;
		static constexpr float k_LUTBufferGrowthFactor = 2.f;
		static constexpr float k_LUTBufferShrinkFactor = 0.75f; // Add some slop to prevent oscillation

		util::ThreadProtector m_ibmThreadProtector;


	private:
		IndexedBufferManager(IndexedBufferManager const&) = delete;
		IndexedBufferManager& operator=(IndexedBufferManager const&) = delete;
	};


	// ---


	template<typename RenderDataType, typename BufferDataType>
	IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::TypedIndexedBuffer(
		IndexedBufferManager* ibm,
		BufferDataType(*createBufferData)(RenderDataType const&, IDType, gr::RenderDataManager const&),
		char const* bufferName,
		re::Buffer::MemoryPoolPreference memPoolPreference,
		re::Buffer::AccessMask accessMask,
		bool(*FilterCallback)(RenderDataType const*) /*= nullptr*/,
		RenderObjectFeature featureBits /*= RenderObjectFeature::None*/)
		: IIndexedBufferInternal(ibm)
		, m_createBufferData(createBufferData)
		, m_bufferName(bufferName)
		, m_filterCallback(FilterCallback)
		, m_featureBits(featureBits)
		, m_memPoolPreference(memPoolPreference)
		, m_accessMask(accessMask)
	{
		SEAssert(m_createBufferData != nullptr, "Invalid Buffer creation callback");

		std::vector<BufferDataType> dummyData;
		dummyData.emplace_back(BufferDataType{});

		m_dummyBuffer = re::Buffer::CreateArray(
			bufferName, 
			dummyData.data(), 
			re::Buffer::BufferParams{
				.m_lifetime = re::Lifetime::Permanent,
				.m_stagingPool = re::Buffer::StagingPool::Temporary, // Will never be updated
				.m_memPoolPreference = m_memPoolPreference,
				.m_accessMask = m_accessMask,
				.m_usageMask = re::Buffer::Usage::Structured,
				.m_arraySize = util::CheckedCast<uint32_t>(dummyData.size()),
			});
	}


	template<typename RenderDataType, typename BufferDataType>
	bool IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::Clear()
	{
		if (m_buffer)
		{
			m_idToBufferIdx.clear();
			m_freeIndexes = {};
			m_buffer = nullptr;
			
			return true;
		}
		return false;
	}

	template<typename RenderDataType, typename BufferDataType>
	void IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::Destroy()
	{
		util::ScopedThreadProtector lock(m_threadProtector);

		Clear();
	}


	template<typename RenderDataType, typename BufferDataType>
	bool IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::UpdateBuffer(
		gr::RenderDataManager const& renderData)
	{
		SEBeginCPUEvent("IndexedBufferManager::TypedIndexedBuffer::UpdateBuffer");

		util::ScopedThreadProtector lock(m_threadProtector);

		if (renderData.HasAnyDirtyData<RenderDataType>() == false &&
			renderData.HasIDsWithDeletedData<RenderDataType>() == false)
		{
			SEEndCPUEvent();
			return false; // Early out if nothing has changed
		}

		const uint32_t numRenderDataElements = renderData.GetNumElementsOfType<RenderDataType>(m_featureBits);
		if (numRenderDataElements == 0)
		{
			const bool didClear = Clear();
			return didClear;
		}
		
		bool didReallocate = false;
		if (m_buffer == nullptr ||
			m_buffer->GetBufferParams().m_arraySize < numRenderDataElements ||
			m_buffer->GetBufferParams().m_arraySize <= (numRenderDataElements / k_shrinkFactor))
		{
			const uint32_t arraySize = util::RoundUpToNearestMultiple(numRenderDataElements, k_arraySizeAlignment);

			LOG(std::format("Creating indexed buffer from RenderData \"{}\" for buffer data \"{}\", with {} elements",
				std::type_index(typeid(RenderDataType)).name(),
				std::type_index(typeid(BufferDataType)).name(),
				arraySize));

			didReallocate = (m_buffer != nullptr);

			if (!didReallocate)
			{
				m_dummyBuffer = nullptr;
			}

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
			for (IndexType i = 0; i < arraySize; ++i)
			{
				m_freeIndexes.emplace(i);
			}

			// Clear our index map: We'll re-populate it as we build our Buffer data
			m_idToBufferIdx.clear();

			// Build and commit our Buffer data:
			std::vector<BufferDataType> bufferData;
			bufferData.resize(arraySize);

			if constexpr (std::is_same_v<RenderDataType, gr::Transform::RenderData>)
			{
				// Transforms are treated as a special case by the RenderDataManager; We must do the same here
				for (gr::TransformID transformID : renderData.GetRegisteredTransformIDs())
				{
					gr::Transform::RenderData const& transformRenderData = 
						renderData.GetTransformDataFromTransformID(transformID);

					// Execute the filter callback if one was provided:
					if (m_filterCallback && m_filterCallback(&transformRenderData) == false)
					{
						continue;
					}

					SEAssert(!m_freeIndexes.empty(), "No more free indexes. This should not be possible");

					const IndexType currentBufferIdx = m_freeIndexes.top();
					m_freeIndexes.pop();

					bufferData[currentBufferIdx] = m_createBufferData(
						renderData.GetTransformDataFromTransformID(transformID), transformID, renderData);

					m_idToBufferIdx.emplace(transformID, currentBufferIdx);
				}
			}
			else
			{
				gr::ObjectAdapter<RenderDataType> objAdapter(renderData, m_featureBits);
				for (auto const& itr : objAdapter)
				{
					RenderDataType const& objectRenderData = itr->Get<RenderDataType>();
					
					// Execute the filter callback if one was provided:
					if (m_filterCallback && m_filterCallback(&objectRenderData) == false)
					{
						continue;
					}

					SEAssert(!m_freeIndexes.empty(), "No more free indexes. This should not be possible");

					const IndexType currentBufferIdx = m_freeIndexes.top();
					m_freeIndexes.pop();

					bufferData[currentBufferIdx] = m_createBufferData(
						objectRenderData, itr->GetRenderDataID(), renderData);

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
						// An ID might not be found if it was filtered out via the m_filterCallback
						auto idToBufferIdxItr = m_idToBufferIdx.find(deletedID);
						if (idToBufferIdxItr != m_idToBufferIdx.end())
						{
							const IndexType deletedIdx = m_idToBufferIdx.at(deletedID);
							m_idToBufferIdx.erase(deletedID);
							m_freeIndexes.emplace(deletedIdx);
						}
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
						RenderDataType const* data = nullptr;
						if constexpr (std::is_same_v<RenderDataType, gr::Transform::RenderData>)
						{
							data = &renderData.GetTransformDataFromTransformID(dirtyID);
						}
						else
						{
							data = &renderData.GetObjectData<RenderDataType>(dirtyID);
						}

						// Execute the filter callback if one was provided:
						if (m_filterCallback && m_filterCallback(data) == false)
						{
							continue;
						}

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

						BufferDataType const& bufferData = m_createBufferData(*data, dirtyID, renderData);

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
				std::vector<gr::RenderDataID> const& dirtyIDs =
					renderData.GetIDsWithAnyDirtyData<RenderDataType>(m_featureBits);
				ProcessDirtyIDs(dirtyIDs);
			}
		}

		SEAssert(m_idToBufferIdx.size() + m_freeIndexes.size() == m_buffer->GetArraySize(),
			"Indexes are out of sync");

		SEEndCPUEvent();

		return didReallocate;
	}


	template<typename RenderDataType, typename BufferDataType>
	std::shared_ptr<re::Buffer> IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::GetBuffer() const
	{
		m_threadProtector.ValidateThreadAccess();

		if (m_buffer == nullptr)
		{
			return m_dummyBuffer;
		}
		return m_buffer;
	}


	template<typename RenderDataType, typename BufferDataType>
	re::BufferInput IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::GetBufferInput(
		char const* shaderName) const
	{
		return re::BufferInput(shaderName, GetBuffer(), re::Lifetime::SingleFrame);
	}


	template<typename RenderDataType, typename BufferDataType>
	re::BufferInput IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::GetBufferInput(
		gr::RenderDataManager const& renderData, IDType id, char const* shaderName) const
	{
		const uint32_t idx = GetIndex(renderData, id);
		SEAssert(idx != INVALID_RESOURCE_IDX, "Failed to find a valid index for the given ID. Was it registered for this type?");

		return re::BufferInput(
			shaderName,
			GetBuffer(),
			re::BufferView::BufferType{
				.m_firstElement = idx,
				.m_numElements = 1,
				.m_structuredByteStride = sizeof(BufferDataType),
				.m_firstDestIdx = 0,
			},
			re::Lifetime::SingleFrame);
	}


	template<typename RenderDataType, typename BufferDataType>
	IndexedBufferManager::IndexType IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::GetIndex(
		gr::RenderDataManager const& renderData, IDType id) const
	{
		m_threadProtector.ValidateThreadAccess();

		if constexpr (std::is_same_v<RenderDataType, gr::Transform::RenderData>)
		{
			// Transform buffers map TransformID -> buffer index, convert RenderDataID -> TransformID:
			id = renderData.GetTransformIDFromRenderDataID(id);
		}

		auto itr = m_idToBufferIdx.find(id);
		if (itr == m_idToBufferIdx.end())
		{
			return INVALID_RESOURCE_IDX;
		}
		return itr->second;
	}


	template<typename RenderDataType, typename BufferDataType>
	void IndexedBufferManager::TypedIndexedBuffer<RenderDataType, BufferDataType>::ShowImGuiWindow() const
	{
		re::Buffer const* buffer = m_buffer.get();
		if (buffer == nullptr)
		{
			buffer = m_dummyBuffer.get();
		}
		if (buffer == nullptr)
		{
			ImGui::Text("<Null buffer>");
			return;
		}

		if (ImGui::CollapsingHeader(std::format("{}##{}", buffer->GetName(), buffer->GetUniqueID()).c_str()))
		{
			ImGui::Indent();

			if (ImGui::CollapsingHeader(
				std::format("{} registered RenderDataIDs##", m_idToBufferIdx.size(), buffer->GetUniqueID()).c_str()))
			{
				ImGui::Indent();

				constexpr ImGuiTableFlags k_flags = 
					ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

				constexpr int k_numCols = 2;
				if (ImGui::BeginTable(
					std::format("Registered RenderDataIDs##{}", buffer->GetUniqueID()).c_str(), k_numCols, k_flags))
				{
					// Headers:				
					ImGui::TableSetupColumn("RenderObjectID");
					ImGui::TableSetupColumn("Buffer index");

					ImGui::TableHeadersRow();

					for (auto const& entry : m_idToBufferIdx)
					{
						const IDType id = entry.first;
						const IndexType index = entry.second;

						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						// RenderDataID (Ref. count)
						ImGui::Text(std::format("{}", id).c_str());

						ImGui::TableNextColumn();

						ImGui::Text(std::format("{}", index).c_str());

						ImGui::TableNextColumn();
					}

					ImGui::EndTable();
				}
				ImGui::Unindent();
			}
			ImGui::Text(std::format("{} remaining free indexes", m_freeIndexes.size()).c_str());
			ImGui::Text(std::format("Buffer array size: {}", buffer->GetArraySize()).c_str());

			ImGui::NewLine();

			ImGui::Text(std::format("Buffer CBV resource handle: {}", 
				buffer->GetBindlessResourceHandle(re::ViewType::CBV)).c_str());
			ImGui::Text(std::format("Buffer SRV resource handle: {}", 
				buffer->GetBindlessResourceHandle(re::ViewType::SRV)).c_str());

			ImGui::NewLine();

			ImGui::Text(std::format("Filter callback: {}", m_filterCallback ? "Enabled" : "Disabled").c_str());
			ImGui::Text(std::format("Feature bits: {:#010b}", static_cast<uint32_t>(m_featureBits)).c_str());

			ImGui::Unindent();
		}
	}


	// ---


	template<typename RenderDataType, typename BufferDataType>
	IndexedBufferManager::IIndexedBuffer* IndexedBufferManager::AddIndexedBuffer(
		char const* bufferName,
		BufferDataType(*CreateBufferData)(RenderDataType const&, IDType, gr::RenderDataManager const&),
		re::Buffer::MemoryPoolPreference memPool, 
		bool(*FilterCallback)(RenderDataType const*) /*= nullptr*/,
		RenderObjectFeature featureBits /*= RenderObjectFeature::None*/)
	{
		util::ScopedThreadProtector lock(m_ibmThreadProtector);

		re::Buffer::AccessMask access = re::Buffer::Access::GPURead;
		if (memPool == re::Buffer::MemoryPoolPreference::UploadHeap)
		{
			access |= re::Buffer::Access::CPUWrite;
		}

		auto const& itr = m_indexedBuffers.emplace_back(
			std::make_unique<TypedIndexedBuffer<RenderDataType, BufferDataType>>(
				this,
				CreateBufferData,
				bufferName,
				memPool,
				access,
				FilterCallback,
				featureBits));

		m_bufferNameHashToIndexedBuffer.emplace(util::HashKey(bufferName), itr.get());

		return itr.get();
	}


	template<typename LUTBuffer>
	std::shared_ptr<re::Buffer> IndexedBufferManager::GetLUTBuffer(
		std::ranges::range auto const& renderDataIDs,
		IndexType& baseIdxOut)
	{
		// Assemble the minimum required number of initial LUTBuffer data elements
		std::vector<LUTBuffer> initialLUTData;
		initialLUTData.resize(renderDataIDs.size());

		return GetLUTBuffer<LUTBuffer>(std::move(initialLUTData), renderDataIDs, baseIdxOut);
	}


	template<typename LUTBuffer>
	std::shared_ptr<re::Buffer> IndexedBufferManager::GetLUTBuffer(
		std::vector<LUTBuffer>&& initialLUTData,
		std::ranges::range auto&& renderDataIDs,
		IndexType& baseIdxOut)
	{
		SEBeginCPUEvent("IndexedBufferManager::GetLUTBuffer");
		
		util::ScopedThreadProtector lock(m_ibmThreadProtector);

		const std::type_index lutTypeIdx = std::type_index(typeid(LUTBuffer));

		SEAssert(m_lutWritingBuffers.contains(lutTypeIdx),
			"No indexed buffers have a registered LUT data writer of this type");

		SEAssert(m_LUTTypeToLUTMetadata.contains(lutTypeIdx),
			"No LUT buffer entry exists. It should have already been added");

		baseIdxOut = 0; // Initialize with a dummy value

		// We'll pad the initial data out if we have too many/too few elements:
		const uint32_t requiredSize = util::CheckedCast<uint32_t>(renderDataIDs.size());

		auto lutMetadataItr = m_LUTTypeToLUTMetadata.find(lutTypeIdx);

		const bool hasBuffer = lutMetadataItr->second.m_LUTBuffer != nullptr;
		const bool mustGrow = hasBuffer && lutMetadataItr->second.HasFreeBlock(requiredSize) == false;
		const bool mustShrink = !mustGrow && lutMetadataItr->second.GetMustShrink();

		const bool mustReallocate = !hasBuffer || mustGrow || mustShrink;
		if (mustReallocate)
		{
			if (mustGrow)
			{
				const uint32_t nextSize = 
					static_cast<uint32_t>(lutMetadataItr->second.m_LUTBuffer->GetArraySize() * k_LUTBufferGrowthFactor);

				const uint32_t expandedSize = std::max(requiredSize, nextSize);

				initialLUTData.resize(expandedSize);
			}
			else if (mustShrink)
			{
				const uint32_t nextSize = util::CheckedCast<uint32_t>(std::max(
					requiredSize,
					std::max(k_defaultLUTBufferArraySize,
						static_cast<uint32_t>(lutMetadataItr->second.m_LUTBuffer->GetArraySize() * k_LUTBufferShrinkFactor))));

				initialLUTData.resize(nextSize);

				lutMetadataItr->second.MarkAsShrunk();
			}
			else if (initialLUTData.size() < k_defaultLUTBufferArraySize) // Ensure a minimum size
			{
				initialLUTData.resize(k_defaultLUTBufferArraySize);
			}

			LOG(std::format("{} indexed buffer LUT for type \"{}\", ({} elements)",
				(hasBuffer ? (mustGrow ? "Growing" : "Shrinking") : "Creating"),
				lutTypeIdx.name(),
				initialLUTData.size()));

			// Populate the intial entries with LUT data for our RenderDataIDs:
			PopulateLUTData<LUTBuffer>(renderDataIDs, std::span<LUTBuffer>(initialLUTData.begin(), renderDataIDs.size()));

			// Create the buffer:
			lutMetadataItr->second.m_LUTBuffer = re::Buffer::CreateArray(
				std::format("{}_ManagedLUT", lutTypeIdx.name()),
				initialLUTData.data(),
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::Permanent,
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Structured,
					.m_arraySize = util::CheckedCast<uint32_t>(initialLUTData.size()),
				});

			lutMetadataItr->second.Update(); // Reset the LUT block allocation tracking

			if (!renderDataIDs.empty()) // Otherwise, will still return baseIdxOut = 0 for dummy buffers
			{
				baseIdxOut = lutMetadataItr->second.Allocate(util::CheckedCast<uint32_t>(renderDataIDs.size()));
			}
		}
		else
		{
			SEAssert(lutMetadataItr->second.HasFreeBlock(requiredSize),
				"Not enough space to place the new entries, this should not be possible");

			if (requiredSize > 0)
			{
				SEAssert(initialLUTData.empty() || requiredSize <= initialLUTData.size(),
					"Initial data must be empty, or have at least 1 entry per ID");
				if (initialLUTData.empty())
				{
					initialLUTData.resize(requiredSize);
				}

				baseIdxOut = lutMetadataItr->second.Allocate(requiredSize);

				// Record our current entries:
				PopulateLUTData<LUTBuffer>(renderDataIDs, std::span<LUTBuffer>(initialLUTData));

				// Commit the updated data:
				lutMetadataItr->second.m_LUTBuffer->Commit(
					initialLUTData.data(),
					baseIdxOut,
					util::CheckedCast<uint32_t>(initialLUTData.size()));
			}
		}

		initialLUTData.clear(); // This is an R-value and we're done with it. Free it for the caller

		SEEndCPUEvent();

		return lutMetadataItr->second.m_LUTBuffer;
	}


	template<typename LUTBuffer>
	void IndexedBufferManager::PopulateLUTData(
		std::ranges::range auto&& renderDataIDs, std::span<LUTBuffer> lutBufferData)
	{
		const std::type_index lutTypeIdx = std::type_index(typeid(LUTBuffer));

		// Multiple writers may write to the same LUTBuffer type:
		auto entries = m_lutWritingBuffers.equal_range(lutTypeIdx);
		for (auto& itr = entries.first; itr != entries.second; ++itr)
		{
			for (size_t writeIdx = 0; writeIdx < renderDataIDs.size(); ++writeIdx)
			{
				itr->second->WriteLUTData<LUTBuffer>(
					m_renderData,
					renderDataIDs[writeIdx],
					&lutBufferData[writeIdx]);
			}
		}
	}


	template<typename LUTBuffer>
	inline re::BufferInput IndexedBufferManager::GetLUTBufferInput(
		char const* shaderName, std::ranges::range auto const& renderDataIDs)
	{
		return GetLUTBufferInput(shaderName, std::vector<LUTBuffer>(), renderDataIDs);
	}


	template<typename LUTBuffer>
	re::BufferInput IndexedBufferManager::GetLUTBufferInput(
		char const* shaderName,
		std::vector<LUTBuffer>&& initialLUTData,
		std::ranges::range auto const& renderDataIDs)
	{
		const std::type_index lutTypeIdx = std::type_index(typeid(LUTBuffer));

		// Critical section: Get/create a LUT BufferInput
		{
			std::lock_guard<std::mutex> lock(m_LUTTypeToLUTMetadataMutex);

			// Create a metadata entry:
			auto metadataItr = m_LUTTypeToLUTMetadata.find(lutTypeIdx);
			if (metadataItr == m_LUTTypeToLUTMetadata.end())
			{
				metadataItr = m_LUTTypeToLUTMetadata.emplace(lutTypeIdx, LUTMetadata{}).first;
			}

			IndexType firstElement = std::numeric_limits<IndexType>::max();

			std::shared_ptr<re::Buffer> const& lutBuffer = 
				GetLUTBuffer<LUTBuffer>(std::move(initialLUTData), renderDataIDs, firstElement);
			SEAssert(firstElement != std::numeric_limits<IndexType>::max(), "Failed to get a valid 1st element");

			return re::BufferInput(
				shaderName,
				lutBuffer,
				re::BufferView::BufferType{
					.m_firstElement = firstElement,
					.m_numElements = util::CheckedCast<uint32_t>(renderDataIDs.size()),
					.m_structuredByteStride = sizeof(LUTBuffer),
					.m_firstDestIdx = 0,
				},
				re::Lifetime::SingleFrame);
		}
	}


	template<typename LUTBuffer>
	void IndexedBufferManager::GetLUTBufferData(
		std::vector<LUTBuffer>& LUTData, std::ranges::range auto const& renderDataIDs)
	{
		const std::type_index lutTypeIdx = std::type_index(typeid(LUTBuffer));

		// Critical section: Get/create a LUT BufferInput
		{
			std::lock_guard<std::mutex> lock(m_LUTTypeToLUTMetadataMutex);

			PopulateLUTData<LUTBuffer>(renderDataIDs, LUTData);
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


	inline re::BufferInput IndexedBufferManager::GetSingleElementBufferInput(
		IDType id, util::HashKey bufferNameHash, char const* shaderName) const
	{
		return m_bufferNameHashToIndexedBuffer.at(bufferNameHash)->GetBufferInput(m_renderData, id, shaderName);
	}


	inline re::BufferInput IndexedBufferManager::GetSingleElementBufferInput(
		IDType id, char const* bufferName, char const* shaderName) const
	{		
		return GetSingleElementBufferInput(id, util::HashKey(bufferName), shaderName);
	}


	inline std::shared_ptr<re::Buffer const> IndexedBufferManager::GetIndexedBuffer(util::HashKey bufferNameHash) const
	{
		SEAssert(m_bufferNameHashToIndexedBuffer.contains(bufferNameHash), "Buffer name not found");
		return m_bufferNameHashToIndexedBuffer.at(bufferNameHash)->GetBuffer();
	}


	inline std::shared_ptr<re::Buffer const> IndexedBufferManager::GetIndexedBuffer(char const* bufferName) const
	{
		return GetIndexedBuffer(util::HashKey(bufferName));
	}


	inline void IndexedBufferManager::RegisterLUTWriter(std::type_index typeIdx, IIndexedBufferInternal* indexedBuffer)
	{
		util::ScopedThreadProtector lock(m_ibmThreadProtector);

		m_lutWritingBuffers.emplace(typeIdx, indexedBuffer);
	}
}