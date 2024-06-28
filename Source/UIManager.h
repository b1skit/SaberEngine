// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IEventListener.h"

#include "Core/Interfaces/IEngineComponent.h"


namespace fr
{
	class UIManager : public virtual en::IEngineComponent, public virtual core::IEventListener
	{
	public:
		static UIManager* Get(); // Singleton functionality


	public:
		UIManager();
		~UIManager() = default;


	public: // IEngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		void HandleEvents() override;


	private:
		void SubmitImGuiRenderCommands(uint64_t frameNum);

		std::atomic<bool> m_debugUIRenderSystemCreated; // True if m_debugUICommandMgr, m_imguiGlobalMutex are safe to use
		core::FrameIndexedCommandManager* m_debugUICommandMgr;
		std::mutex* m_imguiGlobalMutex;


	private:
		bool m_imguiMenuVisible;
		bool m_prevImguiMenuVisible;

		bool m_imguiWantsToCaptureKeyboard;
		bool m_imguiWantsToCaptureMouse;
	};


	// ---


	/*	A helper to cut down on ImGui render command boiler plate.This is not mandatory for submitting commands to the
	*	ImGui command queue, but it should cover most common cases.
	*
	*	Internally, it uses a static unordered_map to link a unique identifier (e.g. a lambda address or some other
	*	value) with ImGui's ImGuiOnceUponAFrame to ensure commands submitted multiple times per frame are only executed
	*	once.
	*
	*	Use this class as a wrapper for a lambda that captures any required data by reference:
	*
	*	auto SomeLambda = [&]() { // Do something };
	*
	*	re::RenderManager::Get()->EnqueueImGuiCommand<re::ImGuiRenderCommand<decltype(SomeLambda)>>(
	*		re::ImGuiRenderCommand<decltype(SomeLambda)>(SomeLambda));
	*
	*	In cases where the lamba being wrapped is called multiple times but capturing different data (e.g. from within
	*	a static function), use the 2nd ctor to supply a unique ID each time
	*/
	template<typename T>
	class ImGuiRenderCommand
	{
	public:
		// When the 
		ImGuiRenderCommand(T wrapperLambda)
			: m_imguiWrapperLambda(wrapperLambda), m_uniqueID(util::PtrToID(&wrapperLambda)) {};

		ImGuiRenderCommand(T wrapperLambda, uint64_t uniqueID)
			: m_imguiWrapperLambda(wrapperLambda), m_uniqueID(uniqueID) {};

		static void Execute(void* cmdData)
		{
			ImGuiRenderCommand<T>* cmdPtr = reinterpret_cast<ImGuiRenderCommand<T>*>(cmdData);
			if (m_uniqueIDToImGuiOnceUponAFrame[cmdPtr->m_uniqueID]) // Insert or access our ImGuiOnceUponAFrame
			{
				cmdPtr->m_imguiWrapperLambda();
			}
		}

		static void Destroy(void* cmdData)
		{
			ImGuiRenderCommand<T>* cmdPtr = reinterpret_cast<ImGuiRenderCommand<T>*>(cmdData);
			cmdPtr->~ImGuiRenderCommand();
		}

	private:
		T m_imguiWrapperLambda;
		uint64_t m_uniqueID;

		static std::unordered_map<uint64_t, ImGuiOnceUponAFrame> m_uniqueIDToImGuiOnceUponAFrame;
	};
	template<typename T>
	inline std::unordered_map<uint64_t, ImGuiOnceUponAFrame> ImGuiRenderCommand<T>::m_uniqueIDToImGuiOnceUponAFrame;
}