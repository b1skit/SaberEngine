// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"
#include "TransformRenderData.h"

#include "Core/Assert.h"
#include "Core/Util/CastUtils.h"
#include "Core/Util/ThreadProtector.h"


namespace gr
{
	class IndexedBufferManager;


	// Render-thread-side scene data. Data is set via the render command queue (on a single thread), and graphics
	// systems index use constant forward iterators to access it
	class RenderDataManager
	{
	public:
		RenderDataManager();

		~RenderDataManager();
		RenderDataManager(RenderDataManager const&) = default;
		RenderDataManager(RenderDataManager&&) noexcept = default;
		RenderDataManager& operator=(RenderDataManager const&) = default;
		RenderDataManager& operator=(RenderDataManager&&) noexcept = default;


	public:
		void Initialize();
		void Destroy();

		void BeginFrame(uint64_t currentFrame);
		void Update();

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
		
		template<typename T, typename Next, typename... Rest>
		[[nodiscard]] bool HasObjectData(gr::RenderDataID) const;

		template<typename T>
		[[nodiscard]] bool HasObjectData() const; // Does data of the given type exist for any ID

		template<typename T, typename Next, typename... Rest>
		[[nodiscard]] bool HasObjectData() const;

		template<typename T>
		[[nodiscard]] bool HasIDsWithNewData() const;

		// Get a list of IDs that had data of a specific type added for the very first time this frame
		template<typename T>
		[[nodiscard]] std::vector<gr::RenderDataID> const* GetIDsWithNewData() const;

		template<typename T>
		[[nodiscard]] bool HasIDsWithDeletedData() const;

		template<typename T>
		std::vector<gr::RenderDataID> const* GetIDsWithDeletedData() const;

		std::vector<gr::RenderDataID> const& GetIDsWithAnyDeletedData() const;

		// Get a list of IDs that had data of a specific type modified (i.e. SetObjectData() was called) this frame
		template<typename T>
		std::vector<gr::RenderDataID> const* GetIDsWithDirtyData() const;

		[[nodiscard]] bool HasAnyDirtyData() const; // Has any dirty data at all, regardless of type

		template<typename... Ts>
		[[nodiscard]] bool HasAnyDirtyData() const;

		// Get a unique list of IDs that have all Ts, where any/all of the Ts have dirty data for this frame
		template<typename... Ts>
		[[nodiscard]] std::vector<gr::RenderDataID> GetIDsWithAnyDirtyData(gr::FeatureBitmask = RenderObjectFeature::None) const;

		template<typename T>
		[[nodiscard]] bool IsDirty(gr::RenderDataID) const;

		template<typename T>
		void DestroyObjectData(gr::RenderDataID);

		template<typename T>
		[[nodiscard]] uint32_t GetNumElementsOfType() const;

		template<typename T>
		[[nodiscard]] uint32_t GetNumElementsOfType(gr::RenderObjectFeature) const;

		void SetFeatureBits(gr::RenderDataID, gr::FeatureBitmask); // Logical OR
		
		[[nodiscard]] gr::FeatureBitmask GetFeatureBits(gr::RenderDataID) const;

		// Get IDs associated with a type
		template<typename T>
		[[nodiscard]] std::vector<gr::RenderDataID> const* GetRegisteredRenderDataIDs() const;

		template<typename T>
		[[nodiscard]] std::span<const gr::RenderDataID> GetRegisteredRenderDataIDsSpan() const; // As a span

		// Get all RenderDataIDs (regardless of associated data types)
		[[nodiscard]] std::vector<gr::RenderDataID> const& GetRegisteredRenderDataIDs() const; 

		[[nodiscard]] std::vector<gr::TransformID> const& GetRegisteredTransformIDs() const;


	private: // Variadic helpers:
		template<typename T>
		[[nodiscard]] bool HasAnyDirtyDataInternal() const;

		template<typename T, typename Next, typename... Rest>
		[[nodiscard]] bool HasAnyDirtyDataInternal() const;

		template<typename T>
		[[nodiscard]] void GetIDsWithAnyDirtyDataInternal(std::vector<gr::RenderDataID>&) const;

		template<typename T, typename Next, typename... Rest>
		[[nodiscard]] void GetIDsWithAnyDirtyDataInternal(std::vector<gr::RenderDataID>&) const;

		template<typename T>
		[[nodiscard]] size_t GetNumberOfDirtyIDs() const;

		template<typename T, typename Next, typename... Rest>
		[[nodiscard]] size_t GetNumberOfDirtyIDs() const;


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

		[[nodiscard]] std::vector<gr::TransformID> const& GetNewTransformIDs() const;
		[[nodiscard]] std::vector<gr::TransformID> const& GetDeletedTransformIDs() const;

		[[nodiscard]] gr::TransformID GetTransformIDFromRenderDataID(gr::RenderDataID) const;
		[[nodiscard]] std::vector<gr::RenderDataID> GetRenderDataIDsReferencingTransformID(gr::TransformID) const;

