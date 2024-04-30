// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	struct IPlatformParams
	{
		IPlatformParams() = default;
		IPlatformParams(IPlatformParams&&) = default;
		IPlatformParams& operator=(IPlatformParams&&) = default;
		virtual ~IPlatformParams() = 0;

		template<typename T>
		T As();

		template<typename T>
		T const As() const;

		// No copying allowed:
		IPlatformParams(IPlatformParams const&) = delete;
		IPlatformParams const& operator=(IPlatformParams const&) = delete;
	};


	template<typename T>
	inline T IPlatformParams::As()
	{
		return static_cast<T>(this);
	}


	template<typename T>
	inline T const IPlatformParams::As() const
	{
		return static_cast<T const>(this);
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline IPlatformParams::~IPlatformParams() {}
}