// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"
#include "TransformComponent.h"

#include "Core/Assert.h"
#include "Core/Util/CastUtils.h"
#include "Core/Util/ThreadProtector.h"


namespace gr
{
	// Render-thread-side scene data. Data is set via the render command queue (on a single thread), and graphics
	// systems index use constant forward iterators to access it
	class RenderDataManager
	{
	public:
		RenderDataManager();
		~RenderDataManager() = default;

		void Destroy();

		void BeginFrame(uint64_t currentFrame);

		// Render data interface:
		void RegisterObject(gr::RenderDataID, gr::TransformID);
		void DestroyObject(gr::RenderDataID);

		template<typename T>
		void SetObjectData(gr::RenderDataID, T const*);

		// To ensure this is thread safe, objects can only be accessed once all updates are complete (i.e. after all
		// render commands have been executed)
		template<typename T>
		[[nodiscard]] T const& GetObjectData(gr::RenderDataID) const;

		template<typename T>
		[[nodiscard]] bool HasObjectData(gr::RenderDataID) const;
		
		template<typename T>
		[[nodiscard]] bool HasObjectData() const; // Does data of the given type exist for any ID

		template<typename T>
		[[nodiscard]] bool HasIDsWithNewData() const;

		// Get a list of IDs that had data of a specific type added for the very first time this frame
		template<typename T>
		[[nodiscard]] std::vector<gr::RenderDataID> const* GetIDsWithNewData() const;

		template<typename T>
		[[nodiscard]] bool HasIDsWithDeletedData() const;

		template<typename T>
		std::vector<gr::RenderDataID> const* GetIDsWithDeletedData() const;

		// Get a list of IDs that had data of a specific type modified (i.e. SetObjectData() was called) this frame
		template<typename T>
		std::vector<gr::RenderDataID> const* GetIDsWithDirtyData() const;

		template<typename T>
		[[nodiscard]] bool IsDirty(gr::RenderDataID) const;

		template<typename T>
		void DestroyObjectData(gr::RenderDataID);

		template<typename T>
		[[nodiscard]] uint32_t GetNumElementsOfType() const;

		void SetFeatureBits(gr::RenderDataID, gr::FeatureBitmask); // Logical OR
		
		[[nodiscard]] gr::FeatureBitmask GetFeatureBits(gr::RenderDataID) const;

		// Get IDs associated with a type
		template<typename T>
		[[nodiscard]] std::vector<gr::RenderDataID> const& GetRegisteredRenderDataIDs() const;

		// Get all RenderDataIDs (regardless of associated data types)
		[[nodiscard]] std::vector<gr::RenderDataID> const& GetRegisteredRenderDataIDs() const; 

		[[nodiscard]] std::vector<gr::RenderDataID> const& GetRegisteredTransformIDs() const;


	public:
		void ShowImGuiWindow() const;
	private:
		template<typename T>
		void PopulateTypesImGuiHelper(std::vector<std::string>&, char const*) const;


	private:
		// Transform interface.
		// We treat Transforms as a special case because all render objects require a Transform, and we expect
		// Transforms to be the largest and most frequently updated data mirrored on the render thread. It's likely many
		// render objects share a transform (e.g. multiple mesh primitives), so we can minimize duplicated effort.
		void RegisterTransform(gr::TransformID);
		void UnregisterTransform(gr::TransformID);

	public:
		void SetTransformData(gr::TransformID, gr::Transform::RenderData const&);
		
		[[nodiscard]] gr::Transform::RenderData const& GetTransformDataFromTransformID(gr::TransformID) const;

		// Note: This function is slower than direct access via the TransformID. If you have a TransformID, use it
		[[nodiscard]] gr::Transform::RenderData const& GetTransformDataFromRenderDataID(gr::RenderDataID) const; // Slower than using TransformID
		
		[[nodiscard]] bool TransformIsDirty(gr::TransformID) const; // Was the Transform updated in the current frame?
		[[nodiscard]] bool TransformIsDirtyFromRenderDataID(gr::RenderDataID) const; // Slower than using TransformID

		[[nodiscard]] std::vector<gr::TransformID> const& GetIDsWithDirtyTransformData() const;


	private:
		typedef uint8_t DataTypeIndex;
		typedef uint32_t DataIndex;
		typedef std::map<DataTypeIndex, DataIndex> ObjectTypeToDataIndexMap; // [data type index] == object index

		typedef std::map<DataTypeIndex, uint64_t> LastDirtyFrameMap; // [data type index] == last dirty frame
		static constexpr uint64_t k_invalidDirtyFrameNum = std::numeric_limits<uint64_t>::max();

		struct RenderObjectMetadata
		{
			ObjectTypeToDataIndexMap m_dataTypeToDataIndexMap;
			LastDirtyFrameMap m_dirtyFrameMap;

			gr::TransformID m_transformID;

			FeatureBitmask m_featureBits; // To assist in interpreting render data

			uint32_t m_referenceCount;

			RenderObjectMetadata(gr::TransformID transformID)
				: m_transformID(transformID), m_featureBits(0), m_referenceCount(1) {}
			RenderObjectMetadata() = delete;
		};

		struct TransformMetadata
		{
			DataIndex m_transformIdx;
			uint32_t m_referenceCount;

			uint64_t m_dirtyFrame;
		};

