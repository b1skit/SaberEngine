// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "EnumTypes.h"

#include "Core/Assert.h"

#include "Core/Host/PerformanceTimer.h"

#include "renderdoc_app.h"


namespace re
{
	class Context;


	class ICapture
	{
	public:
		virtual ~ICapture() = default;


	public:
		bool TriggerCapture(); // Returns true if the capture was successfully triggered, false otherwise
		bool CaptureIsTriggered() const;
		virtual bool CaptureIsComplete() = 0; // Called once per frame to update/poll for completion


	protected:
		virtual bool TriggerCaptureInternal() = 0;


	private:
		bool m_captureTriggered = false;


	protected:
		friend class re::Context;
		static re::Context* s_context;
	};


	// ---


	class RenderDocCapture : public virtual ICapture
	{
	public:
		typedef RENDERDOC_API_1_1_2 RenderDocAPI;
		static RenderDocAPI* InitializeRenderDocAPI(platform::RenderingAPI);

		
	public:
		~RenderDocCapture() override = default;


	public:
		static void RequestGPUCapture(uint32_t numFrames);


	public:
		static void ShowImguiWindow();


	private:
		RenderDocCapture(uint32_t numFrames);


	public: // ICapture:
		bool CaptureIsComplete() override { return true; } // RenderDoc captures are triggered immediately

	private: 
		bool TriggerCaptureInternal() override;


	private:
		uint32_t m_numFrames;
	};


	// ---


	class PIXCapture final : public virtual ICapture
	{
	public:
		struct PIXCPUCaptureSettings
		{
			float m_captureTimeSec = 30.f;
			bool m_captureGPUTimings = true;
			bool m_captureCallstacks = true;
			bool m_captureCpuSamples = true;
			uint32_t m_cpuSamplesPerSecond = 1000;
			bool m_captureFileIO = false;
			bool m_captureVirtualAllocEvents = false;
			bool m_captureHeapAllocEvents = false;
		};

	public:
		static HMODULE InitializePIXCPUCaptureModule();
		static HMODULE InitializePIXGPUCaptureModule();


	public:
		~PIXCapture() override;


	public:
		static void RequestGPUCapture(uint32_t numFrames, std::string const& captureOutputDirectory);
		static void RequestCPUCapture(PIXCPUCaptureSettings const&, std::string const& captureOutputDirectory);


	public:
		static void ShowImguiWindow();


	private:
		enum class CaptureType : uint8_t
		{
			CPU,
			GPU
		};
		PIXCapture(uint32_t numFrames, std::string&& captureOutputDir);
		PIXCapture(PIXCPUCaptureSettings const&, std::string&& captureOutputDir);


	public: // ICapture:
		bool CaptureIsComplete() override;

	private:
		bool TriggerCaptureInternal() override;


	private:
		std::string m_captureOutputDirectory;
		union
		{
			PIXCPUCaptureSettings m_cpuCaptureSettings;
			uint32_t m_numGPUFrames;			
		};
		CaptureType m_type;

		host::PerformanceTimer m_cpuCaptureTimer;
	};


	// ---


	inline bool ICapture::TriggerCapture()
	{
		SEAssert(CaptureIsTriggered() == false, "Capture has already been triggered");

		m_captureTriggered = true;

		return TriggerCaptureInternal();
	}


	inline bool ICapture::CaptureIsTriggered() const
	{
		return m_captureTriggered;
	}
}