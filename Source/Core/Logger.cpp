// © 2022 Adam Badke. All rights reserved.
#include "Logger.h"
#include "ThreadPool.h"

#include "Definitions/ConfigKeys.h"


namespace
{
	// This is just a modified version of the ImGui log window demo
	struct ImGuiLogWindow
	{
		ImGuiTextBuffer m_textBuffer;
		std::mutex m_textBufferMutex;

		ImGuiTextFilter m_textFilter;
		ImVector<int> m_lineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
		bool m_autoScroll;  // Keep scrolling if already at the bottom.
		
		static constexpr ImVec4 s_logColor = ImVec4(1,1,1,1);
		static constexpr ImVec4 s_warningColor = ImVec4(1, 0.404f, 0.f, 1);
		static constexpr ImVec4 s_errorColor = ImVec4(1, 0, 0, 1);

		ImGuiLogWindow()
		{
			m_autoScroll = true;
			Clear();
		}

		void Clear()
		{
			{
				std::lock_guard<std::mutex> lock(m_textBufferMutex);
				m_textBuffer.clear();
			}

			m_lineOffsets.clear();
			m_lineOffsets.push_back(0);
		}

		void AddLog(const char* fmt, ...) IM_FMTARGS(2)
		{
			std::lock_guard<std::mutex> lock(m_textBufferMutex);

			int oldSize = m_textBuffer.size();
			va_list args;
			va_start(args, fmt);
			m_textBuffer.appendfv(fmt, args);
			va_end(args);
			for (int new_size = m_textBuffer.size(); oldSize < new_size; oldSize++)
			{
				if (m_textBuffer[oldSize] == '\n')
				{
					m_lineOffsets.push_back(oldSize + 1);
				}
			}
		}

		ImVec4 const& GetTextColor(const char* lineStart, ImVec4 const*& lastLineColor)
		{
			// We update the lastLineColor to maintain consistent colors for multi-line cases without a prefix
			if (strncmp(lineStart, logging::k_logPrefix, logging::k_logPrefixLen) == 0)
			{
				lastLineColor = &s_logColor;
				return s_logColor;
			}
			else if (strncmp(lineStart, logging::k_warnPrefix, logging::k_warnPrefixLen) == 0)
			{
				lastLineColor = &s_warningColor;
				return s_warningColor;
			}
			else if (strncmp(lineStart, logging::k_errorPrefix, logging::k_errorPrefixLen) == 0)
			{
				lastLineColor = &s_errorColor;
				return s_errorColor;
			}
			else
			{
				return *lastLineColor;
			}
		}

		void Draw(const char* title, bool* p_open = NULL)
		{
			if (!ImGui::Begin(title, p_open))
			{
				ImGui::End();
				return;
			}

			// Options menu
			if (ImGui::BeginPopup("Options"))
			{
				ImGui::Checkbox("Auto-scroll", &m_autoScroll);
				ImGui::EndPopup();
			}

			// Main window
			if (ImGui::Button("Options"))
			{
				ImGui::OpenPopup("Options");
			}
			ImGui::SameLine();
			const bool clear = ImGui::Button("Clear");
			ImGui::SameLine();
			const bool copy = ImGui::Button("Copy");
			ImGui::SameLine();
			m_textFilter.Draw("Filter", -100.0f);

			ImGui::Separator();
			ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

			if (clear)
			{
				Clear();
			}
			if (copy)
			{
				ImGui::LogToClipboard();
			}

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

			ImVec4 const* lastLineColor = &s_logColor;

			std::lock_guard<std::mutex> lock(m_textBufferMutex);
			const char* buf = m_textBuffer.begin();
			const char* buf_end = m_textBuffer.end();
			if (m_textFilter.IsActive())
			{
				// In this example we don't use the clipper when Filter is enabled.
				// This is because we don't have a random access on the result on our filter.
				// A real application processing logs with ten of thousands of entries may want to store the result of
				// search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
				for (int line_no = 0; line_no < m_lineOffsets.Size; line_no++)
				{
					const char* lineStart = buf + m_lineOffsets[line_no];
					const char* lineEnd = (line_no + 1 < m_lineOffsets.Size) ? (buf + m_lineOffsets[line_no + 1] - 1) : buf_end;
					if (m_textFilter.PassFilter(lineStart, lineEnd))
					{
						ImGui::PushStyleColor(ImGuiCol_Text, GetTextColor(lineStart, lastLineColor));
						ImGui::TextUnformatted(lineStart, lineEnd);
						ImGui::PopStyleColor();
					}
				}
			}
			else
			{
				// The simplest and easy way to display the entire buffer:
				//   ImGui::TextUnformatted(buf_begin, buf_end);
				// And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward
				// to skip non-visible lines. Here we instead demonstrate using the clipper to only process lines that are
				// within the visible area.
				// If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them
				// on your side is recommended. Using ImGuiListClipper requires
				// - A) random access into your data
				// - B) items all being the  same height,
				// both of which we can handle since we an array pointing to the beginning of each line of text.
				// When using the filter (in the block of code above) we don't have random access into the data to display
				// anymore, which is why we don't use the clipper. Storing or skimming through the search result would make
				// it possible (and would be recommended if you want to search through tens of thousands of entries).
				ImGuiListClipper clipper;
				clipper.Begin(m_lineOffsets.Size);
				while (clipper.Step())
				{
					for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
					{
						const char* lineStart = buf + m_lineOffsets[line_no];
						const char* lineEnd = (line_no + 1 < m_lineOffsets.Size) ? (buf + m_lineOffsets[line_no + 1] - 1) : buf_end;

						ImGui::PushStyleColor(ImGuiCol_Text, GetTextColor(lineStart, lastLineColor));
						ImGui::TextUnformatted(lineStart, lineEnd);
						ImGui::PopStyleColor();
					}
				}
				clipper.End();
			}
			ImGui::PopStyleVar();

			if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			{
				ImGui::SetScrollHereY(1.0f);
			}

			ImGui::EndChild();
			ImGui::End();
		}
	};
}