		// Convenience helpers: We track the currently registered IDs seperately for external use. We maintain these in
		// sorted order
		std::vector<gr::RenderDataID> m_registeredRenderObjectIDs;
		std::vector<gr::TransformID> m_registeredTransformIDs;

		std::vector<std::vector<gr::RenderDataID>> m_perTypeRegisteredRenderDataIDs;

		// New IDs/IDs with new types of data added in the current frame
		std::vector<std::vector<gr::RenderDataID>> m_perFramePerTypeNewDataIDs;
		
		// IDs/IDs with data deleted in the current frame
		std::vector<std::vector<gr::RenderDataID>> m_perFramePerTypeDeletedDataIDs;

		// IDs that had data of a given type modified in the current frame. We track the IDs we've modified so we don't 
		// double-add IDs to the vector
		std::vector<std::vector<gr::RenderDataID>> m_perFramePerTypeDirtyDataIDs;
		std::vector<std::unordered_set<gr::RenderDataID>> m_perFramePerTypeDirtySeenDataIDs; 
		
		std::vector<gr::TransformID> m_perFrameDirtyTransformIDs;
		std::unordered_set<gr::TransformID> m_perFrameSeenDirtyTransformIDs;


	public:
		// LinearIterator: Iterate over a single type of data, in whatever order it is stored in memory.
		// This is the fastest iterator type, but elements are accessed out of order with respect to the elements of
		// different data types with the same gr::RenderDataID.
		// RenderDataManager iterators are not thread safe.
		template <typename T>
		class LinearIterator
		{	
		protected:
			friend class gr::RenderDataManager;
			LinearIterator(T const* beginPtr, T const* endPtr) : m_ptr(beginPtr), m_endPtr(endPtr) {}


		public:
			[[nodiscard]] T const& operator*() const { return *m_ptr; }
			[[nodiscard]] T const* operator->() { return m_ptr; }

			template<typename T>
			[[nodiscard]] T const& Get() const { return *m_ptr; }

			LinearIterator& operator++(); // Prefix increment
			LinearIterator operator++(int); // Postfix increment

			// These are declared as friends so they're not classified as member functions
			friend bool operator==(LinearIterator const& lhs, LinearIterator const& rhs) { return lhs.m_ptr == rhs.m_ptr; }
			friend bool operator!=(LinearIterator const& lhs, LinearIterator const& rhs) { return lhs.m_ptr != rhs.m_ptr; }


		private:
			T const* m_ptr;
			T const* m_endPtr;
		};


	public:
		// Iterate over multiple data types, with each iteration's elements associated by gr::RenderDataID.
		// This is slower than a LinearIterator, but elements of different data types are guaranteed to be assocaited
		// with the same gr::RenderDataID.
		// RenderDataManager iterators are not thread safe.
		template <typename... Ts>
		class ObjectIterator 
		{			
		protected:
			friend class gr::RenderDataManager;
			ObjectIterator(
				gr::RenderDataManager const* renderData,
				std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator renderObjectMetadataBeginItr,
				std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator const renderObjectMetadataEndItr,
				std::tuple<Ts const*...> endPtrs,
				uint64_t currentFrame);


		public:
			template<typename T>
			[[nodiscard]] T const& Get() const;

			template<typename T>
			[[nodiscard]] bool IsDirty() const;

			gr::RenderDataID GetRenderDataID() const;

			[[nodiscard]] gr::Transform::RenderData const& GetTransformData() const;
			[[nodiscard]] bool TransformIsDirty() const;

			[[nodiscard]] gr::FeatureBitmask GetFeatureBits() const;

			ObjectIterator& operator++(); // Prefix increment
			ObjectIterator operator++(int); // Postfix increment

			// These are declared as friends so they're not classified as member functions
			friend bool operator==(ObjectIterator const& lhs, ObjectIterator const& rhs) { return lhs.m_ptrs == rhs.m_ptrs; }
			friend bool operator!=(ObjectIterator const& lhs, ObjectIterator const& rhs) { return lhs.m_ptrs != rhs.m_ptrs; }


		private:
			template <typename T>
			T const* GetPtrFromCurrentObjectDataIndicesItr() const;


		private:
			std::tuple<Ts const*...> m_ptrs;
			std::tuple<Ts const*...> m_endPtrs;

			gr::RenderDataManager const* m_renderData;
			std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator m_renderObjectMetadataItr;
			std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator const m_renderObjectMetadataEndItr;

			uint64_t m_currentFrame;
		};


	public:
		// Iterate over objects via a vector of RenderDataIDs. This is largely a convenience iterator; it functions 
		// similarly to calling RenderDataManager::GetObjectData with each RenderDataID in the supplied vector, except
		// the results of the RenderDataID -> RenderObjectMetadata lookup are cached when the iterator is incremented.
		// RenderDataManager iterators are not thread safe.
		class IDIterator
		{			
		protected:
			friend class gr::RenderDataManager;
			IDIterator(
				gr::RenderDataManager const*, 
				std::vector<gr::RenderDataID>::const_iterator,
				std::vector<gr::RenderDataID>::const_iterator,
				std::unordered_map<gr::RenderDataID, RenderObjectMetadata> const*,
				uint64_t currentFrame);


		public:
			template<typename T>
			[[nodiscard]] bool HasObjectData() const;

			template<typename T>
			[[nodiscard]] T const& Get() const;

			template<typename T>
			[[nodiscard]] bool IsDirty() const;

