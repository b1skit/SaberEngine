// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"


namespace en
{
	class NamedObject
	{
	public: 
		virtual ~NamedObject() = 0;

	public:
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
		inline void ComputeUniqueID();

	private:
		std::string m_name;
		std::wstring m_wName;
		uint64_t m_nameID;
		uint64_t m_uniqueID;
		
	private:
		NamedObject() = delete;
	};


	class NameComponent
	{
	public:
		NameComponent(char const* name) { strcpy(m_name, name); }
		NameComponent(std::string const& name) { strcpy(m_name, name.c_str()); }
		
		char const* GetName() { return m_name; }


	private:
		static constexpr uint32_t k_maxNameLength = 128;
		char m_name[1024];
	};


	inline NamedObject::NamedObject(std::string const& name)
	{
		SEAssert("Empty name strings are not allowed", !name.empty());

		SetName(name);
		ComputeUniqueID();
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


	void NamedObject::ComputeUniqueID()
	{
		// Hash the name and a unique digit; Will be unique for all objects regardless of their name
		static std::atomic<uint64_t> objectIDs = 0;
		const uint64_t thisObjectID = objectIDs.fetch_add(1);
		const std::string hashString = m_name + std::to_string(thisObjectID);
		m_uniqueID = std::hash<std::string>{}(hashString);
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline NamedObject::~NamedObject() {}
}