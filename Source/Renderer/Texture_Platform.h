// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	template<typename T>
	class InvPtr;
}

namespace re
{
	class Texture;
}

namespace platform
{
	bool RegisterPlatformFunctions();


	class Texture
	{
	public:
		static void CreatePlatformObject(re::Texture& texture);


		// API-specific function bindings:
		/*********************************/
	public:
		static void CreateAPIResource(core::InvPtr<re::Texture> const&, void* platformObject);
	private:
		friend bool RegisterPlatformFunctions();
		static void (*Create)(core::InvPtr<re::Texture> const&, void*); // Use CreateAPIResource()

	public:
		static void (*Destroy)(re::Texture&);
		static void (*ShowImGuiWindow)(core::InvPtr<re::Texture> const&, float scale);
	};
}

