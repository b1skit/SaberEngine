// © 2025 Adam Badke. All rights reserved.
#include "PerfLogger.h"

#include "../Assert.h"


namespace core
{
	PerfLogger* PerfLogger::Get()
	{
		static std::unique_ptr<core::PerfLogger> instance = std::make_unique<core::PerfLogger>();
		return instance.get();
	}


	PerfLogger::~PerfLogger()
	{
		for (auto& record : m_times)
		{
			if (record.second.m_timer.IsRunning())
			{
				record.second.m_timer.StopMs();
			}
		}
	}


	void PerfLogger::Register(util::CHashKey key, double warnThresholdMs /*= 14.0*/, double alertThresholdMs /*= 16.0*/)
	{
		{
			std::lock_guard<std::shared_mutex> lock(m_timesMutex);

			m_times.emplace(
				key, 
				TimeRecord{
					.m_warnThresholdMs = warnThresholdMs,
					.m_alertThresholdMs = alertThresholdMs});
		}
	}


	void PerfLogger::NotifyBegin(util::CHashKey key)
	{
		{
			// We're modifying existing entries in place, don't need an exclusive lock
			std::shared_lock<std::shared_mutex> readLock(m_timesMutex);

			SEAssert(m_times.contains(key), "Key not found, was it registered?");

			TimeRecord& record = m_times.at(key);

			m_times.at(key).m_timer.Start();
		}
	}


	void PerfLogger::NotifyEnd(util::CHashKey key)
	{
		{
			// We're modifying existing entries in place, don't need an exclusive lock.
			// Note: There's a potential issue here where multiple threads could modify the same record, but that's
			// invalid usage of this system
			std::shared_lock<std::shared_mutex> readLock(m_timesMutex);

			SEAssert(m_times.contains(key), "Key not found, was it registered?");

			TimeRecord& record = m_times.at(key);

			if (record.m_timer.IsRunning()) // Might not be running (e.g. 1st update in a loop)
			{
				record.m_mostRecentTimeMs = record.m_timer.StopMs();
			}
		}
	}


	void PerfLogger::NotifyPeriod(util::CHashKey key, double totalTimeMs)
	{
		{
			// We're modifying existing entries in place, don't need an exclusive lock
			std::shared_lock<std::shared_mutex> readLock(m_timesMutex);

			SEAssert(m_times.contains(key), "Key not found, was it registered?");

			TimeRecord& record = m_times.at(key);

			SEAssert(!record.m_timer.IsRunning(), "Timer is running, This is invalid if manually setting the period");

			record.m_mostRecentTimeMs = totalTimeMs;
		}
	}


	void PerfLogger::ShowImGuiWindow(bool* show)
	{
		enum OverlayLocation : uint8_t
		{
			TopLeft,
			TopRight,
			BottomLeft,
			BottomRight,
		};

		constexpr ImGuiWindowFlags windowFlags = 
			ImGuiWindowFlags_NoDecoration | 
			ImGuiWindowFlags_NoDocking | 
			ImGuiWindowFlags_AlwaysAutoResize | 
			ImGuiWindowFlags_NoSavedSettings | 
			ImGuiWindowFlags_NoFocusOnAppearing | 
			ImGuiWindowFlags_NoNav | 
			ImGuiWindowFlags_NoMove;

		ImGuiIO& io = ImGui::GetIO();

		static OverlayLocation location = OverlayLocation::TopRight;

		constexpr float k_padding = 10.0f;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImVec2 const& workPos = viewport->WorkPos; // Use work area to avoid menu/task-bar, if any
		ImVec2 const& workSize = viewport->WorkSize;

		const ImVec2 windowPos(
			(location & 1) ? (workPos.x + workSize.x - k_padding) : (workPos.x + k_padding),
			(location & 2) ? (workPos.y + workSize.y - k_padding) : (workPos.y + k_padding));
		
		const ImVec2 windowPosPivot(
			(location & 1) ? 1.f : 0.f,
			(location & 2) ? 1.f : 0.f);

		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
		ImGui::SetNextWindowViewport(viewport->ID);
		
		ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

		if (ImGui::Begin("Performance logger overlay", show, windowFlags))
		{
			constexpr ImVec4 k_defaultColor = ImVec4(0.f, 1.f, 0.f, 1.f);
			constexpr ImVec4 k_warningColor = ImVec4(1.f, 0.404f, 0.f, 1.f);
			constexpr ImVec4 k_alertColor = ImVec4(1.f, 0.f, 0.f, 1.f);

			{
				// We're getting a read lock here, so there is a potential race condition if another thread modifies a
				// record, but we're trying to avoid contention skewing our results
				std::shared_lock<std::shared_mutex> readLock(m_timesMutex);

				for (auto const& record : m_times)
				{
					std::string const& recordTex = std::format("{}: {:.2f}ms / {:.2f}fps",
						record.first.GetKey(),
						record.second.m_mostRecentTimeMs,
						1000.0 / record.second.m_mostRecentTimeMs);

					if (record.second.m_mostRecentTimeMs < record.second.m_warnThresholdMs)
					{
						ImGui::TextColored(k_defaultColor, recordTex.c_str());
					}
					else if (record.second.m_mostRecentTimeMs < record.second.m_alertThresholdMs)
					{
						ImGui::TextColored(k_warningColor, recordTex.c_str());
					}
					else
					{
						ImGui::TextColored(k_alertColor, recordTex.c_str());
					}
				}
			}

			if (ImGui::BeginPopupContextWindow())
			{
				if (ImGui::MenuItem("Top-left", nullptr, location == TopLeft))
				{
					location = TopLeft;
				}
				if (ImGui::MenuItem("Top-right", nullptr, location == TopRight))
				{
					location = TopRight;
				}
				if (ImGui::MenuItem("Bottom-left", nullptr, location == BottomLeft))
				{
					location = BottomLeft;
				}
				if (ImGui::MenuItem("Bottom-right", nullptr, location == BottomRight))
				{
					location = BottomRight;
				}
				if (show && ImGui::MenuItem("Hide"))
				{
					*show = false;
				}

				ImGui::EndPopup();
			}
		}
		ImGui::End();
	}
}