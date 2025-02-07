// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class RenderStage;
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
		static std::unique_ptr<RLibrary> Create(Type);

		static void Execute(re::RenderStage*, void* platformObject);


	public:
		virtual void Destroy() = 0;


	public:
		virtual ~RLibrary() = default;
	};
}
