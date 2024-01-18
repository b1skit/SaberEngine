// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"


namespace en
{
	class NamedObject
	{
	public:
		static constexpr size_t k_maxNameLength = 260; // Windows MAX_PATH = 260 chars, including null terminator

	public: 
		virtual ~NamedObject() = 0;

	public:
		explicit NamedObject(char const* name);
		explicit NamedObject(std::string const& name);

		// m_name as supplied at construction
		std::string const& GetName() const;
		std::wstring const& GetWName() const;

		// Integer identifier computed by hashing m_name. Any object with the same m_name will have the same NameID
		uint64_t GetNameID() const;

		// Unique integer identifier, hashed from m_name concatenated with a monotonically-increasing value
		uint64_t GetUniqueID() const;

		// Update the name of an object. Does not modify the UniqueID assigned at creation
		void SetName(std::string const& name);


	public:
		// Compute an integer identifier from a string equivalent to the GetNameID() of objects with the same name
		static uint64_t ComputeIDFromName(std::string const& name);
		

	private:
		inline void AssignUniqueID();

	private:
		std::string m_name;
		std::wstring m_wName;
		uint64_t m_nameID;
		uint64_t m_uniqueID;
		
	private:
		NamedObject() = delete;
	};


	inline NamedObject::NamedObject(char const* name)
	{
		SEAssert(strnlen_s(name, k_maxNameLength) > 0 && strnlen_s(name, k_maxNameLength) < k_maxNameLength,
			"Empty, null, or non-terminated name strings are not allowed");

		SetName(name);
		AssignUniqueID();
	}


	inline NamedObject::NamedObject(std::string const& name)
		: NamedObject(name.c_str())
	{
	}


	inline std::string const& NamedObject::GetName() const
	{
		return m_name;
	}


	inline std::wstring const& NamedObject::GetWName() const
	{
		return m_wName;
	}


	inline uint64_t NamedObject::GetNameID() const
	{
		return m_nameID;
	}


	inline uint64_t NamedObject::GetUniqueID() const
	{
		return m_uniqueID;
	}


	inline uint64_t NamedObject::ComputeIDFromName(std::string const& name)
	{
		return std::hash<std::string>{}(name);
	}


	inline void NamedObject::SetName(std::string const& name)
	{
		m_name = name;
		m_nameID = ComputeIDFromName(name);

		m_wName = std::wstring(m_name.begin(), m_name.end()).c_str();
	}


	void NamedObject::AssignUniqueID()
	{
		// We assign a simple monotonically-increasing value as a unique identifier
		static std::atomic<uint64_t> s_uniqueIDs = 0;
		m_uniqueID = s_uniqueIDs.fetch_add(1);
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline NamedObject::~NamedObject() {}
}