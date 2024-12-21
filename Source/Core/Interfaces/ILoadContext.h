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

		// This is executed on the calling thread, before any async load work is kicked off. Use this to notify any 
		// systems that might need a copy of the InvPtr immediately
		inline virtual void OnLoadBegin(core::InvPtr<T>) {};

		// Async: The bulk of the loading and creation should be done here
		inline virtual std::unique_ptr<T> Load(core::InvPtr<T>) = 0;

		// Async: Called after loading completes, and the InvPointer state has been set to Ready
		inline virtual void OnLoadComplete(core::InvPtr<T>) {}; 

		bool m_isPermanent = false; // If true, the resource will not be deleted when the last InvPtr goes out of scope
	};
}