		[[nodiscard]] uint32_t GetNumTransforms() const;


	public:
		gr::IndexedBufferManager& GetInstancingIndexedBufferManager() const;


	public:
		void ShowImGuiWindow() const;
	private:
		template<typename T>
		void PopulateTypesImGuiHelper(std::vector<std::string>&, char const*) const;


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

		std::vector<gr::RenderDataID> m_perFrameDeletedDataIDs; // IDs with ANY deleted data, regardless of type
		std::unordered_set<gr::RenderDataID> m_perFrameSeenDeletedDataIDs; 

		// IDs that had data of a given type modified in the current frame. We track the IDs we've modified so we don't 
		// double-add IDs to the vector
		std::vector<std::vector<gr::RenderDataID>> m_perFramePerTypeDirtyDataIDs;
		std::vector<std::unordered_set<gr::RenderDataID>> m_perFramePerTypeDirtySeenDataIDs; 
		

		// Transforms:
		std::vector<gr::TransformID> m_perFrameNewTransformIDs;
		std::vector<gr::TransformID> m_perFrameDeletedTransformIDs;

		std::vector<gr::TransformID> m_perFrameDirtyTransformIDs;
		std::unordered_set<gr::TransformID> m_perFrameSeenDirtyTransformIDs;

		// Multiple RenderDataIDs can share the same TransformID
		std::unordered_multimap<gr::TransformID, gr::RenderDataID> m_transformToRenderDataIDs;


	private:
		std::unique_ptr<gr::IndexedBufferManager> m_indexedBufferManager;


	public:
		template <typename T>
		friend class LinearAdapter;

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
			~LinearIterator() = default;
			LinearIterator(LinearIterator const&) = default;
			LinearIterator(LinearIterator&&) noexcept = default;
			LinearIterator& operator=(LinearIterator const&) = default;
			LinearIterator& operator=(LinearIterator&&) noexcept = default;

		public:
			template<typename Other>
			[[nodiscard]] bool HasObjectData() const { return std::is_same_v<T, Other>; }

			template<typename GetType>
			[[nodiscard]] T const& Get() const
			{
				SEStaticAssert((std::is_same_v<T, GetType> == true), "Invalid type for a linear iterator");
				return *m_ptr;
			}

			LinearIterator& operator++(); // Prefix increment
			LinearIterator operator++(int); // Postfix increment

			LinearIterator* operator*() { return this; }
			LinearIterator const* operator*() const { return this; }

			// These are declared as friends so they're not classified as member functions
			friend bool operator==(LinearIterator const& lhs, LinearIterator const& rhs) { return lhs.m_ptr == rhs.m_ptr; }
			friend bool operator!=(LinearIterator const& lhs, LinearIterator const& rhs) { return lhs.m_ptr != rhs.m_ptr; }


		private:
			T const* m_ptr;
			T const* m_endPtr;
		};


	private:
		template<typename... Ts>
		friend class ObjectAdapter;

