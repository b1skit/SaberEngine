// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"


namespace accesscontrol
{
	/*******************************************************************************************************************
	* Access control system: 
	* This is a zero-runtime-cost system for restricting access to certain functions to specific types (e.g. for
	* enforcing ordering, thread safety, etc).
	* 
	* AccessKeys provide compile-time access control, ensuring that only allowed types can call certain functions.
	* 
	* Usage:
	* 1) Declare a public AccessKey with the allowed types:
	*	using AccessKey = accesscontrol::AccessKey<List, Of, Allowed, Types>;
	* 
	* 2) Use the AccessKey in the function signature:
	*	void SomeClass::RestrictedFunction(AccessKey, ...);
	* 
	* 3) Callers in the allowed types can create an AccessKey using the macro helper:
	*	m_someClass.RestrictedFunction(ACCESS_KEY(Allowed::AccessKey), ...);
	********************************************************************************************************************/

	template<typename... AllowedTypes>
	class AccessKey;

	namespace internal
	{
		// Helper trait to check if a type is in a parameter pack
		template<typename T, typename... Types>
		struct IsOneOf : std::false_type {};

		template<typename T, typename First, typename... Rest>
		struct IsOneOf<T, First, Rest...>
			: std::conditional_t<std::is_same_v<T, First>, std::true_type, IsOneOf<T, Rest...>>
		{};

		template<typename T, typename... Types>
		inline constexpr bool IsOneOf_v = IsOneOf<T, Types...>::value;

		// Trait to check if Caller is allowed access based on AllowedTypes
		template<typename Caller, typename... AllowedTypes>
		struct IsAccessAllowed : std::false_type {};

		template<typename Caller, typename... AllowedTypes>
		struct IsAccessAllowed<Caller, AccessKey<AllowedTypes...>>
			: std::bool_constant<IsOneOf_v<Caller, AllowedTypes...>>
		{};
	} // internal


	// Access key class - zero-size with compile-time access control
	template<typename... AllowedTypes>
	class AccessKey
	{
	public:
		// Only allowed types can construct this key
		template<typename Caller>
		explicit constexpr AccessKey(Caller*)
		{
			SEStaticAssert((internal::IsOneOf_v<Caller, AllowedTypes...>), "Type is not allowed to access this function");
		}

		// No copies/key passing allowed
		AccessKey(const AccessKey&) = delete;
		AccessKey(AccessKey&&) = delete;
		AccessKey& operator=(const AccessKey&) = delete;
		AccessKey& operator=(AccessKey&&) = delete;
	};

	// Convenience macro to create an access key
#define ACCESS_KEY(AccessorType) AccessorType{static_cast<std::decay_t<decltype(*this)>*>(nullptr)}

	// Key creation: Alternative function-based approach
	template<typename Caller, typename... AllowedTypes>
	constexpr AccessKey<AllowedTypes...> CreateAccessKey()
	{
		return AccessKey<AllowedTypes...>{static_cast<Caller*>(nullptr)};
	}

} // accesscontrol

