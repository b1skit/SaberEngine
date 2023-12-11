// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace gr
{
	typedef uint32_t RenderObjectID;


	// Automatically assigns itself a unique RenderObjectID
	struct RenderDataComponent
	{
		RenderDataComponent() : m_objectID(s_objectIDs.fetch_add(1)) {}

		gr::RenderObjectID m_objectID;

	private:
		static std::atomic<gr::RenderObjectID> s_objectIDs;
	};


	// ---


	class CreateRenderObjectCommand
	{
	public:
		CreateRenderObjectCommand(gr::RenderObjectID);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderObjectID m_objectID;
	};


	// ---


	class DestroyRenderObjectCommand
	{
	public:
		DestroyRenderObjectCommand(gr::RenderObjectID);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderObjectID m_objectID;
	};
}
