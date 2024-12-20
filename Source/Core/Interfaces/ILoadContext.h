// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	template<typename T>
	class InvPtr;


	// Visitor interface: Inherit from this to handle specific loading cases
	template<typename T>
	struct ILoadContext
	{
		virtual ~ILoadContext() = default;

		inline virtual void OnLoadBegin(core::InvPtr<T>) {};

		inline virtual std::unique_ptr<T> Load(core::InvPtr<T>) = 0;

		inline virtual void OnLoadComplete(core::InvPtr<T>) {}; // Called after loading, but before the InvPointer state is updated

		bool m_isPermanent = false; // If true, the resource will not be deleted when the last InvPtr goes out of scope
	};
}