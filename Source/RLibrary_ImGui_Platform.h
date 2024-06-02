// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "RLibrary_Platform.h"
#include "RenderStage.h"

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
			uint64_t m_currentFrameNum;
			core::FrameIndexedCommandManager* m_perFrameCommands;
		};


	public:
		static std::unique_ptr<platform::RLibrary>(*Create)();


	protected:
		static void CreateInternal(RLibraryImGui&); // Common/platform-agnostic creation & ImGui setup steps


	public:
		virtual void Execute(re::RenderStage*) = 0;
		virtual void Destroy() = 0;


	public:
		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<PlatformParams> params) { m_platformParams = std::move(params); }


	private:
		std::unique_ptr<PlatformParams> m_platformParams;
	};


	inline RLibraryImGui::PlatformParams::~PlatformParams() {}
}