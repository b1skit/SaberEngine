// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Assert.h"

#include "Core/Interfaces/IUniqueID.h"

#include "Core/Util/TextUtils.h"

using NameID = uint64_t;


namespace core
{
	class INamedObject : public virtual IUniqueID
	{
	public:
		static constexpr NameID k_invalidNameID = std::numeric_limits<NameID>::max();
		static constexpr size_t k_maxNameLength = 260; // Windows MAX_PATH = 260 chars, including null terminator


	public:
		// Compute an integer identifier from a string equivalent to the GetNameID() of objects with the same name
		static NameID ComputeIDFromName(std::string const& name);


	public: 
		virtual ~INamedObject() = 0;


	public:
		explicit INamedObject(char const* name);
		explicit INamedObject(std::string const& name);

		INamedObject(INamedObject const&) = default;
		INamedObject(INamedObject&&) = default;
		INamedObject& operator=(INamedObject const&) = default;
		INamedObject& operator=(INamedObject&&) = default;


	public:
		// m_name as supplied at construction
		std::string const& GetName() const;
		std::wstring const& GetWName() const;

		// Integer identifier computed by hashing m_name. Any object with the same m_name will have the same NameID
		NameID GetNameID() const;

		// Update the name of an object. Does not modify the UniqueID assigned at creation
		void SetName(std::string const& name);


	private:
		std::string m_name;
		std::wstring m_wName;
		NameID m_nameID;
		

	private:
		INamedObject() = delete;
	};


	inline INamedObject::INamedObject(char const* name)
	{
		SEAssert(strnlen_s(name, k_maxNameLength) > 0 && strnlen_s(name, k_maxNameLength) < k_maxNameLength,
			"Empty, null, or non-terminated name strings are not allowed");

		SetName(name);
	}


	inline INamedObject::INamedObject(std::string const& name)
		: INamedObject(name.c_str())
	{
	}


	inline std::string const& INamedObject::GetName() const
	{
		return m_name;
	}


	inline std::wstring const& INamedObject::GetWName() const
	{
		return m_wName;
	}


	inline NameID INamedObject::GetNameID() const
	{
		return m_nameID;
	}


	inline NameID INamedObject::ComputeIDFromName(std::string const& name)
	{
		return std::hash<std::string>{}(name);
	}


	inline void INamedObject::SetName(std::string const& name)
	{
		m_name = name;
		m_nameID = ComputeIDFromName(name);
		
		m_wName = util::ToWideString(m_name);
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline INamedObject::~INamedObject() {}
}