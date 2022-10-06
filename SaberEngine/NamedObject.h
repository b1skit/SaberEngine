#pragma once

#include <string>


namespace en
{
	class NamedObject
	{
	public:
		explicit NamedObject(std::string const& name) :
			m_name(name), 
			m_nameID(ComputeIDFromName(name))
		{
			ComputeUniqueID();
		}

		virtual ~NamedObject() = 0;

		// m_name as supplied at construction
		inline std::string const& GetName() const { return m_name; }

		// Integer identifier computed by hasing m_name. Any object with the same m_name will have the same NameID
		inline size_t GetNameID() const { return m_nameID; }

		// Unique integer identifier, hashed from m_name concatenated with a monotonically-increasing value
		inline size_t GetUniqueID() const { return m_uniqueID; }

		// Compute an integer identifier from a string equivalent to the GetNameID() of objects with the same name
		inline static size_t ComputeIDFromName(std::string const& name) { return std::hash<std::string>{}(name); }

	protected:
		const std::string m_name;

	private:
		size_t m_uniqueID;
		size_t m_nameID;

		void ComputeUniqueID()
		{
			// Hash the name and a unique digit; Will be unique for all objects regardless of their name
			static size_t objectIDs{ 0 };
			const std::string hashString = m_name + std::to_string(objectIDs++); 
			m_uniqueID = std::hash<std::string>{}(hashString);	
		}

	private:
		NamedObject() = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline NamedObject::~NamedObject() {}
}