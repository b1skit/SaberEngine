// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
#include "CastUtils.h"
#include "RenderObjectIDs.h"
#include "ThreadProtector.h"
#include "TransformComponent.h"


namespace gr
{
	// Render-thread-side scene data. Data is set via the render command queue (on a single thread), and graphics
	// systems index use constant forward iterators to access it
	class RenderDataManager
	{
	public:
		RenderDataManager() = default;
		~RenderDataManager() = default;

		void Destroy();

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
		void DestroyObjectData(gr::RenderDataID);

		template<typename T>
		uint32_t GetNumElementsOfType() const;


	private:
		// Transform interface.
		// We treat Transforms as a special case because all render objects require a Transform, and we expect
		// Transforms to be the largest and most frequently updated data mirrored on the render thread. It's likely many
		// render objects share a transform (e.g. multiple mesh primitives), so we can minimize duplicated effort.
		void RegisterTransform(gr::TransformID);
		void UnregisterTransform(gr::TransformID);

	public:
		void SetTransformData(gr::TransformID, gr::Transform::RenderData const&);
		[[nodiscard]] gr::Transform::RenderData const& GetTransformData(gr::TransformID) const;


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

			LinearIterator& operator++(); // Prefix increment
			LinearIterator operator++(int); // Postfix increment

			// These are declared as friends so they're not classified as member functions
			friend bool operator==(LinearIterator const& lhs, LinearIterator const& rhs) { return lhs.m_ptr == rhs.m_ptr; }
			friend bool operator!=(LinearIterator const& lhs, LinearIterator const& rhs) { return lhs.m_ptr != rhs.m_ptr; }


		private:
			T const* m_ptr;
			T const* m_endPtr;
		};

	private:
		typedef std::vector<uint32_t> ObjectTypeToDataIndexTable; // [data type index] == object index

		struct RenderObjectMetadata
		{
			ObjectTypeToDataIndexTable m_objectTypeToDataIndexTable;
			
			gr::TransformID m_transformID;

			uint32_t m_referenceCount;
		};

		struct TransformMetadata
		{
			uint32_t m_transformIdx;
			uint32_t m_referenceCount;
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
				std::tuple<Ts const*...> endPtrs);


		public:
			template<typename T>
			[[nodiscard]] T const& Get() const;

			gr::RenderDataID GetRenderDataID() const;

			gr::Transform::RenderData const& GetTransformData() const;

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
		

	private:
		template<typename T>
		uint8_t GetAllocateDataIndexFromType();

		template<typename T>
		uint8_t GetDataIndexFromType() const;

		template<typename T>
		T const* GetObjectDataVectorIfExists(uint32_t index) const;

		template <typename T>
		T const* GetEndPtr() const; // Iterator begin/end helper


	private:
		static constexpr uint8_t k_invalidDataTypeIdx = std::numeric_limits<uint8_t>::max();
		static constexpr uint32_t k_invalidDataIdx = std::numeric_limits<uint32_t>::max();

