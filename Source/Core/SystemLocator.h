// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"


namespace core
{
	class SystemLocator
	{
	public:
		template<typename SystemType, typename AccessKey>
		static void Register(AccessKey, SystemType* serviceInstance)
		{
			SEStaticAssert((std::is_same_v<AccessKey, typename SystemType::AccessKey>),
				"Invalid service access");

			SEAssert(Access<SystemType>() == nullptr, "Service is already registered");
			Access<SystemType>() = serviceInstance;
		}


		template<typename SystemType, typename AccessKey>
		static void Unregister(AccessKey)
		{
			SEStaticAssert((std::is_same_v<AccessKey, typename SystemType::AccessKey>),
				"Invalid service access");

			Access<SystemType>() = nullptr;
		}


		template<typename SystemType, typename AccessKey>
		static SystemType* Get(AccessKey)
		{
			SEStaticAssert((std::is_same_v<AccessKey, typename SystemType::AccessKey>),
				"Invalid service access");

			SystemType* serviceInstance = Access<SystemType>();
			SEAssert(serviceInstance != nullptr, "Service has not been registered");
			return serviceInstance;
		}


	private:
		template<typename SystemType>
		static SystemType*& Access()
		{
			static SystemType* s_serviceInstance = nullptr;

#if defined(_DEBUG)
			static SystemLocatorValidator<SystemType> s_sanitizer{};
#endif

			return s_serviceInstance;
		}


	private:
		template<typename SystemType>
		class SystemLocatorValidator
		{
		public:
			~SystemLocatorValidator()
			{
				SEAssert(SystemLocator::Access<SystemType>() == nullptr, "Service was not unregistered");
			}
		};
	};
}