// © 2022 Adam Badke. All rights reserved.
#pragma once


using UniqueID = uint64_t;
static constexpr UniqueID k_invalidUniqueID = std::numeric_limits<UniqueID>::max();

namespace core
{
	class IUniqueID
	{
	public:
		virtual ~IUniqueID() = default;

	public:
		IUniqueID();

		IUniqueID(IUniqueID const&) = default;
		IUniqueID(IUniqueID&&) noexcept = default;
		IUniqueID& operator=(IUniqueID const&) = default;
		IUniqueID& operator=(IUniqueID&&) noexcept = default;


	public:
		UniqueID GetUniqueID() const noexcept; // Unique integer identifier


	private:
		void AssignUniqueID();


	private:
		UniqueID m_uniqueID;
	};


	inline IUniqueID::IUniqueID()
	{
		AssignUniqueID();
	}


	inline UniqueID IUniqueID::GetUniqueID() const noexcept
	{
		return m_uniqueID;
	}


	inline void IUniqueID::AssignUniqueID()
	{
		// We assign a simple monotonically-increasing value as a unique identifier
		static std::atomic<UniqueID> s_uniqueIDs = 0;
		m_uniqueID = s_uniqueIDs.fetch_add(1);
	}
}