			gr::RenderDataID GetRenderDataID() const;
			gr::TransformID GetTransformID() const;

			gr::Transform::RenderData const& GetTransformData() const;
			[[nodiscard]] bool TransformIsDirty() const;

			[[nodiscard]] gr::FeatureBitmask GetFeatureBits() const;

			IDIterator& operator++(); // Prefix increment
			IDIterator operator++(int); // Postfix increment

			// These are declared as friends so they're not classified as member functions
			friend bool operator==(IDIterator const& lhs, IDIterator const& rhs) { return lhs.m_idsIterator == rhs.m_idsIterator; }
			friend bool operator!=(IDIterator const& lhs, IDIterator const& rhs) { return lhs.m_idsIterator != rhs.m_idsIterator; }


		private:
			gr::RenderDataManager const* m_renderData;
			std::vector<gr::RenderDataID>::const_iterator m_idsIterator;
			std::vector<gr::RenderDataID>::const_iterator const m_idsEndIterator;

			std::unordered_map<gr::RenderDataID, RenderObjectMetadata> const* m_IDToRenderObjectMetadata;
			std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator m_currentObjectMetadata;

			uint64_t m_currentFrame;
		};


	public:
		template <typename T>
		LinearIterator<T> Begin() const;

		template <typename T>
		LinearIterator<T> End() const;

		template <typename... Ts>
		ObjectIterator<Ts...> ObjectBegin() const;

		template <typename... Ts>
		ObjectIterator<Ts...> ObjectEnd() const;

		IDIterator IDBegin(std::vector<gr::RenderDataID> const&) const;
		
		IDIterator IDEnd(std::vector<gr::RenderDataID> const&) const;


	private:
		template<typename T>
		DataTypeIndex GetAllocateDataIndexFromType();

		template<typename T>
		DataTypeIndex GetDataIndexFromType() const;

		template<typename T>
		T const* GetObjectDataVectorIfExists(DataIndex) const;

		template <typename T>
		T const* GetEndPtr() const; // Iterator begin/end helper


	private:
		static constexpr DataTypeIndex k_invalidDataTypeIdx = std::numeric_limits<DataTypeIndex>::max();

		uint64_t m_currentFrame;

		// Each type of render data is tightly packed into an array maintained in m_dataVectors
		std::map<size_t, DataTypeIndex> m_typeInfoHashToDataVectorIdx;
		std::vector<std::shared_ptr<void>> m_dataVectors; // Use shared_ptr because it type erases

		// Render objects are represented as a set of indexes into arrays of typed data (meshes, materials, etc).
		// Each render object maps to 0 or 1 instance of each data type
		std::unordered_map<gr::RenderDataID, RenderObjectMetadata> m_IDToRenderObjectMetadata;

		// Every render object has a transform, but many render objects share the same transform (E.g. mesh primitives).
		// We expect Transforms to be both our largest and most frequently updated data mirrored in RenderDataManager, so we
		// treat them as a special case to allow sharing
		std::unordered_map<gr::TransformID, TransformMetadata> m_transformIDToTransformMetadata;
		std::vector<gr::Transform::RenderData> m_transformRenderData;

		// RenderDataManager accesses are all const, and we only update the RenderData via RenderCommands which are processed
		// single-threaded at the beginning of a render thread frame. Thus, we don't have any syncronization primitives;
		// we just use a thread protector to guard against any mistakes
		mutable util::ThreadProtector m_threadProtector;
	};


