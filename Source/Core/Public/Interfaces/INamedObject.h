// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Assert.h"

#include "Core/Util/HashKey.h"
#include "Core/Util/TextUtils.h"



namespace core
{
	class INamedObject
	{
	public:
		static constexpr size_t k_maxNameLength = 260; // Windows MAX_PATH = 260 chars, including null terminator


	public: 
		virtual ~INamedObject() = default;


	public:
		explicit INamedObject(char const* name);
		explicit INamedObject(std::string const& name);

		INamedObject(INamedObject const&) = default;
		INamedObject(INamedObject&&) noexcept = default;
		INamedObject& operator=(INamedObject const&) = default;
		INamedObject& operator=(INamedObject&&) noexcept = default;


	public:
		// m_name as supplied at construction
		std::string const& GetName() const;
		std::wstring const& GetWName() const;

		util::HashKey GetNameHash() const;

		// Update the name of an object. Does not modify the UniqueID assigned at creation
		void SetName(std::string const& name);


	private:
		std::string m_name;
		std::wstring m_wName;
		util::HashKey m_nameHash;
		

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


	inline util::HashKey INamedObject::GetNameHash() const
	{
		return m_nameHash;
	}


	inline void INamedObject::SetName(std::string const& name)
	{
		m_name = name;
		m_nameHash = util::HashKey(name);
		
		m_wName = util::ToWideString(m_name);
	}
}