		// Iterate over multiple data types, with each iteration's elements associated by gr::RenderDataID.
		// This is slower than a LinearIterator, but elements of different data types are guaranteed to be associated
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
				uint64_t currentFrame,
				FeatureBitmask featureMask);

		public:
			~ObjectIterator() = default;
			ObjectIterator(ObjectIterator const&) = default;
			ObjectIterator(ObjectIterator&&) noexcept = default;
			ObjectIterator& operator=(ObjectIterator const&) = default;
			ObjectIterator& operator=(ObjectIterator&&) noexcept = default;

		public:
			template<typename T>
			[[nodiscard]] bool HasObjectData() const;

			template<typename T>
			[[nodiscard]] T const& Get() const;

			template<typename T>
			[[nodiscard]] bool IsDirty() const;

			bool AnyDirty() const;

			[[nodiscard]] gr::RenderDataID GetRenderDataID() const;
			[[nodiscard]] gr::TransformID GetTransformID() const;

			[[nodiscard]] gr::Transform::RenderData const& GetTransformData() const;
			[[nodiscard]] bool TransformIsDirty() const;

			[[nodiscard]] gr::FeatureBitmask GetFeatureBits() const;
			[[nodiscard]] bool HasAllFeatures() const;

			ObjectIterator& operator++(); // Prefix increment
			ObjectIterator operator++(int); // Postfix increment

			ObjectIterator* operator*() { return this; }
			ObjectIterator const* operator*() const { return this; }

			gr::RenderDataManager const* GetRenderDataManager() const { return m_renderData; }

			// These are declared as friends so they're not classified as member functions
			friend bool operator==(ObjectIterator const& lhs, ObjectIterator const& rhs) { return lhs.m_ptrs == rhs.m_ptrs; }
			friend bool operator!=(ObjectIterator const& lhs, ObjectIterator const& rhs) { return lhs.m_ptrs != rhs.m_ptrs; }


		private:
			template <typename T>
			T const* GetPtrFromCurrentObjectDataIndicesItr() const;

			template<typename T, typename Next, typename... Rest>
			bool AnyDirtyHelper() const;

			template<typename T>
			bool AnyDirtyHelper() const;


		private:
			std::tuple<Ts const*...> m_ptrs;
			std::tuple<Ts const*...> m_endPtrs;

			gr::RenderDataManager const* m_renderData;
			std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator m_renderObjectMetadataItr;
			std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator const m_renderObjectMetadataEndItr;

			uint64_t m_currentFrame;
			FeatureBitmask m_featureMask;
		};


	public:
		template<typename Container>
		friend class IDAdapter;

		// Iterate over objects via std containers of RenderDataIDs. This is largely a convenience iterator; it functions 
		// similarly to calling RenderDataManager::GetObjectData with each RenderDataID in the supplied container, except
		// the results of the RenderDataID -> RenderObjectMetadata lookup are cached when the iterator is incremented.
		// RenderDataManager iterators are not thread safe.
		template<typename Container>
		class IDIterator
		{			
		protected:
			friend class gr::RenderDataManager;
			IDIterator(
				gr::RenderDataManager const*, 
				Container::const_iterator,
				Container::const_iterator,
				std::unordered_map<gr::RenderDataID, RenderObjectMetadata> const*,
				uint64_t currentFrame,
				RenderObjectFeature featureMask);

		public:
			~IDIterator() = default;
			IDIterator(IDIterator const&) = default;
			IDIterator(IDIterator&&) noexcept = default;
			IDIterator& operator=(IDIterator const&) = default;
			IDIterator& operator=(IDIterator&&) noexcept = default;

		public:
			template<typename T>
			[[nodiscard]] bool HasObjectData() const;

			template<typename T>
			[[nodiscard]] T const& Get() const;

			template<typename T>
			[[nodiscard]] bool IsDirty() const;

			[[nodiscard]] gr::RenderDataID GetRenderDataID() const;
			[[nodiscard]] gr::TransformID GetTransformID() const;

			[[nodiscard]] gr::Transform::RenderData const& GetTransformData() const;
			[[nodiscard]] bool TransformIsDirty() const;

			[[nodiscard]] gr::FeatureBitmask GetFeatureBits() const;

			IDIterator& operator++(); // Prefix increment
			IDIterator operator++(int); // Postfix increment

			IDIterator* operator*() { return this; }
			IDIterator const* operator*() const { return this; }

			gr::RenderDataManager const* GetRenderDataManager() const { return m_renderData; }

			// These are declared as friends so they're not classified as member functions
			friend bool operator==(IDIterator const& lhs, IDIterator const& rhs) { return lhs.m_idsIterator == rhs.m_idsIterator; }
			friend bool operator!=(IDIterator const& lhs, IDIterator const& rhs) { return lhs.m_idsIterator != rhs.m_idsIterator; }


		private:
			gr::RenderDataManager const* m_renderData;
			Container::const_iterator m_idsIterator;
			Container::const_iterator const m_idsEndIterator;

			std::unordered_map<gr::RenderDataID, RenderObjectMetadata> const* m_IDToRenderObjectMetadata;
			std::unordered_map<gr::RenderDataID, RenderObjectMetadata>::const_iterator m_currentObjectMetadata;


			uint64_t m_currentFrame;
			RenderObjectFeature m_featureMask;
		};


	private:
		template <typename T>
		LinearIterator<T> LinearBegin() const;

		template <typename T>
		LinearIterator<T> LinearEnd() const;

		template <typename... Ts>
		ObjectIterator<Ts...> ObjectBegin(RenderObjectFeature = RenderObjectFeature::None) const;

		template <typename... Ts>
		ObjectIterator<Ts...> ObjectEnd() const;

		template <typename Container>
		IDIterator<Container> IDBegin(Container const&, RenderObjectFeature = RenderObjectFeature::None) const;

		template <typename Container>
		IDIterator<Container> IDEnd(Container const&) const;


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
	class LinearAdapter
	{
	public:
		LinearAdapter(gr::RenderDataManager const& renderData) : m_renderData(renderData) {}

		RenderDataManager::LinearIterator<T> begin() const
		{
			return m_renderData.LinearBegin<T>();
		}
		RenderDataManager::LinearIterator<T> end() const
		{
			return m_renderData.LinearEnd<T>();
		}

		RenderDataManager::LinearIterator<T> cbegin() const
		{
			return m_renderData.LinearBegin<T>();
		}
		RenderDataManager::LinearIterator<T> cend() const
		{
			return m_renderData.LinearEnd<T>();
		}

	private:
		gr::RenderDataManager const& m_renderData;
	};


	template<typename... Ts>
	class ObjectAdapter
	{
	public:
		ObjectAdapter(gr::RenderDataManager const& renderData, RenderObjectFeature featureMask = RenderObjectFeature::None)
			: m_renderData(renderData)
			, m_featureMask(featureMask)
		{}

		RenderDataManager::ObjectIterator<Ts...> begin() const
		{
			return m_renderData.ObjectBegin<Ts...>(m_featureMask);
		}
		RenderDataManager::ObjectIterator<Ts...> end() const
		{
			return m_renderData.ObjectEnd<Ts...>();
		}

		RenderDataManager::ObjectIterator<Ts...> cbegin() const
		{
			return m_renderData.ObjectBegin<Ts...>(m_featureMask);
		}
		RenderDataManager::ObjectIterator<Ts...> cend() const
		{
			return m_renderData.ObjectEnd<Ts...>();
		}
		

	private:
		gr::RenderDataManager const& m_renderData;
		RenderObjectFeature m_featureMask;
	};


	template<typename Container>
	class IDAdapter
	{
	public:
		IDAdapter(
			gr::RenderDataManager const& renderData,
			Container const& renderDataIDs, 
			RenderObjectFeature featureMask = RenderObjectFeature::None)
			: m_renderData(renderData)
			, m_renderDataIDs(renderDataIDs)
			, m_featureMask(featureMask)
		{
		}

		RenderDataManager::IDIterator<Container> begin() const
		{
			return m_renderData.IDBegin<Container>(m_renderDataIDs, m_featureMask);
		}
		RenderDataManager::IDIterator<Container> end() const
		{
			return m_renderData.IDEnd<Container>(m_renderDataIDs);
		}

		RenderDataManager::IDIterator<Container> cbegin() const
		{
			return m_renderData.IDBegin<Container>(m_renderDataIDs, m_featureMask);
		}
		RenderDataManager::IDIterator<Container> cend() const
		{
			return m_renderData.IDEnd<Container>(m_renderDataIDs);
		}


	private:
		gr::RenderDataManager const& m_renderData;
		Container const& m_renderDataIDs;
		RenderObjectFeature m_featureMask;
	};


	// ---


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
		SEStaticAssert((std::is_same_v<T, gr::Transform::RenderData> == false),
			"This function does not (currently) support gr::Transform::RenderData queries");

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
		if constexpr (std::is_same_v<T, gr::Transform::RenderData>)
		{
			return true; // All RenderDataIDs are associated with a gr::Transform::RenderData
		}

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


	template<typename T, typename Next, typename... Rest>
	bool RenderDataManager::HasObjectData(gr::RenderDataID renderDataID) const
	{
		return HasObjectData<T>(renderDataID) && HasObjectData<Next, Rest...>(renderDataID);
	}


	template<typename T>
	bool RenderDataManager::HasObjectData() const
	{
		if constexpr (std::is_same_v<T, gr::Transform::RenderData>)
		{
			return !m_transformIDToTransformMetadata.empty();
		}

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();

		return dataTypeIndex != k_invalidDataTypeIdx;
	}


	template<typename T, typename Next, typename... Rest>
	bool RenderDataManager::HasObjectData() const
	{
		return HasObjectData<T>() && HasObjectData<Next, Rest...>();
	}


	template<typename T>
	bool RenderDataManager::HasIDsWithNewData() const
	{
		SEStaticAssert((std::is_same_v<T, gr::Transform::RenderData> == false),
			"This function does not (currently) support gr::Transform::RenderData queries");

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();

		return dataTypeIndex < m_perFramePerTypeNewDataIDs.size() && 
			!m_perFramePerTypeNewDataIDs[dataTypeIndex].empty();
	}


	template<typename T>
	std::vector<gr::RenderDataID> const* RenderDataManager::GetIDsWithNewData() const
	{
		SEStaticAssert((std::is_same_v<T, gr::Transform::RenderData> == false),
			"This function does not (currently) support gr::Transform::RenderData queries");

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

		if constexpr (std::is_same_v<T, gr::Transform::RenderData>)
		{
			return m_perFrameDeletedTransformIDs.empty() == false;
		}

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();

		return dataTypeIndex < m_perFramePerTypeDeletedDataIDs.size() &&
			!m_perFramePerTypeDeletedDataIDs[dataTypeIndex].empty();
	}


	template<typename T>
	std::vector<gr::RenderDataID> const* RenderDataManager::GetIDsWithDeletedData() const
	{
		SEStaticAssert((std::is_same_v<T, gr::Transform::RenderData> == false),
			"This function does not (currently) support gr::Transform::RenderData queries");

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex != k_invalidDataTypeIdx)
		{
			SEAssert(dataTypeIndex < m_perFramePerTypeDeletedDataIDs.size(), "Data type index is OOB");
			return &m_perFramePerTypeDeletedDataIDs[dataTypeIndex];
		}
		return nullptr;			
	}


	inline std::vector<gr::RenderDataID> const& RenderDataManager::GetIDsWithAnyDeletedData() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return m_perFrameDeletedDataIDs;
	}


	template<typename T>
	std::vector<gr::RenderDataID> const* RenderDataManager::GetIDsWithDirtyData() const
	{
		SEStaticAssert((std::is_same_v<T, gr::Transform::RenderData> == false),
			"This function does not (currently) support gr::Transform::RenderData queries");

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex != k_invalidDataTypeIdx)
		{
			SEAssert(dataTypeIndex < m_perFramePerTypeDirtyDataIDs.size(), "Data type index is OOB");
			return &m_perFramePerTypeDirtyDataIDs[dataTypeIndex];
		}
		return nullptr;
	}


	template<>
	inline std::vector<gr::TransformID> const* RenderDataManager::GetIDsWithDirtyData<gr::Transform::RenderData>() const
	{
		return &GetIDsWithDirtyTransformData();
	}


	inline bool RenderDataManager::HasAnyDirtyData() const
	{
		bool hasDirtyData = m_perFrameDirtyTransformIDs.empty() == false;
		if (!hasDirtyData)
		{
			for (auto const& dirtyIDs : m_perFramePerTypeDirtyDataIDs)
			{
				hasDirtyData |= dirtyIDs.empty() == false;

				if (hasDirtyData)
				{
					break; // Early out
				}
			}
		}
		return hasDirtyData;
	}


	template<typename... Ts>
	[[nodiscard]] bool RenderDataManager::HasAnyDirtyData() const
	{
		return HasAnyDirtyDataInternal<Ts...>();
	}


	template<typename T>
	[[nodiscard]] bool RenderDataManager::HasAnyDirtyDataInternal() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		if constexpr (std::is_same_v<T, gr::Transform::RenderData>)
		{
			return m_perFrameDirtyTransformIDs.empty() == false;
		}
		else
		{
			std::vector<gr::RenderDataID> const* dirtyIDs = GetIDsWithDirtyData<T>();
			return dirtyIDs != nullptr && dirtyIDs->empty() == false;
		}
	}


	template<typename T, typename Next, typename... Rest>
	[[nodiscard]] bool RenderDataManager::HasAnyDirtyDataInternal() const
	{
		return HasAnyDirtyDataInternal<T>() && HasAnyDirtyDataInternal<Next, Rest...>();
	}


	template<typename T>
	void RenderDataManager::GetIDsWithAnyDirtyDataInternal(std::vector<gr::RenderDataID>& dirtyIDs) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		if constexpr (std::is_same_v<T, gr::Transform::RenderData>)
		{
			std::vector<gr::TransformID> const& dirtyTransformIDs = GetIDsWithDirtyTransformData();

			for (gr::TransformID transformID : dirtyTransformIDs)
			{
				auto renderDataIDs = m_transformToRenderDataIDs.equal_range(transformID);
				while (renderDataIDs.first != renderDataIDs.second)
				{
					dirtyIDs.emplace_back(renderDataIDs.first->second);
					++renderDataIDs.first;
				}
			}
		}
		else
		{
			std::vector<gr::RenderDataID> const* dirtyTs = GetIDsWithDirtyData<T>();
			if (dirtyTs)
			{
				dirtyIDs.insert(dirtyIDs.end(), dirtyTs->begin(), dirtyTs->end());
			}
		}
	}


	template<typename T, typename Next, typename... Rest>
	void RenderDataManager::GetIDsWithAnyDirtyDataInternal(std::vector<gr::RenderDataID>& uniqueDirtyIDs) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		GetIDsWithAnyDirtyDataInternal<T>(uniqueDirtyIDs);
		GetIDsWithAnyDirtyDataInternal<Next, Rest...>(uniqueDirtyIDs);
	}


	template<typename... Ts>
	std::vector<gr::RenderDataID> RenderDataManager::GetIDsWithAnyDirtyData(
		gr::FeatureBitmask featureBits /*= RenderObjectFeature::None*/) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const size_t numDirtyIDs = GetNumberOfDirtyIDs<Ts...>(); // Likely an over-estimation
		if (numDirtyIDs == 0)
		{
			return {}; // Early out
		}

		// Concatenate a list of all dirty RenderDataIDs for each type:
		std::vector<gr::RenderDataID> dirtyIDs;
		dirtyIDs.reserve(numDirtyIDs);
		
		GetIDsWithAnyDirtyDataInternal<Ts...>(dirtyIDs);
		SEAssert(dirtyIDs.size() <= numDirtyIDs, "Found more dirty IDs than anticipated. This should not be possible");

		// Post-process the RenderDataIDs in-place to remove duplicates or IDs that don't own ALL of the required types
		std::unordered_set<gr::RenderDataID> seenIDs;
		seenIDs.reserve(dirtyIDs.size());

		auto idItr = dirtyIDs.begin();
		while (idItr != dirtyIDs.end())
		{
			const gr::RenderDataID curID = *idItr;

			// We return a list of unique IDs, that contains all of the types
			// If we've seen this ID, or it doesn't have all the types/feature bits, remove it
			if (seenIDs.contains(curID) || 
				!HasObjectData<Ts...>(curID) ||
				(featureBits != RenderObjectFeature::None && !HasAllFeatures(featureBits, GetFeatureBits(curID))))
			{
				bool isLast = false;
				auto lastElementItr = std::prev(dirtyIDs.end());
				if (lastElementItr != idItr)
				{
					*idItr = *lastElementItr;
				}
				else
				{
					isLast = true;
				}
				dirtyIDs.pop_back();

				seenIDs.emplace(curID);

				if (dirtyIDs.empty() || isLast)
				{
					break;
				}
			}
			else
			{
				seenIDs.emplace(curID);
				++idItr;
			}
		}

		return dirtyIDs;
	}


	template<typename T>
	size_t RenderDataManager::GetNumberOfDirtyIDs() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		if constexpr (std::is_same_v<T, gr::Transform::RenderData>)
		{
			size_t count = 0;
			std::vector<gr::TransformID> const& dirtyTransformIDs = GetIDsWithDirtyTransformData();
			for (gr::TransformID transformID : dirtyTransformIDs)
			{
				auto renderDataIDsItr = m_transformToRenderDataIDs.equal_range(transformID);
				count += std::distance(renderDataIDsItr.first, renderDataIDsItr.second);
			}
			return count;
		}
		else
		{
			std::vector<gr::RenderDataID> const* dirtyIDs = GetIDsWithDirtyData<T>();
			return dirtyIDs ? dirtyIDs->size() : 0;
		}
	}


	template<typename T, typename Next, typename... Rest>
	size_t RenderDataManager::GetNumberOfDirtyIDs() const
	{
		return GetNumberOfDirtyIDs<T>() + GetNumberOfDirtyIDs<Next, Rest...>();
	}


	template<typename T>
	bool RenderDataManager::IsDirty(gr::RenderDataID renderDataID) const
	{
		SEStaticAssert((std::is_same_v<T, gr::Transform::RenderData> == false),
			"This function does not (currently) support gr::Transform::RenderData queries");

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
		if constexpr (std::is_same_v<T, gr::Transform::RenderData>)
		{
			return GetNumTransforms();
		}

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
	[[nodiscard]] uint32_t RenderDataManager::GetNumElementsOfType(gr::RenderObjectFeature featureBits) const
	{
		if constexpr (std::is_same_v<T, gr::Transform::RenderData>)
		{
			SEAssert(featureBits == gr::RenderObjectFeature::None, "Feature bits are not valid for Transforms");
			return GetNumTransforms();
		}

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		// Avoid explicit counting if we can:
		if (featureBits == RenderObjectFeature::None)
		{
			return GetNumElementsOfType<T>();
		}

		// Manually count each element that matches the feature bits:
		uint32_t count = 0;

		ObjectAdapter<T> objAdapter(*this, featureBits);
		for (auto const& itr : objAdapter)
		{
			if (itr->HasAllFeatures())
			{
				++count;
			}
		}
		return count;
	}


	[[nodiscard]] inline gr::TransformID RenderDataManager::GetTransformIDFromRenderDataID(
		gr::RenderDataID renderDataID) const
	{
		// Note: This function is slower than direct access via the TransformID. If you have a TransformID, use it

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "Trying to find an object that does not exist");

		RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);

		return renderObjectMetadata.m_transformID;
	}


	[[nodiscard]] inline std::vector<gr::RenderDataID> RenderDataManager::GetRenderDataIDsReferencingTransformID(
		gr::TransformID transformID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		std::vector<gr::TransformID> result;

		auto resultsItr = m_transformToRenderDataIDs.equal_range(transformID);

		result.reserve(std::distance(resultsItr.first, resultsItr.second));

		for (auto& itr = resultsItr.first; itr != resultsItr.second; ++itr)
		{
			result.emplace_back(itr->second);
		}
		return result;
	}


	[[nodiscard]] inline uint32_t RenderDataManager::GetNumTransforms() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return util::CheckedCast<uint32_t>(m_registeredTransformIDs.size());
	}


	template<typename T>
	std::vector<gr::RenderDataID> const* RenderDataManager::GetRegisteredRenderDataIDs() const
	{
		SEStaticAssert((std::is_same_v<T, gr::Transform::RenderData> == false), "Invalid type for this function");

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex == k_invalidDataTypeIdx)
		{
			return nullptr;
		}
		return &m_perTypeRegisteredRenderDataIDs[dataTypeIndex];
	}


	template<typename T>
	std::span<const gr::RenderDataID> RenderDataManager::GetRegisteredRenderDataIDsSpan() const
	{
		SEStaticAssert((std::is_same_v<T, gr::Transform::RenderData> == false), "Invalid type for this function");

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex == k_invalidDataTypeIdx)
		{
			return std::span<gr::RenderDataID>{};
		}
		return std::span{m_perTypeRegisteredRenderDataIDs[dataTypeIndex]};
	}


	inline std::vector<gr::RenderDataID> const& RenderDataManager::GetRegisteredRenderDataIDs() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return m_registeredRenderObjectIDs;
	}


	inline std::vector<gr::TransformID> const& RenderDataManager::GetRegisteredTransformIDs() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return m_registeredTransformIDs;
	}


	template<typename T>
	void RenderDataManager::DestroyObjectData(gr::RenderDataID renderDataID)
	{
		SEStaticAssert((std::is_same_v<T, gr::Transform::RenderData> == false), "Invalid type for this function");

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

		// Add the RenderDataID to the deleted data trackers:
		m_perFramePerTypeDeletedDataIDs[dataTypeIndex].emplace_back(renderDataID);

		if (!m_perFrameSeenDeletedDataIDs.contains(renderDataID))
		{
			m_perFrameDeletedDataIDs.emplace_back(renderDataID);
			m_perFrameSeenDeletedDataIDs.emplace(renderDataID);
		}

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
	RenderDataManager::LinearIterator<T> RenderDataManager::LinearBegin() const
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
	RenderDataManager::LinearIterator<T> RenderDataManager::LinearEnd() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		T const* endPtr = GetEndPtr<T>();
		return LinearIterator<T>(endPtr, endPtr);
	}


	template <typename... Ts>
	RenderDataManager::ObjectIterator<Ts...> RenderDataManager::ObjectBegin(
		RenderObjectFeature featureMask/*= RenderObjectFeature::None*/) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return ObjectIterator<Ts...>(
			this,
			m_IDToRenderObjectMetadata.begin(),
			m_IDToRenderObjectMetadata.end(),
			std::make_tuple(GetEndPtr<Ts>()...),
			m_currentFrame,
			featureMask);
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
			m_currentFrame,
			RenderObjectFeature::None);
	}


	template <typename Container>
	RenderDataManager::IDIterator<Container> RenderDataManager::IDBegin(
		Container const& renderDataIDs, RenderObjectFeature featureMask /*= RenderObjectFeature::None*/) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return IDIterator<Container>(
			this,
			renderDataIDs.cbegin(),
			renderDataIDs.cend(),
			&m_IDToRenderObjectMetadata,
			m_currentFrame,
			featureMask);
	}


	template <typename Container>
	RenderDataManager::IDIterator<Container> RenderDataManager::IDEnd(Container const& renderDataIDs) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return IDIterator<Container>(
			this,
			renderDataIDs.cend(),
			renderDataIDs.cend(),
			&m_IDToRenderObjectMetadata,
			m_currentFrame,
			RenderObjectFeature::None);
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
		uint64_t currentFrame,
		FeatureBitmask featureMask)
		: m_endPtrs(endPtrs)
		, m_renderData(renderData)
		, m_renderObjectMetadataItr(objectIDToRenderObjectMetadataBeginItr)
		, m_renderObjectMetadataEndItr(objectIDToRenderObjectMetadataEndItr)
		, m_currentFrame(currentFrame)
		, m_featureMask(featureMask)
	{
		// Find our first valid set of starting pointers:
		bool hasValidPtrs = false;
		while (m_renderObjectMetadataItr != m_renderObjectMetadataEndItr)
		{
			// We have a set of gr::RenderDataID data indices. Try and make a tuple of valid pointers from all of them
			m_ptrs = std::make_tuple(GetPtrFromCurrentObjectDataIndicesItr<Ts>()...);
			
			// Check the feature mask:
			if (gr::HasAllFeatures(m_featureMask, m_renderObjectMetadataItr->second.m_featureBits) == false)
			{
				m_renderObjectMetadataItr++;
				continue;
			}

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

			// Check the feature mask:
			if (gr::HasAllFeatures(m_featureMask, m_renderObjectMetadataItr->second.m_featureBits) == false)
			{
				continue;
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
	[[nodiscard]] bool RenderDataManager::ObjectIterator<Ts...>::HasObjectData() const
	{
		return std::disjunction_v<std::is_same<T, Ts>...>;
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
	bool RenderDataManager::ObjectIterator<Ts...>::AnyDirty() const
	{
		return TransformIsDirty() || AnyDirtyHelper<Ts...>();
	}


	template<typename... Ts>
	template<typename T, typename Next, typename... Rest>
	bool RenderDataManager::ObjectIterator<Ts...>::AnyDirtyHelper() const
	{
		return IsDirty<T>() || AnyDirtyHelper<Next, Rest...>();
	}

	template<typename... Ts>
	template<typename T>
	bool RenderDataManager::ObjectIterator<Ts...>::AnyDirtyHelper() const
	{
		return IsDirty<T>();
	}

	
	template<typename... Ts>
	gr::RenderDataID RenderDataManager::ObjectIterator<Ts...>::GetRenderDataID() const
	{
		return m_renderObjectMetadataItr->first;
	}


	template<typename... Ts>
	gr::RenderDataID RenderDataManager::ObjectIterator<Ts...>::GetTransformID() const
	{
		return m_renderObjectMetadataItr->second.m_transformID;
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


	template <typename... Ts>
	inline bool RenderDataManager::ObjectIterator<Ts...>::HasAllFeatures() const
	{
		return m_featureMask == RenderObjectFeature::None ||
			(m_featureMask & GetFeatureBits()) == m_featureMask;
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


	template<typename Container>
	inline RenderDataManager::IDIterator<Container>::IDIterator(
		gr::RenderDataManager const* renderData,
		Container::const_iterator renderDataIDsBegin,
		Container::const_iterator renderDataIDsEnd,
		std::unordered_map<gr::RenderDataID, RenderObjectMetadata> const* IDToRenderObjectMetadata,
		uint64_t currentFrame,
		RenderObjectFeature featureMask)
		: m_renderData(renderData)
		, m_idsIterator(renderDataIDsBegin)
		, m_idsEndIterator(renderDataIDsEnd)
		, m_IDToRenderObjectMetadata(IDToRenderObjectMetadata)
		, m_currentFrame(currentFrame)
		, m_featureMask(featureMask)
	{
		while (m_idsIterator != m_idsEndIterator &&
			gr::HasAllFeatures(m_featureMask, 
				m_IDToRenderObjectMetadata->find(*m_idsIterator)->second.m_featureBits) == false)
		{
			++m_idsIterator;
		}

		if (m_idsIterator != m_idsEndIterator)
		{
			m_currentObjectMetadata = m_IDToRenderObjectMetadata->find(*m_idsIterator);

			SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->end(),
				"Failed to find a metadata entry for the current ID."); // We can't iterate over deleted IDs
		}
		else
		{
			m_currentObjectMetadata = m_IDToRenderObjectMetadata->cend();
		}
	}


	template<typename Container>
	template<typename T>
	bool RenderDataManager::IDIterator<Container>::HasObjectData() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");

		const DataTypeIndex dataTypeIndex = m_renderData->GetDataIndexFromType<T>();

		return m_currentObjectMetadata->second.m_dataTypeToDataIndexMap.contains(dataTypeIndex);
	}


	template<typename Container>
	template<typename T>
	inline T const& RenderDataManager::IDIterator<Container>::Get() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");

		return m_renderData->GetObjectData<T>(*m_idsIterator);
	}


	template<typename Container>
	template<typename T>
	bool RenderDataManager::IDIterator<Container>::IsDirty() const
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


	template<typename Container>
	inline gr::RenderDataID RenderDataManager::IDIterator<Container>::GetRenderDataID() const
	{
		SEAssert(m_idsIterator != m_idsEndIterator, "Invalid Get: Current m_idsIterator is past-the-end");
		return *m_idsIterator;
	}


	template<typename Container>
	inline gr::TransformID RenderDataManager::IDIterator<Container>::GetTransformID() const
	{
		SEAssert(m_idsIterator != m_idsEndIterator, "Invalid Get: Current m_idsIterator is past-the-end");
		return m_currentObjectMetadata->second.m_transformID;
	}


	template<typename Container>
	inline gr::Transform::RenderData const& RenderDataManager::IDIterator<Container>::GetTransformData() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");
		return m_renderData->GetTransformDataFromTransformID(m_currentObjectMetadata->second.m_transformID);
	}


	template<typename Container>
	inline bool RenderDataManager::IDIterator<Container>::TransformIsDirty() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");

		return m_renderData->TransformIsDirty(m_currentObjectMetadata->second.m_transformID);
	}


	template<typename Container>
	inline gr::FeatureBitmask RenderDataManager::IDIterator<Container>::GetFeatureBits() const
	{
		SEAssert(m_currentObjectMetadata != m_IDToRenderObjectMetadata->cend(),
			"Invalid Get: Current m_currentObjectMetadata is past-the-end");

		return m_currentObjectMetadata->second.m_featureBits;
	}


	template<typename Container>
	inline RenderDataManager::IDIterator<Container>& RenderDataManager::IDIterator<Container>::operator++() // Prefix increment
	{
		while (++m_idsIterator != m_idsEndIterator &&
			gr::HasAllFeatures(m_featureMask,
				m_IDToRenderObjectMetadata->find(*m_idsIterator)->second.m_featureBits) == false)
		{
		}

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


	template<typename Container>
	inline RenderDataManager::IDIterator<Container> RenderDataManager::IDIterator<Container>::operator++(int) // Postfix increment
	{
		IDIterator current = *this;
		++(*this);
		return current;
	}
}

