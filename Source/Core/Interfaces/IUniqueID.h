// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"

using UniqueID = uint64_t;


namespace core
{
	class IUniqueID
	{
	public:
		static constexpr UniqueID k_invalidUniqueID = std::numeric_limits<UniqueID>::max();

	public:
		virtual ~IUniqueID() = 0;

	public:
		IUniqueID();

		IUniqueID(IUniqueID const&) = default;
		IUniqueID(IUniqueID&&) = default;
		IUniqueID& operator=(IUniqueID const&) = default;
		IUniqueID& operator=(IUniqueID&&) = default;


	public:
		UniqueID GetUniqueID() const; // Unique integer identifier


	private:
		void AssignUniqueID();


	private:
		UniqueID m_uniqueID;
	};


	inline IUniqueID::IUniqueID()
	{
		AssignUniqueID();
	}


	inline UniqueID IUniqueID::GetUniqueID() const
	{
		return m_uniqueID;
	}


	inline void IUniqueID::AssignUniqueID()
	{
		// We assign a simple monotonically-increasing value as a unique identifier
		static std::atomic<UniqueID> s_uniqueIDs = 0;
		m_uniqueID = s_uniqueIDs.fetch_add(1);
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline IUniqueID::~IUniqueID() {}
}