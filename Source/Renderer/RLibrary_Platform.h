// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class Context;
}
namespace platform
{
	class RLibrary
	{
	public:
		static bool RegisterPlatformLibraries();


	public:
		enum Type : uint8_t
		{
			ImGui,

			Type_Count
		};


	public:
		struct IPayload
		{
			virtual ~IPayload() = default;
		};


	public:
		static std::unique_ptr<RLibrary> Create(Type);

		static void Execute(re::Context* context, Type, std::unique_ptr<IPayload>&&, void* platformObject);


	public:
		virtual void Destroy() = 0;


	public:
		virtual ~RLibrary() = default;
	};
}
