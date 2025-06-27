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
		explicit INamedObject(std::string_view name);

		INamedObject(INamedObject const&) = default;
		INamedObject(INamedObject&&) noexcept = default;
		INamedObject& operator=(INamedObject const&) = default;
		INamedObject& operator=(INamedObject&&) noexcept = default;


	public:
		// m_name as supplied at construction
		std::string const& GetName() const noexcept;
		std::wstring const& GetWName() const noexcept;

		util::HashKey GetNameHash() const noexcept;

		// Update the name of an object. Does not modify the UniqueID assigned at creation
		void SetName(std::string_view name);


	private:
		std::string m_name;
		std::wstring m_wName;
		util::HashKey m_nameHash;
		

	private:
		INamedObject() = delete;
	};


	inline INamedObject::INamedObject(std::string_view name)
	{
		SEAssert(name.size() > 0 && name.size() < k_maxNameLength,
			"Empty or excessively long name strings are not allowed");
		SEAssert(name.data()[name.size()] == '\0', "std::string_view must be null-terminated for INamedObject usage");

		SetName(name);
	}


	inline std::string const& INamedObject::GetName() const noexcept
	{
		return m_name;
	}


	inline std::wstring const& INamedObject::GetWName() const noexcept
	{
		return m_wName;
	}


	inline util::HashKey INamedObject::GetNameHash() const noexcept
	{
		return m_nameHash;
	}


	inline void INamedObject::SetName(std::string_view name)
	{
		SEAssert(name.data()[name.size()] == '\0', "std::string_view must be null-terminated for INamedObject usage");

		m_name = name;
		m_nameHash = util::HashKey(name.data());
		
		m_wName = util::ToWideString(m_name);
	}
}