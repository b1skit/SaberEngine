#pragma once

#include <string>


namespace en
{
	class SaberObject
	{
	public:
		SaberObject(std::string const& name) :
			m_name(name)
		{

			objectID = AssignObjectID();
		}

		SaberObject() = delete;
		
		virtual ~SaberObject() = 0;
		virtual void Update() = 0;

		// Getters/Setters:
		inline unsigned long GetObjectID() const { return objectID; }

		inline std::string const& GetName() const { return m_name; }

		// Used to hash objects when inserting into an unordered_map
		inline std::string const& GetHashString() const { return m_hashString; }
		

	protected:
		unsigned long objectID; // Hashed value

	private:
		std::string const m_name;
		std::string m_hashString;

		std::hash<std::string> m_hashFunction;

		// Utilities:
		unsigned long AssignObjectID()
		{
			static unsigned long objectIDs{ 0 }; // Initializes to 0 the 1st time this function is called

			m_hashString = m_name + std::to_string(objectIDs++); // Append a number to get different hashes for same name
			size_t hash = m_hashFunction(m_hashString);
			return (unsigned long) hash;
		}
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline SaberObject::~SaberObject() {}
}