		// Each type of render data is tightly packed into an array maintained in m_dataVectors
		std::map<size_t, uint8_t> m_typeInfoHashToDataVectorIdx;
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
		util::ThreadProtector m_threadProtector;
	};


	template<typename T>
	void RenderDataManager::SetObjectData(gr::RenderDataID objectID, T const* data)
	{
		static const uint8_t s_dataTypeIndex = GetAllocateDataIndexFromType<T>();

		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		SEAssert("Data type index is OOB", s_dataTypeIndex < m_dataVectors.size());
		std::vector<T>& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[s_dataTypeIndex]).get();

		SEAssert("Invalid object ID", m_IDToRenderObjectMetadata.contains(objectID));
		RenderObjectMetadata& renderObjectMetadata = m_IDToRenderObjectMetadata.at(objectID);

		// If the data index table doesn't contain enough room for the data type index, increase its size and pad with
		// the invalid flag
		if (s_dataTypeIndex >= renderObjectMetadata.m_objectTypeToDataIndexTable.size())
		{
			renderObjectMetadata.m_objectTypeToDataIndexTable.resize(s_dataTypeIndex + 1, k_invalidDataIdx);
		}

		// Get the index of the data in data vector for its type
		const uint32_t dataIndex = renderObjectMetadata.m_objectTypeToDataIndexTable[s_dataTypeIndex];
		if (dataIndex == k_invalidDataIdx)
		{
			// This is the first time we've added data for this object, we must store the destination index
			const uint32_t newDataIndex = util::CheckedCast<uint32_t>(dataVector.size());
			dataVector.emplace_back(*data);
			renderObjectMetadata.m_objectTypeToDataIndexTable[s_dataTypeIndex] = newDataIndex;
		}
		else
		{
			dataVector[dataIndex] = *data;
		}
	}


	template<typename T>
	T const& RenderDataManager::GetObjectData(gr::RenderDataID renderDataID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		SEAssert("renderDataID is not registered", m_IDToRenderObjectMetadata.contains(renderDataID));

		const uint8_t dataTypeIndex = GetDataIndexFromType<T>();
		SEAssert("Invalid data type index. This suggests we're accessing data of a specific type using an index, when "
			"no data of that type exists", 
			dataTypeIndex != k_invalidDataTypeIdx && dataTypeIndex < m_dataVectors.size());
		
		RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);
		SEAssert("Metadata table does not have an entry for the current data type", 
			dataTypeIndex < renderObjectMetadata.m_objectTypeToDataIndexTable.size());
		const size_t dataIdx = renderObjectMetadata.m_objectTypeToDataIndexTable[dataTypeIndex];

		std::vector<T> const& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[dataTypeIndex]).get();
		SEAssert("Object index is OOB", dataIdx < dataVector.size());

		return dataVector[dataIdx];
	}


	template<typename T>
	T const* RenderDataManager::GetObjectDataVectorIfExists(uint32_t dataIndex) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		const uint8_t dataTypeIndex = GetDataIndexFromType<T>();
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
		const uint8_t dataTypeIndex = GetDataIndexFromType<T>();
		if (dataTypeIndex == k_invalidDataTypeIdx)
		{
			return 0;
		}

		std::vector<T> const& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[dataTypeIndex]).get();
		
		return util::CheckedCast<uint32_t>(dataVector.size());
	}


	template<typename T>
	void RenderDataManager::DestroyObjectData(gr::RenderDataID objectID)
	{
		const uint8_t dataTypeIndex = GetDataIndexFromType<T>();

		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		SEAssert("Data index is OOB", dataTypeIndex < m_dataVectors.size());
		SEAssert("Invalid object ID", m_IDToRenderObjectMetadata.contains(objectID));
		RenderObjectMetadata& renderObjectMetadata = m_IDToRenderObjectMetadata.at(objectID);

		std::vector<T>& dataVector = *std::static_pointer_cast<std::vector<T>>(m_dataVectors[dataTypeIndex]).get();

		// Replace our dead element with one from the end:
		const size_t indexToMove = dataVector.size() - 1;
		const uint32_t indexToReplace = renderObjectMetadata.m_objectTypeToDataIndexTable[dataTypeIndex];
		SEAssert("Object does not have data of this type to destroy", indexToReplace != k_invalidDataIdx);

		if (indexToMove != indexToReplace)
		{
			dataVector[indexToReplace] = dataVector[indexToMove];

			// Find whatever table was referencing the index we moved, and update it. This is expensive, as we iterate
			// over every RenderDataID until we find a match
			bool foundMatch = false;
			for (auto& objectDataIndices : m_IDToRenderObjectMetadata)
			{
				ObjectTypeToDataIndexTable& dataIndexTableToUpdate = objectDataIndices.second.m_objectTypeToDataIndexTable;
				if (dataTypeIndex < dataIndexTableToUpdate.size() && dataIndexTableToUpdate[dataTypeIndex] == indexToMove)
				{
					dataIndexTableToUpdate[dataTypeIndex] = indexToReplace;
					foundMatch = true;
					break;
				}				
			}
			SEAssert("Matching object was not found. This should not be possible", foundMatch);
		}
		dataVector.pop_back();

		// Finally, remove the index in the object's data index table:
		renderObjectMetadata.m_objectTypeToDataIndexTable[dataTypeIndex] = k_invalidDataIdx;
	}


	template<typename T>
	uint8_t RenderDataManager::GetAllocateDataIndexFromType()
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		static uint8_t s_dataTypeIdx = std::numeric_limits<uint8_t>::max();
		if (s_dataTypeIdx == std::numeric_limits<uint8_t>::max())
		{
			s_dataTypeIdx = util::CheckedCast<uint8_t>(m_dataVectors.size());
			m_dataVectors.emplace_back(std::make_shared<std::vector<T>>());

			// Store a map of the typeID hash code to the data type index for const access
			m_typeInfoHashToDataVectorIdx.emplace(typeid(T).hash_code(), s_dataTypeIdx);

			// Guarantee there is always some backing memory allocated for our iterator .end() to reference
			std::vector<T>& dataVector =
				*std::static_pointer_cast<std::vector<T>>(m_dataVectors.back()).get();

			constexpr size_t k_expectedMaxNumDataTypes = 16;
			dataVector.reserve(k_expectedMaxNumDataTypes);
		}
		return s_dataTypeIdx;
	}


	template<typename T>
	uint8_t RenderDataManager::GetDataIndexFromType() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		static uint8_t s_dataTypeIdx = std::numeric_limits<uint8_t>::max();
		if (s_dataTypeIdx == std::numeric_limits<uint8_t>::max())
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

		const uint8_t s_dataTypeIndex = GetDataIndexFromType<T>();
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

		const uint8_t dataTypeIndex = GetDataIndexFromType<T>();
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
			std::make_tuple(GetEndPtr<Ts>()...));
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
			std::make_tuple(GetEndPtr<Ts>()...));
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
		std::tuple<Ts const*...> endPtrs)
		: m_endPtrs(endPtrs)
		, m_renderData(renderData)
		, m_renderObjectMetadataItr(objectIDToRenderObjectMetadataBeginItr)
		, m_renderObjectMetadataEndItr(objectIDToRenderObjectMetadataEndItr)
	{
		// Find our first valid set of starting pointers:
		bool hasValidPtrs = false;
		while (m_renderObjectMetadataItr != m_renderObjectMetadataEndItr)
		{
			// We have a valid set of gr::RenderDataID data indices. Make a tuple from them
			m_ptrs = std::make_tuple(GetPtrFromCurrentObjectDataIndicesItr<Ts>()...);

			// If the current object doesn't have a data of the given type (i.e. any of the tuple elements rae null), we
			// want to skip over it to the next object:
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
			SEAssert("We should have checked every element, how is this possible?",
				m_renderObjectMetadataItr == m_renderObjectMetadataEndItr);

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

	
	template<typename... Ts>
	gr::RenderDataID RenderDataManager::ObjectIterator<Ts...>::GetRenderDataID() const
	{
		return m_renderObjectMetadataItr->first;
	}


	template <typename... Ts>
	gr::Transform::RenderData const& RenderDataManager::ObjectIterator<Ts...>::GetTransformData() const
	{
		return m_renderData->GetTransformData(m_renderObjectMetadataItr->second.m_transformID);
	}


	template <typename... Ts> template <typename T>
	T const* RenderDataManager::ObjectIterator<Ts...>::GetPtrFromCurrentObjectDataIndicesItr() const
	{
		const uint8_t dataTypeIndex = m_renderData->GetDataIndexFromType<T>();

		ObjectTypeToDataIndexTable const& objectTypeToDataIndexTable = 
			m_renderObjectMetadataItr->second.m_objectTypeToDataIndexTable;

		// Make sure the data type index is in bounds of the current object's data index table (it could be invalid, or
		// the table might not have allocated an entry for this type)
		if (dataTypeIndex < objectTypeToDataIndexTable.size()) // Guarantees dataTypeIndex != k_invalidDataTypeIdx
		{
			// Get the index of the data within its typed array, for the current RenderDataID iteration
			const uint32_t objectDataIndex = objectTypeToDataIndexTable[dataTypeIndex];

			return m_renderData->GetObjectDataVectorIfExists<T>(objectDataIndex);
		}
		return nullptr;
	}
}
