// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "EventManager.h"
#include "LogManager.h"

using en::EventManager;


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
					const char* line_start = buf + m_lineOffsets[line_no];
					const char* line_end = (line_no + 1 < m_lineOffsets.Size) ? (buf + m_lineOffsets[line_no + 1] - 1) : buf_end;
					if (m_textFilter.PassFilter(line_start, line_end))
					{
						ImGui::TextUnformatted(line_start, line_end);
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
						const char* line_start = buf + m_lineOffsets[line_no];
						const char* line_end = (line_no + 1 < m_lineOffsets.Size) ? (buf + m_lineOffsets[line_no + 1] - 1) : buf_end;
						ImGui::TextUnformatted(line_start, line_end);
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


namespace en
{
	LogManager* LogManager::Get()
	{
		static std::unique_ptr<en::LogManager> instance = std::make_unique<en::LogManager>();
		return instance.get();
	}


	LogManager::LogManager()
	{
		m_imGuiLogWindow = std::make_unique<ImGuiLogWindow>();
	}


	void LogManager::Startup() 
	{
		LOG("Log manager starting...");
	}


	void LogManager::Shutdown()
	{
		LOG("Log manager shutting down...");
	}


	void LogManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		HandleEvents();
	}


	void LogManager::HandleEvents()
	{
		while (HasEvents())
		{
			SEAssertF("We don't have any subscriptions, this is unexpected");
		}	
	}


	void LogManager::ShowImGuiWindow(bool* show)
	{
		if (*show)
		{
			constexpr char const* logWindowTitle = "Saber Engine Log";
			ImGui::Begin(logWindowTitle, show);
			ImGui::End();

			// Actually call in the regular Log helper (which will Begin() into the same window as we just did)
			m_imGuiLogWindow->Draw(logWindowTitle, show);
		}
	}


	void LogManager::AddMessage(std::string&& msg)
	{
		m_imGuiLogWindow->AddLog(msg.c_str());

		// Print the message to the terminal. Note: We might get different ordering since m_imGuiLogWindow internally
		// locks a mutex before appending the new message
		printf(msg.c_str());
	}


	void LogManager::AssembleStringFromVariadicArgs(char* buf, uint32_t bufferSize, const char* msg, ...)
	{
		va_list args;
		va_start(args, msg);
		const int numChars = vsprintf_s(buf, bufferSize, msg, args);
		SEAssert("Message is larger than the buffer size; it will be truncated",
			static_cast<uint32_t>(numChars) < bufferSize);
		va_end(args);
	}


	void LogManager::AssembleStringFromVariadicArgs(wchar_t* buf, uint32_t bufferSize, const wchar_t* msg, ...)
	{
		va_list args;
		va_start(args, msg);
		const int numChars = vswprintf_s(buf, bufferSize, msg, args);
		SEAssert("Message is larger than the buffer size; it will be truncated",
			static_cast<uint32_t>(numChars) < bufferSize);
		va_end(args);
	}


	std::string LogManager::FormatStringForLog(char const* prefix, const char* tag, char const* assembledMsg)
	{
		std::ostringstream stream;
		if (prefix)
		{
			stream << prefix;
		}
		if (tag)
		{
			stream << tag;
		}
		stream << assembledMsg << "\n";

		return stream.str();
	}


	std::wstring LogManager::FormatStringForLog(wchar_t const* prefix, wchar_t const* tag, wchar_t const* assembledMsg)
	{
		std::wstringstream stream;
		if (prefix)
		{
			stream << prefix;
		}
		if (tag)
		{
			stream << tag;
		}
		stream << assembledMsg << "\n";

		return stream.str();
	}
}