namespace core
{
	std::unique_ptr<ImGuiLogWindow> Logger::s_imGuiLogWindow = nullptr;
	bool Logger::s_isRunning = false;
	bool Logger::s_showHostConsole = false;

	std::queue<std::array<char, Logger::k_internalStagingBufferSize>> Logger::s_messages;
	std::mutex Logger::s_messagesMutex;
	std::condition_variable Logger::s_messagesCV;

	std::ofstream Logger::s_logOutputStream;


	// ---


	void Logger::Startup(bool isSystemConsoleWindowEnabled)
	{
		LOG("Log manager starting...");

		s_isRunning = true; // Start running *before* we kick off a thread
		s_showHostConsole = isSystemConsoleWindowEnabled;

		s_imGuiLogWindow = std::make_unique<ImGuiLogWindow>();

		core::ThreadPool::EnqueueJob([]()
			{
				core::ThreadPool::NameCurrentThread(L"Logger Thread");
				Run();
			});
	}


	void Logger::Shutdown()
	{
		LOG("Log manager shutting down...");
		s_isRunning = false;

		FlushMessages(); // Flush any remaining messages on the queue

		s_logOutputStream.close();
		s_imGuiLogWindow = nullptr;
	}


	void Logger::PrintMessage(char const* msg)
	{
		s_imGuiLogWindow->AddLog(msg);

		// Print the message to the terminal. Note: We might get different ordering since s_imGuiLogWindow
		// internally locks a mutex before appending the new message
		if (s_showHostConsole)
		{
			printf(msg);
		}

		s_logOutputStream << msg;
		s_logOutputStream.flush(); // Flush every time to keep the log up to date
	};


	void Logger::FlushMessages()
	{
		assert(!s_isRunning && "Flushing messages while running. This is unexpected");

		{
			std::unique_lock<std::mutex> modifyLock(s_messagesMutex);

			while (!s_messages.empty())
			{
				PrintMessage(s_messages.front().data());
				s_messages.pop();
			}
		}
	}


	void Logger::Run()
	{
		std::filesystem::create_directory(core::configkeys::k_logOutputDir); // No error if the directory already exists

		s_logOutputStream.open(
			std::format("{}{}", core::configkeys::k_logOutputDir, core::configkeys::k_logFileName).c_str(),
			std::ios::out);
		assert(s_logOutputStream.good() && "Error creating log output stream");

		std::array<char, Logger::k_internalStagingBufferSize> messageBuffer{ '\0' };

		while (s_isRunning)
		{
			std::unique_lock<std::mutex> waitingLock(s_messagesMutex);
			s_messagesCV.wait(waitingLock,
				[]() { return !s_messages.empty() || !s_isRunning; }); // while (!stop_waiting())
			if (!s_isRunning)
			{
				return;
			}

			// Copy the front message into the intermediate buffer, then release the lock so more messages can be added
			strcpy(messageBuffer.data(), s_messages.back().data());
			s_messages.pop();

			waitingLock.unlock();

			PrintMessage(messageBuffer.data());
		}
	}


	void Logger::ShowImGuiWindow(bool* show)
	{
		if (*show)
		{
			constexpr char const* logWindowTitle = "Saber Engine Log";
			ImGui::Begin(logWindowTitle, show);
			ImGui::End();

			// Actually call in the regular Log helper (which will Begin() into the same window as we just did)
			s_imGuiLogWindow->Draw(logWindowTitle, show);
		}
	}


	void Logger::AddMessage(char const* msg)
	{
		{
			std::unique_lock<std::mutex> lock(s_messagesMutex);
			s_messages.emplace();
			strcpy(s_messages.back().data(), msg);
		}
		s_messagesCV.notify_one();
	}
}