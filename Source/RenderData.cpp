// © 2023 Adam Badke. All rights reserved.
#include "Camera.h"
#include "RenderData.h"


namespace gr
{
	void RenderData::Destroy()
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		// TODO: Uncomment this once we've rearranged the shutdown order. Currently fires because the render manager is
		// being destroyed before the gameplay mgr
		/*SEAssert("Object ID to data indices map is not empty: Was a render object not destroyed via a render command?",
			m_objectIDToDataIndices.empty());*/
	}


	void RenderData::RegisterObject(gr::RenderObjectID objectID)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		SEAssert("Trying to register an object that already exists", !m_objectIDToDataIndices.contains(objectID));

		m_objectIDToDataIndices.emplace(objectID, ObjectTypeToDataIndexTable());
	}


	void RenderData::DestroyObject(gr::RenderObjectID objectID)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		SEAssert("Trying to destroy an object that does not exist", m_objectIDToDataIndices.contains(objectID));

		ObjectTypeToDataIndexTable const& dataIndexTable = m_objectIDToDataIndices.at(objectID);

#if defined(_DEBUG)
		for (size_t dataIndexEntry = 0; dataIndexEntry < dataIndexTable.size(); dataIndexEntry++)
		{
			SEAssert("Cannot destroy an object with first destroying its associated ata",
				dataIndexTable[dataIndexEntry] == k_invalidDataIdx);
		}
#endif

		m_objectIDToDataIndices.erase(objectID);
	}
}