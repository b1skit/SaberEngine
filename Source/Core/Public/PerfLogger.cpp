// © 2025 Adam Badke. All rights reserved.
#include "PerfLogger.h"

#include "../Assert.h"
#include "../Config.h"
#include "../Definitions/EventKeys.h"


namespace core
{
	PerfLogger* PerfLogger::Get()
	{
		static std::unique_ptr<core::PerfLogger> instance = std::make_unique<core::PerfLogger>();
		return instance.get();
	}


	PerfLogger::PerfLogger()
		: m_numFramesInFlight(core::Config::Get()->GetValue<int>(core::configkeys::k_numBackbuffersKey))
		, m_isEnabled(false)
	{
		core::EventManager::Get()->Subscribe(eventkey::TogglePerformanceTimers, this);
	}


	PerfLogger::~PerfLogger()
	{
		Destroy();
	}


	void PerfLogger::BeginFrame()
	{
		HandleEvents();

		if (m_isEnabled.load() == false)
		{
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_perfLoggerMutex);

			// Update our lifetime counters, and erase anything that ages out
			for (auto recordItr = m_times.begin(); recordItr != m_times.end(); )
			{
				recordItr->second.m_numFramesSinceUpdated++;

				if (recordItr->second.m_numFramesSinceUpdated > k_maxFramesWithoutUpdate &&
					recordItr->second.m_children.empty()) // Keep parents alive
				{
					// Remove ourselves from our parent's records:
					if (recordItr->second.m_hasParent)
					{
						SEAssert(m_times.contains(recordItr->second.m_parentNameHash),
							"Parent not found. This should not be possible");

						std::vector<util::HashKey>& parentsChildren = 
							m_times.at(recordItr->second.m_parentNameHash).m_children;

						auto childEntryItr = std::find(parentsChildren.begin(), parentsChildren.end(), recordItr->first);
						SEAssert(childEntryItr != parentsChildren.end(),
							"Failed to find child record. This should not be possible");

						parentsChildren.erase(childEntryItr);
					}

					// Remove ourselves as a parent from any child records:
					for (auto& childNameHash : recordItr->second.m_children)
					{
						SEAssert(m_times.contains(childNameHash), "Child record not found. This should not be possible");

						TimeRecord& childRecord = m_times.at(childNameHash);

						SEAssert(childRecord.m_hasParent, "Child not marked as having a parent. This should not be possible");

						childRecord.m_hasParent = false;
						childRecord.m_parentName = std::string(/*empty*/);
						childRecord.m_parentNameHash = util::HashKey();
					}

					if (recordItr->second.m_timer.IsRunning())
					{
						recordItr->second.m_timer.StopMs();
					}

					recordItr = m_times.erase(recordItr);
				}
				else
				{
					++recordItr;
				}
			}
		}
	}


	PerfLogger::TimeRecord& PerfLogger::AddUpdateTimeRecordHelper(
		char const* name,
		char const* parentName /*= nullptr*/)
	{
		// Note: Internal helper function: We assume m_perfLoggerMutex is already locked

		SEAssert(m_isEnabled.load(), "Timer is not enabled");

		const util::HashKey nameHash(name);
		const bool hasParent = parentName != nullptr;

		auto recordItr = m_times.find(nameHash);
		if (recordItr == m_times.end())
		{
			recordItr = m_times.emplace(
				nameHash,
				TimeRecord{
					.m_name = name,
					.m_nameHash = nameHash,
					.m_parentName = hasParent ? parentName : std::string(/*empty*/),
					.m_parentNameHash = hasParent ? util::HashKey(parentName) : util::HashKey(),
					.m_mostRecentTimeMs = 0.0,
					.m_hasParent = hasParent,
					.m_numFramesSinceUpdated = 0,
				}).first;

			if (hasParent)
			{
				// If the parent has not been found, recursively add it:
				TimeRecord* parentRecord = nullptr;
				if (!m_times.contains(recordItr->second.m_parentNameHash))
				{
					parentRecord = &AddUpdateTimeRecordHelper(parentName, nullptr);
				}
				else
				{
					parentRecord = &m_times.at(recordItr->second.m_parentNameHash);
				}
				parentRecord->m_children.emplace_back(nameHash);
			}
		}
		else
		{
			recordItr->second.m_numFramesSinceUpdated = 0;

			// If our record was recursively pre-created by a child, ensure our own parent is correctly recorded
			if (hasParent && !recordItr->second.m_hasParent)
			{
				recordItr->second.m_hasParent = true;
				recordItr->second.m_parentName = parentName;
				recordItr->second.m_parentNameHash = util::HashKey(parentName);

				if (!m_times.contains(recordItr->second.m_parentNameHash))
				{
					AddUpdateTimeRecordHelper(parentName, nullptr);
				}

				m_times.at(recordItr->second.m_parentNameHash).m_children.emplace_back(nameHash);
			}
		}

		SEAssert(!hasParent ||
			(recordItr->second.m_hasParent && m_times.contains(recordItr->second.m_parentNameHash)),
			"Parent inconsistency");

		if (hasParent) // Keep parents alive if their children are being updated
		{
			TimeRecord& parentRecord = m_times.at(recordItr->second.m_parentNameHash);
			parentRecord.m_numFramesSinceUpdated = 0;
		}

		return recordItr->second;
	}


	void PerfLogger::NotifyBegin(char const* name, char const* parentName /*= nullptr*/)
	{
		if (m_isEnabled.load() == false)
		{
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_perfLoggerMutex);

			TimeRecord& record = PerfLogger::AddUpdateTimeRecordHelper(name, parentName);

			record.m_timer.Start();
		}
	}


	void PerfLogger::NotifyEnd(char const* name)
	{
		if (m_isEnabled.load() == false)
		{
			return;
		}

		const util::HashKey nameHash(name);

		{
			std::lock_guard<std::mutex> lock(m_perfLoggerMutex);

			auto recordItr = m_times.find(nameHash);
			if (recordItr != m_times.end())
			{
				if (recordItr->second.m_timer.IsRunning()) // Might not be running (e.g. 1st update in a loop)
				{
					recordItr->second.m_mostRecentTimeMs = recordItr->second.m_timer.StopMs();
				}
			}
		}
	}


	void PerfLogger::NotifyPeriod(double totalTimeMs, char const* name, char const* parentName /*= nullptr*/)
	{
		if (m_isEnabled.load() == false)
		{
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_perfLoggerMutex);

			TimeRecord& record = AddUpdateTimeRecordHelper(name, parentName);
			SEAssert(!record.m_timer.IsRunning(), "Timer is running, this is invalid for manual time periods");

			record.m_mostRecentTimeMs = totalTimeMs;
		}
	}


	void PerfLogger::HandleEvents()
	{
		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_eventKey)
			{
			case eventkey::TogglePerformanceTimers:
			{
				m_isEnabled.store(std::get<bool>(eventInfo.m_data));

				if (!m_isEnabled.load())
				{
					Destroy();
				}
			}
			break;			
			default:
				break;
			}
		}
	}


	void PerfLogger::Destroy()
	{
		{
			std::lock_guard<std::mutex> lock(m_perfLoggerMutex);

			for (auto& record : m_times)
			{
				if (record.second.m_timer.IsRunning())
				{
					record.second.m_timer.StopMs();
				}
			}
			m_times.clear();
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

		static OverlayLocation s_location = OverlayLocation::TopRight;

		constexpr float k_padding = 10.0f;
		ImGuiViewport const* viewport = ImGui::GetMainViewport();

		ImVec2 const& workPos = viewport->WorkPos; // Use work area to avoid menu/task-bar, if any
		ImVec2 const& workSize = viewport->WorkSize;

		const ImVec2 windowPos(
			(s_location & 1) ? (workPos.x + workSize.x - k_padding) : (workPos.x + k_padding),
			(s_location & 2) ? (workPos.y + workSize.y - k_padding) : (workPos.y + k_padding));
		
		const ImVec2 windowPosPivot(
			(s_location & 1) ? 1.f : 0.f,	// Right / left
			(s_location & 2) ? 1.f : 0.f);	// Bottom / top

		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
		ImGui::SetNextWindowViewport(viewport->ID);
		
		ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

		if (ImGui::Begin("Performance logger overlay", show, windowFlags))
		{
			constexpr ImVec4 k_defaultColor = ImVec4(0.f, 1.f, 0.f, 1.f);
			constexpr ImVec4 k_warningColor = ImVec4(1.f, 0.404f, 0.f, 1.f);
			constexpr ImVec4 k_alertColor = ImVec4(1.f, 0.f, 0.f, 1.f);

			auto BuildRecordText = [](TimeRecord const& record) -> std::string
				{
					if (!record.m_hasParent) // Root node: Show the ms -> FPS conversion
					{
						return std::format("{}{}",
							record.m_name,
							record.m_mostRecentTimeMs == 0.0 ? // Don't show a time if none was recorded (e.g. untimed parent)
							""
							: std::format(": {:6.2f}ms /{:8.2f}fps",
								record.m_mostRecentTimeMs,
								record.m_mostRecentTimeMs > 0 ? 1000.0 / record.m_mostRecentTimeMs : 0.0).c_str());
					}
					else // Don't show the ms -> FPS conversion for child nodes
					{
						return std::format("{}: {:6.2f}ms",
							record.m_name,
							record.m_mostRecentTimeMs);
					}
				};

			{
				std::lock_guard<std::mutex> lock(m_perfLoggerMutex);
			
				for (auto const& record : m_times)
				{
					if (record.second.m_hasParent)
					{
						continue; // Nested records are printed by their parent
					}

					// Must fully specialize our function object to be able to recursively call it:
					std::function<void(TimeRecord const&)> RecursiveNodeDisplay;
					RecursiveNodeDisplay = 
						[&BuildRecordText, this, &RecursiveNodeDisplay, &k_defaultColor, &k_warningColor, &k_alertColor]
						(TimeRecord const& record)
						{
							std::string const& recordTex = BuildRecordText(record);

							// Set the color for the upcoming element:
							if (record.m_mostRecentTimeMs < k_warnThresholdMs)
							{
								ImGui::PushStyleColor(ImGuiCol_Text, k_defaultColor);
							}
							else if (record.m_mostRecentTimeMs < k_alertThresholdMs)
							{
								ImGui::PushStyleColor(ImGuiCol_Text, k_warningColor);
							}
							else
							{
								ImGui::PushStyleColor(ImGuiCol_Text, k_alertColor);
							}

							// Hide the ">" icon if an entry has no children
							const ImGuiTreeNodeFlags flags = record.m_children.empty() ?
								ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet
								: ImGuiTreeNodeFlags_None;

							if (ImGui::TreeNodeEx(std::format("##{}", record.m_name).c_str(), flags))
							{
								ImGui::SameLine();
								ImGui::Text(recordTex.c_str());

								for (auto const& childKey : record.m_children)
								{
									RecursiveNodeDisplay(m_times.at(childKey));
								}

								ImGui::TreePop();
							}
							else
							{
								ImGui::SameLine();
								ImGui::Text(recordTex.c_str());
							}

							// Cleanup:
							ImGui::PopStyleColor();
						};

					RecursiveNodeDisplay(record.second);
				}
			}

			if (ImGui::BeginPopupContextWindow())
			{
				if (ImGui::MenuItem("Top-left", nullptr, s_location == TopLeft))
				{
					s_location = TopLeft;
				}
				if (ImGui::MenuItem("Top-right", nullptr, s_location == TopRight))
				{
					s_location = TopRight;
				}
				if (ImGui::MenuItem("Bottom-left", nullptr, s_location == BottomLeft))
				{
					s_location = BottomLeft;
				}
				if (ImGui::MenuItem("Bottom-right", nullptr, s_location == BottomRight))
				{
					s_location = BottomRight;
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