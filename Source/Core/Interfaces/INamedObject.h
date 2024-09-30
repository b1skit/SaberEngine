// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Assert.h"

#include "Core/Interfaces/IUniqueID.h"

#include "Core/Util/TextUtils.h"


namespace core
{
	// Convenience/efficiency wrapper for associative containers - Hash of a name string.
	class NameHash
	{
	public:
		static constexpr uint64_t k_invalidNameHash = std::numeric_limits<uint64_t>::max();

	public:
		NameHash(std::string const& name) : m_nameHash(std::hash<std::string>{}(name)) {}

		NameHash() : m_nameHash(k_invalidNameHash) {}; // Invalid

		~NameHash() = default;
		NameHash(NameHash const&) = default;
		NameHash(NameHash&&) noexcept = default;
		NameHash& operator=(NameHash const&) = default;
		NameHash& operator=(NameHash&&) noexcept = default;

		bool operator==(NameHash const& rhs) const { return m_nameHash == rhs.m_nameHash; }
		bool operator<(NameHash const& rhs) const { return m_nameHash < rhs.m_nameHash; }
		bool operator>(NameHash const& rhs) const { return m_nameHash > rhs.m_nameHash; }

		bool operator()(NameHash const& lhs, NameHash const& rhs) const { return lhs.m_nameHash == rhs.m_nameHash; }
		bool operator()(NameHash const& rhs) const { return m_nameHash == rhs.m_nameHash; }

		uint64_t Get() const { return m_nameHash; }
		bool IsValid() const { return m_nameHash != k_invalidNameHash; }


	private:
		uint64_t m_nameHash;
	};	


	class INamedObject : public virtual IUniqueID
	{
	public:
		static constexpr size_t k_maxNameLength = 260; // Windows MAX_PATH = 260 chars, including null terminator


	public: 
		virtual ~INamedObject() = 0;


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

		// Any object with the same name string will have the same NameHash
		NameHash GetNameHash() const;

		// Update the name of an object. Does not modify the UniqueID assigned at creation
		void SetName(std::string const& name);


	private:
		std::string m_name;
		std::wstring m_wName;
		NameHash m_nameHash;
		

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


	inline NameHash INamedObject::GetNameHash() const
	{
		return m_nameHash;
	}


	inline void INamedObject::SetName(std::string const& name)
	{
		m_name = name;
		m_nameHash = NameHash(name);
		
		m_wName = util::ToWideString(m_name);
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline INamedObject::~INamedObject() {}
}


// Hash functions for our NameHash, to allow it to be used as a key in an associative container
template<>
struct std::hash<core::NameHash>
{
	std::size_t operator()(core::NameHash const& nameHash) const
	{
		return nameHash.Get();
	}
};


template <>
struct std::formatter<core::NameHash>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}


	template <typename FormatContext>
	auto format(const core::NameHash& nameHash, FormatContext& ctx)
	{
		return std::format_to(ctx.out(), "{}", nameHash.Get());
	}
};