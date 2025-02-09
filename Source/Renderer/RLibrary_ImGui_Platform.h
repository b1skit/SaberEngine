// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "RLibrary_Platform.h"
#include "Stage.h"

#include "Core/CommandQueue.h"
#include "Core/Interfaces/IPlatformParams.h"


namespace en
{
	class FrameIndexedCommandManager;
}

namespace platform
{
	class RLibraryImGui : public virtual RLibrary
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
		};
		static void CreatePlatformParams(RLibraryImGui&);


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
		virtual void Execute(re::Stage*, void* platformObject) = 0;
		virtual void Destroy() = 0;


	public:
		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<PlatformParams> params) { m_platformParams = std::move(params); }


	private:
		std::unique_ptr<PlatformParams> m_platformParams;
	};


	inline RLibraryImGui::PlatformParams::~PlatformParams() {}
}