// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "RLibrary_Platform.h"
#include "Stage.h"

#include "Core/CommandQueue.h"
#include "Core/Interfaces/IPlatformObject.h"


namespace en
{
	class FrameIndexedCommandManager;
}

namespace platform
{
	class RLibraryImGui : public virtual RLibrary
	{
	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual ~PlatObj() = 0;
		};
		static void CreatePlatformObject(RLibraryImGui&);


	public:
		struct Payload final : public virtual re::LibraryStage::IPayload
		{
			uint64_t m_currentFrameNum = std::numeric_limits<uint64_t>::max();
			core::FrameIndexedCommandManager* m_perFrameCommands = nullptr;
		};


	public:
		static std::unique_ptr<platform::RLibrary>(*Create)();


	protected:
		static void CreateInternal(RLibraryImGui&); // Common/platform-agnostic creation & ImGui setup steps
		static void ConfigureScaling(RLibraryImGui&);

	public:
		virtual void Execute(gr::Stage*, void* platformObject) = 0;
		virtual void Destroy() = 0;


	public:
		inline PlatObj* GetPlatformObject() const { return m_platObj.get(); }
		void SetPlatformObject(std::unique_ptr<PlatObj> platObj) { m_platObj = std::move(platObj); }


	private:
		std::unique_ptr<PlatObj> m_platObj;
	};


	inline RLibraryImGui::PlatObj::~PlatObj() {}
}