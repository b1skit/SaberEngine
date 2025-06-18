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
		
		// Perfect forwarding constructor for string-like types
		template<typename StringType, 
			typename = std::enable_if_t<!std::is_same_v<std::decay_t<StringType>, INamedObject> &&
				std::is_constructible_v<std::string, StringType>>>
		explicit INamedObject(StringType&& name);

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
		void SetName(std::string_view name);
		void SetName(std::string&& name);


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

		SetName(std::string(name));
	}


	template<typename StringType, typename>
	inline INamedObject::INamedObject(StringType&& name)
	{
		// Create string directly from forwarded parameter to avoid temporary
		std::string nameStr(std::forward<StringType>(name));
		
		SEAssert(nameStr.size() > 0 && nameStr.size() < k_maxNameLength,
			"Empty or excessively long name strings are not allowed");

		SetName(std::move(nameStr));
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


	inline void INamedObject::SetName(std::string_view name)
	{
		SEAssert(name.data()[name.size()] == '\0', "std::string_view must be null-terminated for INamedObject usage");

		m_name = name;
		m_nameHash = util::HashKey(name.data());
		
		m_wName = util::ToWideString(m_name);
	}


	inline void INamedObject::SetName(std::string&& name)
	{
		m_name = std::move(name);
		m_nameHash = util::HashKey(m_name.c_str());
		
		m_wName = util::ToWideString(m_name);
	}
}