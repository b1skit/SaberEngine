// An interface for all Saber Engine objects.
// Contains common fields and methods (Eg. identifiers) useful for all Saber Engine objects

#pragma once

#include <string>

using std::string;


// Global variables: These should never be modified directly.
namespace
{
	static unsigned long objectIDs = 0;
}


namespace SaberEngine
{
	// Predeclarations:
	class CoreEngine;

	class SaberObject
	{
	public:
		SaberObject(string name)
		{
			if (!name.length() == 0) // Default to "unnamed" if no valid name is received
			{
				name = name;
			}

			objectID = AssignObjectID();
		}

		SaberObject() = delete;
		
		virtual ~SaberObject() = 0;
		virtual void Update() = 0;

		// Getters/Setters:
		inline unsigned long GetObjectID() { return objectID; }

		inline string GetName() const { return name; }

		// Used to hash objects when inserting into an unordered_map
		inline string GetHashString() { return m_hashString; }
		

	protected:
		unsigned long objectID; // Hashed value

	private:
		string name = "unnamed";
		string m_hashString;

		std::hash<string> m_hashFunction;

		// Utilities:
		unsigned long AssignObjectID()
		{ 
			m_hashString = name + std::to_string(objectIDs++); // Append a number to get different hashes for same name
			size_t hash = m_hashFunction(m_hashString);

			return (unsigned long) hash;
		}
	};


	// We need to provide a destructor implementation since it's pure virutal
	inline SaberObject::~SaberObject() {}
}