	template<typename T>
	void RenderDataManager::SetObjectData(gr::RenderDataID renderDataID, T const* data)
	{
		static const DataTypeIndex s_dataTypeIndex = GetAllocateDataIndexFromType<T>();

		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		SEAssert(s_dataTypeIndex < m_dataVectors.size(), "Data type index is OOB");
		std::vector<T>& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[s_dataTypeIndex]).get();

		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "Invalid object ID");
		RenderObjectMetadata& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);

		// If our tracking tables don't have enough room for the data type index, increase their size
		if (s_dataTypeIndex >= m_perTypeRegisteredRenderDataIDs.size())
		{
			m_perTypeRegisteredRenderDataIDs.resize(s_dataTypeIndex + 1);
		}
		if (s_dataTypeIndex >= m_perFramePerTypeNewDataIDs.size())
		{
			m_perFramePerTypeNewDataIDs.resize(s_dataTypeIndex + 1);
		}
		if (s_dataTypeIndex >= m_perFramePerTypeDeletedDataIDs.size())
		{
			m_perFramePerTypeDeletedDataIDs.resize(s_dataTypeIndex + 1);
		}
		if (s_dataTypeIndex >= m_perFramePerTypeDirtyDataIDs.size())
		{
			m_perFramePerTypeDirtyDataIDs.resize(s_dataTypeIndex + 1);
		}
		if (s_dataTypeIndex >= m_perFramePerTypeDirtySeenDataIDs.size())
		{
			m_perFramePerTypeDirtySeenDataIDs.resize(s_dataTypeIndex + 1);
		}

		// Add/update the dirty frame number:
		renderObjectMetadata.m_dirtyFrameMap[s_dataTypeIndex] = m_currentFrame;

		// Get the index of the data in data vector for its type
		auto const& objectTypeToDataIndex = renderObjectMetadata.m_dataTypeToDataIndexMap.find(s_dataTypeIndex);
		if (objectTypeToDataIndex == renderObjectMetadata.m_dataTypeToDataIndexMap.end())
		{
			// This is the first time we've added data for this object, we must store the destination index
			const DataIndex newDataIndex = util::CheckedCast<DataIndex>(dataVector.size());
			dataVector.emplace_back(*data);
			renderObjectMetadata.m_dataTypeToDataIndexMap.emplace(s_dataTypeIndex , newDataIndex);

			// Record the RenderDataID in our per-type registration list
			m_perTypeRegisteredRenderDataIDs[s_dataTypeIndex].emplace_back(renderDataID);

			// Record the RenderDataID in the per-frame new data type tracker:
			m_perFramePerTypeNewDataIDs[s_dataTypeIndex].emplace_back(renderDataID);
		}
		else
		{
			const DataIndex dataIndex = objectTypeToDataIndex->second;
			dataVector[dataIndex] = *data;
		}

		// Record the RenderDataID in the per-frame dirty data tracker:
		if (m_perFramePerTypeDirtySeenDataIDs[s_dataTypeIndex].emplace(renderDataID).second)
		{
			m_perFramePerTypeDirtyDataIDs[s_dataTypeIndex].emplace_back(renderDataID);
		}
	}


	template<typename T>
	T const& RenderDataManager::GetObjectData(gr::RenderDataID renderDataID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "renderDataID is not registered");

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		SEAssert(dataTypeIndex != k_invalidDataTypeIdx && dataTypeIndex < m_dataVectors.size(),
			"Invalid data type index. This suggests we're accessing data of a specific type using an index, when "
			"no data of that type exists");
		
		RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);
		SEAssert(renderObjectMetadata.m_dataTypeToDataIndexMap.contains(dataTypeIndex),
			"Metadata does not have an entry for the current data type");

		const DataIndex dataIdx = renderObjectMetadata.m_dataTypeToDataIndexMap.find(dataTypeIndex)->second;

		std::vector<T> const& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[dataTypeIndex]).get();
		SEAssert(dataIdx < dataVector.size(), "Object index is OOB");

		return dataVector[dataIdx];
	}


	template<typename T>
	bool RenderDataManager::HasObjectData(gr::RenderDataID renderDataID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "renderDataID is not registered");

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		SEAssert(dataTypeIndex == k_invalidDataTypeIdx || dataTypeIndex < m_dataVectors.size(),
			"Out of bounds data type index received. This shouldn't be possible");

		if (dataTypeIndex != k_invalidDataTypeIdx)
		{
			RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);

			return renderObjectMetadata.m_dataTypeToDataIndexMap.contains(dataTypeIndex);
		}
		return false;
	}


	template<typename T>
	bool RenderDataManager::HasObjectData() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();

		return dataTypeIndex != k_invalidDataTypeIdx;
	}


	template<typename T>
	bool RenderDataManager::HasIDsWithNewData() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();

		return dataTypeIndex < m_perFramePerTypeNewDataIDs.size() && 
			!m_perFramePerTypeNewDataIDs[dataTypeIndex].empty();
	}


	template<typename T>
	std::vector<gr::RenderDataID> const* RenderDataManager::GetIDsWithNewData() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex != k_invalidDataTypeIdx)
		{
			SEAssert(dataTypeIndex < m_perFramePerTypeNewDataIDs.size(), "Data type index is OOB");
			return &m_perFramePerTypeNewDataIDs[dataTypeIndex];
		}
		return nullptr;
	}


	template<typename T>
	bool RenderDataManager::HasIDsWithDeletedData() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();

		return dataTypeIndex < m_perFramePerTypeDeletedDataIDs.size() &&
			!m_perFramePerTypeDeletedDataIDs[dataTypeIndex].empty();
	}


	template<typename T>
	std::vector<gr::RenderDataID> const* RenderDataManager::GetIDsWithDeletedData() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex != k_invalidDataTypeIdx)
		{
			SEAssert(dataTypeIndex < m_perFramePerTypeDeletedDataIDs.size(), "Data type index is OOB");
			return &m_perFramePerTypeDeletedDataIDs[dataTypeIndex];
		}
		return nullptr;			
	}


	template<typename T>
	std::vector<gr::RenderDataID> const* RenderDataManager::GetIDsWithDirtyData() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex != k_invalidDataTypeIdx)
		{
			SEAssert(dataTypeIndex < m_perFramePerTypeDirtyDataIDs.size(), "Data type index is OOB");
			return &m_perFramePerTypeDirtyDataIDs[dataTypeIndex];
		}
		return nullptr;
	}


	template<typename T>
	bool RenderDataManager::IsDirty(gr::RenderDataID renderDataID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "renderDataID is not registered");

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		SEAssert(dataTypeIndex != k_invalidDataTypeIdx && dataTypeIndex < m_dataVectors.size(),
			"Invalid data type index. This suggests we're accessing data of a specific type using an index, when "
			"no data of that type exists");

		RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);
		SEAssert(renderObjectMetadata.m_dirtyFrameMap.contains(dataTypeIndex),
			"Metadata dirty frame map does not have an entry for the current data type");
		
		SEAssert(renderObjectMetadata.m_dirtyFrameMap.at(dataTypeIndex) != k_invalidDirtyFrameNum &&
			renderObjectMetadata.m_dirtyFrameMap.at(dataTypeIndex) <= m_currentFrame &&
			m_currentFrame != k_invalidDirtyFrameNum,
			"Invalid dirty frame value");

		return renderObjectMetadata.m_dirtyFrameMap.at(dataTypeIndex) == m_currentFrame;
	}


	template<typename T>
	T const* RenderDataManager::GetObjectDataVectorIfExists(DataIndex dataIndex) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex == k_invalidDataTypeIdx)
		{
			return nullptr;
		}
		
		std::vector<T> const& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[dataTypeIndex]).get();

		return dataIndex < dataVector.size() ? &dataVector[dataIndex] : nullptr;
	}


	template<typename T>
	uint32_t RenderDataManager::GetNumElementsOfType() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex == k_invalidDataTypeIdx)
		{
			return 0;
		}

		std::vector<T> const& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[dataTypeIndex]).get();
		
		return util::CheckedCast<uint32_t>(dataVector.size());
	}


	template<typename T>
	std::vector<gr::RenderDataID> const& RenderDataManager::GetRegisteredRenderDataIDs() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		SEAssert(dataTypeIndex != k_invalidDataTypeIdx, "No RenderDataIDs are associated with this type");

		return m_perTypeRegisteredRenderDataIDs[dataTypeIndex];
	}


	inline std::vector<gr::RenderDataID> const& RenderDataManager::GetRegisteredRenderDataIDs() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return m_registeredRenderObjectIDs;
	}


	inline std::vector<gr::RenderDataID> const& RenderDataManager::GetRegisteredTransformIDs() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return m_registeredTransformIDs;
	}


	template<typename T>
	void RenderDataManager::DestroyObjectData(gr::RenderDataID renderDataID)
	{
		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();

		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		SEAssert(dataTypeIndex < m_dataVectors.size(), "Data index is OOB");
		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "Invalid object ID");
		RenderObjectMetadata& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);
		
		SEAssert(renderObjectMetadata.m_dataTypeToDataIndexMap.contains(dataTypeIndex),
			"Data type index is not found in the metadata table");

		SEAssert(dataTypeIndex < m_perTypeRegisteredRenderDataIDs.size(),
			"Data type index is OOB of our per-type registration lists");

		// Ensure we've got a vector allocated for the given data type in our deleted data ID tracker
		if (dataTypeIndex >= m_perFramePerTypeDeletedDataIDs.size())
		{
			m_perFramePerTypeDeletedDataIDs.emplace_back();
		}

		std::vector<T>& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[dataTypeIndex]).get();

		// Replace our dead element with one from the end:
		const DataIndex indexToMove = util::CheckedCast<DataIndex>(dataVector.size() - 1);
		const DataIndex indexToReplace = renderObjectMetadata.m_dataTypeToDataIndexMap.find(dataTypeIndex)->second;

		// Move the data:
		if (indexToMove != indexToReplace)
		{
			dataVector[indexToReplace] = dataVector[indexToMove];
		}
		dataVector.pop_back();


		// Walk the list of RenderDataIDs in our registration list for the data type:
		//	a) Update the table referencing the index we moved to its new location
		//	b) Find the location of the RenderObjectID in our per-type registration list
		//	When we've done both, we can stop searching
		bool foundObjectTypeToDataIndexMap = false;
		bool foundPerTypeRegistrationIndex = false;
		size_t perTypeIDIndexToDelete = std::numeric_limits<size_t>::max();
		for (size_t idIndex = 0; idIndex < m_perTypeRegisteredRenderDataIDs[dataTypeIndex].size(); idIndex++)
		{
			gr::RenderDataID currentRenderDataID = m_perTypeRegisteredRenderDataIDs[dataTypeIndex][idIndex];

			// Check: Is this the index of per-type RenderObjectID record we'll be removing?
			if (currentRenderDataID == renderDataID)
			{
				perTypeIDIndexToDelete = idIndex;
				foundPerTypeRegistrationIndex = true;
			}

			if (!foundObjectTypeToDataIndexMap)
			{
				// Find the RenderObjectMetadata record we need to update:
				auto idToObjectMetadataItr = m_IDToRenderObjectMetadata.find(currentRenderDataID);
				SEAssert(idToObjectMetadataItr != m_IDToRenderObjectMetadata.end(),
					"Could not find registered ID in the ID to object metadata map");

				ObjectTypeToDataIndexMap& typeToIndexMapToUpdate =
					idToObjectMetadataItr->second.m_dataTypeToDataIndexMap;

				auto dataTypeToIndexItr = typeToIndexMapToUpdate.find(dataTypeIndex);
				if (dataTypeToIndexItr != typeToIndexMapToUpdate.end() && dataTypeToIndexItr->second == indexToMove)
				{
					dataTypeToIndexItr->second = indexToReplace;

					foundObjectTypeToDataIndexMap = true;
				}
			}			

			if (foundObjectTypeToDataIndexMap && foundPerTypeRegistrationIndex)
			{
				break;
			}
		}
		SEAssert(foundObjectTypeToDataIndexMap && foundPerTypeRegistrationIndex, "Matching object was not found. This should not be possible");

		// Remove the RenderDataID from the per-type registration list:
		m_perTypeRegisteredRenderDataIDs[dataTypeIndex].erase(
			m_perTypeRegisteredRenderDataIDs[dataTypeIndex].begin() + perTypeIDIndexToDelete);

		// Add the RenderDataID to the per-frame deleted data tracker:
		m_perFramePerTypeDeletedDataIDs[dataTypeIndex].emplace_back(renderDataID);

		// Finally, remove the index in the object's data index map:
		renderObjectMetadata.m_dataTypeToDataIndexMap.erase(dataTypeIndex);
		renderObjectMetadata.m_dirtyFrameMap.erase(dataTypeIndex);
	}


	template<typename T>
	RenderDataManager::DataTypeIndex RenderDataManager::GetAllocateDataIndexFromType()
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		static DataTypeIndex s_dataTypeIdx = k_invalidDataTypeIdx;
		if (s_dataTypeIdx == k_invalidDataTypeIdx)
		{
			s_dataTypeIdx = util::CheckedCast<DataTypeIndex>(m_dataVectors.size());
			m_dataVectors.emplace_back(std::make_shared<std::vector<T>>());

			// Store a map of the typeID hash code to the data type index for const access
			m_typeInfoHashToDataVectorIdx.emplace(typeid(T).hash_code(), s_dataTypeIdx);

			// Guarantee there is always some backing memory allocated for our iterator .end() to reference
			std::vector<T>& dataVector =
				*std::static_pointer_cast<std::vector<T>>(m_dataVectors.back()).get();

			constexpr size_t k_expectedMaxNumDataTypes = 16; // Arbitrary
			dataVector.reserve(k_expectedMaxNumDataTypes);
		}
		return s_dataTypeIdx;
	}


	template<typename T>
	RenderDataManager::DataTypeIndex RenderDataManager::GetDataIndexFromType() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		static DataTypeIndex s_dataTypeIdx = k_invalidDataTypeIdx;
		if (s_dataTypeIdx == k_invalidDataTypeIdx)
		{
			// Cache the data type index to avoid the map lookup once a data type exists & has been found once
			auto const& result = m_typeInfoHashToDataVectorIdx.find(typeid(T).hash_code());
			if (result != m_typeInfoHashToDataVectorIdx.end())
			{
				s_dataTypeIdx = result->second;
			}
			else
			{
				return k_invalidDataTypeIdx;
			}
		}
		return s_dataTypeIdx;
	}


	template <typename T>
	T const* RenderDataManager::GetEndPtr() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex s_dataTypeIndex = GetDataIndexFromType<T>();
		if (s_dataTypeIndex != k_invalidDataTypeIdx)
		{
			std::vector<T> const& dataVector = 
				*std::static_pointer_cast<std::vector<T>>(m_dataVectors[s_dataTypeIndex]).get();

			return &dataVector.data()[dataVector.size()]; // Get the address without dereferencing it
		}
		return nullptr;
	}


	template <typename T>
	RenderDataManager::LinearIterator<T> RenderDataManager::Begin() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		T const* endPtr = GetEndPtr<T>();
		T const* beginPtr = endPtr;

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex != k_invalidDataTypeIdx)
		{
			std::vector<T>& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[dataTypeIndex]).get();
			beginPtr = &dataVector.data()[0]; // Get the address without dereferencing it
		}

		return LinearIterator<T>(beginPtr, endPtr);
	}


	template <typename T>
	RenderDataManager::LinearIterator<T> RenderDataManager::End() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		T const* endPtr = GetEndPtr<T>();
		return LinearIterator<T>(endPtr, endPtr);
	}


	template <typename... Ts>
	RenderDataManager::ObjectIterator<Ts...> RenderDataManager::ObjectBegin() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return ObjectIterator<Ts...>(
			this,
			m_IDToRenderObjectMetadata.begin(),
			m_IDToRenderObjectMetadata.end(),
			std::make_tuple(GetEndPtr<Ts>()...),
			m_currentFrame);
	}


	template <typename... Ts>
	RenderDataManager::ObjectIterator<Ts...> RenderDataManager::ObjectEnd() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator objectDataIndicesEndItr =
			m_IDToRenderObjectMetadata.end();

		return ObjectIterator<Ts...>(
			nullptr,
			objectDataIndicesEndItr,
			objectDataIndicesEndItr,
			std::make_tuple(GetEndPtr<Ts>()...),
			m_currentFrame);
	}


	inline RenderDataManager::IDIterator RenderDataManager::IDBegin(
		std::vector<gr::RenderDataID> const& renderDataIDs) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return IDIterator(
			this,
			renderDataIDs.cbegin(),
			renderDataIDs.cend(),
			&m_IDToRenderObjectMetadata,
			m_currentFrame);
	}

	inline RenderDataManager::IDIterator RenderDataManager::IDEnd(
		std::vector<gr::RenderDataID> const& renderDataIDs) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return IDIterator(
			this,
			renderDataIDs.cend(),
			renderDataIDs.cend(),
			&m_IDToRenderObjectMetadata,
			m_currentFrame);
	}


	/******************************************************************************************************************/


	template<typename T>
	RenderDataManager::LinearIterator<T>& RenderDataManager::LinearIterator<T>::operator++() // Prefix increment
	{
		m_ptr++;
		if ((m_ptr <= m_endPtr) == false)
		{
			m_ptr = m_endPtr;
		}
		return *this;
	}


	template<typename T>
	RenderDataManager::LinearIterator<T> RenderDataManager::LinearIterator<T>::operator++(int) // Postfix increment
	{
		LinearIterator current = *this;
		++(*this);
		return current;
	}


	/******************************************************************************************************************/


	template<typename... Ts>
	RenderDataManager::ObjectIterator<Ts...>::ObjectIterator(
		gr::RenderDataManager const* renderData,
		std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator objectIDToRenderObjectMetadataBeginItr,
		std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator const objectIDToRenderObjectMetadataEndItr,
		std::tuple<Ts const*...> endPtrs,
		uint64_t currentFrame)
		: m_endPtrs(endPtrs)
		, m_renderData(renderData)
		, m_renderObjectMetadataItr(objectIDToRenderObjectMetadataBeginItr)
		, m_renderObjectMetadataEndItr(objectIDToRenderObjectMetadataEndItr)
		, m_currentFrame(currentFrame)
	{
		// Find our first valid set of starting pointers:
		bool hasValidPtrs = false;
		while (m_renderObjectMetadataItr != m_renderObjectMetadataEndItr)
		{
			// We have a set of gr::RenderDataID data indices. Try and make a tuple of valid pointers from all of them
			m_ptrs = std::make_tuple(GetPtrFromCurrentObjectDataIndicesItr<Ts>()...);

			// If the current RenderDataID's object doesn't contain all data elements of the given template type (i.e.
			// any of the tuple elements are null), skip to the next object:
			bool tuplePtrsValid = true;
			std::apply([&](auto&&... ptr) { ((tuplePtrsValid = ptr ? tuplePtrsValid : false), ...); }, m_ptrs);
			if (tuplePtrsValid)
			{
				hasValidPtrs = true;
				break; // We're done!
			}

			// Try the next object
			m_renderObjectMetadataItr++;
		}

		if (!hasValidPtrs)
		{
			SEAssert(m_renderObjectMetadataItr == m_renderObjectMetadataEndItr,
				"We should have checked every element, how is this possible?");

			m_ptrs = m_endPtrs;
		}
	}


	template<typename... Ts>
	RenderDataManager::ObjectIterator<Ts...>& RenderDataManager::ObjectIterator<Ts...>::operator++() // Prefix increment
	{
		// We increment our iterator's pointers in lock-step through successive ObjectIDs until either:
		// a) We find an object that has valid pointers for each data type
		// b) One of them is at the .end() -> Invalidate all pointers, we're done walking all valid objects

		// Note: There is a potential inefficiency here. We check every single gr::RenderDataID for the set of data
		// types, but in reality this might be unnecessary (e.g. if we have many objects but one data type with only a
		// single element). I expect we'll have roughly balanced numbers of each data type and lots of cache hits so 
		// hopefully this won't be an issue...

		bool hasValidPtrs = true;
		while (m_renderObjectMetadataItr != m_renderObjectMetadataEndItr)
		{
			m_renderObjectMetadataItr++;
			if (m_renderObjectMetadataItr == m_renderObjectMetadataEndItr)
			{
				hasValidPtrs = false;
				break;
			}

			// We have a valid set of gr::RenderDataID data indices. Make a tuple from them
			m_ptrs = std::make_tuple(GetPtrFromCurrentObjectDataIndicesItr<Ts>()...);

			// If the current object doesn't have a data of the given type (i.e. any of the tuple elements rae null), we
			// want to skip over it to the next object:
			bool tuplePtrsValid = true;
			std::apply([&](auto&&... ptr) { ((tuplePtrsValid = ptr ? tuplePtrsValid : false), ...); }, m_ptrs);
			if (tuplePtrsValid)
			{
				break; // We're done!
			}
		}

		// If we find a single invalid pointer, we must invalidate them all to be equivalent to .end()
		if (!hasValidPtrs)
		{
			m_ptrs = m_endPtrs;
		}

		return *this;
	}


	template<typename... Ts>
	RenderDataManager::ObjectIterator<Ts...> RenderDataManager::ObjectIterator<Ts...>::operator++(int) // Postfix increment
	{
		ObjectIterator current = *this;
		++(*this);
		return current;
	}


	template<typename... Ts> template<typename T>
	inline T const& RenderDataManager::ObjectIterator<Ts...>::Get() const
	{
		return *std::get<T const*>(m_ptrs);
	}


	template<typename... Ts> template<typename T>
	[[nodiscard]] bool RenderDataManager::ObjectIterator<Ts...>::IsDirty() const
	{
		const DataTypeIndex dataTypeIndex = m_renderData->GetDataIndexFromType<T>();

		SEAssert(m_renderObjectMetadataItr->second.m_dirtyFrameMap.contains(dataTypeIndex) &&
			m_renderObjectMetadataItr->second.m_dirtyFrameMap.at(dataTypeIndex) <= m_currentFrame &&
			m_currentFrame != k_invalidDirtyFrameNum,
			"Invalid dirty frame value");

		return m_renderObjectMetadataItr->second.m_dirtyFrameMap.at(dataTypeIndex) == m_currentFrame;
	}

	
	template<typename... Ts>
	gr::RenderDataID RenderDataManager::ObjectIterator<Ts...>::GetRenderDataID() const
	{
		return m_renderObjectMetadataItr->first;
	}


	template <typename... Ts>
	gr::Transform::RenderData const& RenderDataManager::ObjectIterator<Ts...>::GetTransformData() const
	{
		return m_renderData->GetTransformDataFromTransformID(m_renderObjectMetadataItr->second.m_transformID);
	}


	template <typename... Ts>
	bool RenderDataManager::ObjectIterator<Ts...>::TransformIsDirty() const
	{
		return m_renderData->TransformIsDirty(m_renderObjectMetadataItr->second.m_transformID);
	}


	template <typename... Ts>
	gr::FeatureBitmask RenderDataManager::ObjectIterator<Ts...>::GetFeatureBits() const
	{
		return m_renderObjectMetadataItr->second.m_featureBits;
	}


	template <typename... Ts> template <typename T>
	T const* RenderDataManager::ObjectIterator<Ts...>::GetPtrFromCurrentObjectDataIndicesItr() const
	{
		const DataTypeIndex dataTypeIndex = m_renderData->GetDataIndexFromType<T>();

		ObjectTypeToDataIndexMap const& objectTypeToDataIndexMap = 
			m_renderObjectMetadataItr->second.m_dataTypeToDataIndexMap;

		// Make sure the data type index is found in current object's data index map
		auto objecTypeToDataIdxIter = objectTypeToDataIndexMap.find(dataTypeIndex);
		if (objecTypeToDataIdxIter != objectTypeToDataIndexMap.end())
		{
			// Get the index of the data within its typed array, for the current RenderDataID iteration
			const DataIndex objectDataIndex = objecTypeToDataIdxIter->second;

			return m_renderData->GetObjectDataVectorIfExists<T>(objectDataIndex);
		}
		return nullptr;
	}


	// ---


	inline RenderDataManager::IDIterator::IDIterator(
		gr::RenderDataManager const* renderData,
		std::vector<gr::RenderDataID>::const_iterator renderDataIDsBegin,
		std::vector<gr::RenderDataID>::const_iterator renderDataIDsEnd,
		std::unordered_map<gr::RenderDataID, RenderObjectMetadata> const* IDToRenderObjectMetadata,
		uint64_t currentFrame)
		: m_renderData(renderData)
		, m_idsIterator(renderDataIDsBegin)
		, m_idsEndIterator(renderDataIDsEnd)
		, m_IDToRenderObjectMetadata(IDToRenderObjectMetadata)
		, m_currentFrame(currentFrame)
	{
		if (m_idsIterator != m_idsEndIterator)
		{
			m_currentObjectMetadata = m_IDToRenderObjectMetadata->find(*m_idsIterator);
		}
		else
		{
			m_currentObjectMetadata = m_IDToRenderObjectMetadata->cend();
		}
	}


	template<typename T>
	bool RenderDataManager::IDIterator::HasObjectData() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");

		const DataTypeIndex dataTypeIndex = m_renderData->GetDataIndexFromType<T>();

		return m_currentObjectMetadata->second.m_dataTypeToDataIndexMap.contains(dataTypeIndex);
	}


	template<typename T>
	inline T const& RenderDataManager::IDIterator::Get() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");

		return m_renderData->GetObjectData<T>(*m_idsIterator);
	}


	template<typename T>
	bool RenderDataManager::IDIterator::IsDirty() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");

		const DataTypeIndex dataTypeIndex = m_renderData->GetDataIndexFromType<T>();

		SEAssert(m_currentObjectMetadata->second.m_dirtyFrameMap.contains(dataTypeIndex) &&
			m_currentObjectMetadata->second.m_dirtyFrameMap.at(dataTypeIndex) <= m_currentFrame &&
			m_currentFrame != k_invalidDirtyFrameNum,
			"Invalid dirty frame value");

		return m_currentObjectMetadata->second.m_dirtyFrameMap.at(dataTypeIndex) == m_currentFrame;
	}


	inline gr::RenderDataID RenderDataManager::IDIterator::GetRenderDataID() const
	{
		SEAssert(m_idsIterator != m_idsEndIterator, "Invalid Get: Current m_idsIterator is past-the-end");
		return *m_idsIterator;
	}


	inline gr::TransformID RenderDataManager::IDIterator::GetTransformID() const
	{
		SEAssert(m_idsIterator != m_idsEndIterator, "Invalid Get: Current m_idsIterator is past-the-end");
		return m_currentObjectMetadata->second.m_transformID;
	}


	inline gr::Transform::RenderData const& RenderDataManager::IDIterator::GetTransformData() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");
		return m_renderData->GetTransformDataFromTransformID(m_currentObjectMetadata->second.m_transformID);
	}


	inline bool RenderDataManager::IDIterator::TransformIsDirty() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");

		return m_renderData->TransformIsDirty(m_currentObjectMetadata->second.m_transformID);
	}


	inline gr::FeatureBitmask RenderDataManager::IDIterator::GetFeatureBits() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");

		return m_currentObjectMetadata->second.m_featureBits;
	}


	inline RenderDataManager::IDIterator& RenderDataManager::IDIterator::operator++() // Prefix increment
	{
		++m_idsIterator;
		if (m_idsIterator != m_idsEndIterator)
		{
			// As a small optimization, we cache the current RenderObjectMetadata iterator to save repeated queries
			m_currentObjectMetadata = m_IDToRenderObjectMetadata->find(*m_idsIterator);
		}
		else
		{
			m_currentObjectMetadata = m_IDToRenderObjectMetadata->cend();
		}
		return *this;
	}


	inline RenderDataManager::IDIterator RenderDataManager::IDIterator::operator++(int) // Postfix increment
	{
		IDIterator current = *this;
		++(*this);
		return current;
	}
}

