// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	class NamedObject
	{
	public: 
		virtual ~NamedObject() = 0;

	public:
		inline explicit NamedObject(std::string const& name);

		// m_name as supplied at construction
		inline std::string const& GetName() const { return m_name; }
		inline std::wstring const& GetWName() const { return m_wName; }

		// Integer identifier computed by hasing m_name. Any object with the same m_name will have the same NameID
		inline uint64_t GetNameID() const { return m_nameID; }

		// Unique integer identifier, hashed from m_name concatenated with a monotonically-increasing value
		inline uint64_t GetUniqueID() const { return m_uniqueID; }

		// Compute an integer identifier from a string equivalent to the GetNameID() of objects with the same name
		inline static uint64_t ComputeIDFromName(std::string const& name);

		// Update the name of an object. Does not modify the UniqueID assigned at creation
		inline void SetName(std::string const& name);

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


	NamedObject::NamedObject(std::string const& name)
		: m_name(name)
		, m_nameID(ComputeIDFromName(name))
	{
		ComputeUniqueID();
	}


	uint64_t NamedObject::ComputeIDFromName(std::string const& name)
	{
		return std::hash<std::string>{}(name);
	}


	void NamedObject::SetName(std::string const& name)
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