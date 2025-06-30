// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace gr
{
	class Stage;
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

		static void Execute(gr::Stage*, void* platformObject);


	public:
		virtual void Destroy() = 0;


	public:
		virtual ~RLibrary() = default;
	};
}
