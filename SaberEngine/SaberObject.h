// An interface for all Saber Engine objects.
// Contains common fields and methods (Eg. identifiers) useful for all Saber Engine objects

#pragma once

#include <string>

using std::string;


// Global variables: These should never be modified directly.
namespace SaberEnginePrivate
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

		// Getters/Setters:
		inline unsigned long GetObjectID() { return objectID; }

		inline string GetName() const { return name; }

		// Used to hash objects when inserting into an unordered_map
		inline string GetHashString() { return m_hashString; }

		virtual void Update()	= 0;

	protected:
		unsigned long objectID; // Hashed value
	private:
		string name = "unnamed";
		string m_hashString;


		std::hash<string> m_hashFunction;

		// Utilities:
		unsigned long AssignObjectID()
		{ 
			m_hashString = name + std::to_string(SaberEnginePrivate::objectIDs++); // Append a number to give different hashes for the same name
			size_t hash = m_hashFunction(m_hashString);

			return (unsigned long) hash;
		}
	};
}