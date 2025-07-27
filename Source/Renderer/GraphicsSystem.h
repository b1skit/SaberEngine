// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"
#include "GraphicsEvent.h"
#include "GraphicsSystemCommon.h"
#include "RenderPipeline.h"
#include "Texture.h"

#include "Core/Assert.h"
#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"

#include "Core/Util/CHashKey.h"
#include "Core/Util/TextUtils.h"


namespace re
{
	class TextureTargetSet;
}

namespace gr
{
	class GraphicsSystemManager;
	class RenderSystem;


	class GraphicsSystem : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		// Scriptable pipeline: Create a graphics system by the (case insensitive) name provided in a script.
		// Returns null if no GS with that name exists
		static std::unique_ptr<gr::GraphicsSystem> CreateByName(
			char const* scriptName,
			gr::GraphicsSystemManager*,
			std::vector<std::pair<std::string, std::string>> const& flags);

		static std::unique_ptr<gr::GraphicsSystem> CreateByName(
			std::string const& scriptName,
			gr::GraphicsSystemManager*,
			std::vector<std::pair<std::string, std::string>> const& flags);

		// GraphicsSystem interface:
		// -------------------------
		// GraphicsSystems intentionally have a flexible interface with minimal required virtual functionality.
		// Typically, a Raster System will require 1 or more of the following functions:
		// - InitPipeline(gr::StagePipeline&) method(s): Used to attach a sequence of Stages to a StagePipeline
		// - PreRender() method(s): Called every frame to update the GraphicsSystem before platform-level rendering
		// 
		// ------------------------
		// 
		// To participate in the self-enrolling scriptable rendering pipeline, a GraphicsSystem must:
		//	a) Inherit from the IScriptableGraphicsSystem interface
		//	b) Implement a "static constexpr char const* GetScriptName()" member function
		//	c) Provide an implementation of the pure virtual GetRuntimeBindings() function.
		//		-> Use the macros at the end of this file to reduce boilerplate
		//	d) Provide an implementation of the pure virtual RegisterInputs/Outputs() functions
		//		-> These are called before/after InitPipelineFn execution
	public:
		struct RuntimeBindings
		{
			using InitPipelineFn = std::function<void(
				gr::StagePipeline&,
				TextureDependencies const&,
				BufferDependencies const&,
				DataDependencies const&)>;

			using PreRenderFn = std::function<void()>;

			std::vector<std::pair<std::string, InitPipelineFn>> m_initPipelineFunctions;
			std::vector<std::pair<std::string, PreRenderFn>> m_preRenderFunctions;
		};
		virtual RuntimeBindings GetRuntimeBindings() = 0;

	public:
		virtual void RegisterFlags() {}; // Must override this if you want to register flags via the pipeline script
		virtual void RegisterInputs() = 0;
		virtual void RegisterOutputs() = 0;


		// Flags:
	protected:
		void RegisterFlag(util::CHashKey const&);

		bool HasFlagValue(util::CHashKey const& flagName, util::CHashKey const& valueName) const;
		util::CHashKey const& GetFlagValue(util::CHashKey const& flagName) const;


		// Texture inputs/outputs:
	public:
		enum TextureInputDefault : uint8_t // Optional generic fallbacks
		{
			OpaqueWhite,		// (1,1,1,1)
			TransparentWhite,	// (1,1,1,0)
			OpaqueBlack,		// (0,0,0,1)
			TransparentBlack,	// (0,0,0,0)

			CubeMap_OpaqueWhite,		// (1,1,1,1)
			CubeMap_TransparentWhite,	// (1,1,1,0)
			CubeMap_OpaqueBlack,		// (0,0,0,1)
			CubeMap_TransparentBlack,	// (0,0,0,0)
			
			None,				// Default
			TextureInputDefault_Count
		};

		TextureInputDefault GetTextureInputDefaultType(util::CHashKey const&) const;
		TextureInputDefault GetTextureInputDefaultType(char const* inputScriptName) const;
		TextureInputDefault GetTextureInputDefaultType(std::string const& inputScriptName) const;

		bool HasRegisteredTextureInput(util::CHashKey const& inputScriptName) const;
		bool HasRegisteredTextureInput(char const* inputScriptName) const;
		bool HasRegisteredTextureInput(std::string const& inputScriptName) const;

		std::map<util::CHashKey, TextureInputDefault> const& GetTextureInputs() const;

		core::InvPtr<re::Texture> const* GetTextureOutput(util::CHashKey const& outputScriptName) const;
		core::InvPtr<re::Texture> const* GetTextureOutput(char const* outputScriptName) const;
		core::InvPtr<re::Texture> const* GetTextureOutput(std::string const& outputScriptName) const;

	protected:
		void RegisterTextureInput(util::CHashKey const&, TextureInputDefault = TextureInputDefault::None);
		void RegisterTextureOutput(util::CHashKey const&, core::InvPtr<re::Texture> const*);

	private:
		// These must be populated during the call to RegisterInputs/Outputs()
		std::map<util::CHashKey, TextureInputDefault> m_textureInputs;
		std::map<util::CHashKey, core::InvPtr<re::Texture> const*> m_textureOutputs;
		std::unordered_map<util::CHashKey, util::CHashKey> m_flags;


		// Buffer inputs/outputs:
	public:
		bool HasRegisteredBufferInput(util::CHashKey const& scriptName) const;
		bool HasRegisteredBufferInput(char const* scriptName) const;
		bool HasRegisteredBufferInput(std::string const& scriptName) const;

		std::set<util::CHashKey> const& GetBufferInputs() const;

		std::shared_ptr<re::Buffer> const* GetBufferOutput(util::CHashKey const& scriptName) const;
		std::shared_ptr<re::Buffer> const* GetBufferOutput(char const* scriptName) const;
		std::shared_ptr<re::Buffer> const* GetBufferOutput(std::string const& scriptName) const;

	protected:
		void RegisterBufferInput(util::CHashKey const&);
		void RegisterBufferOutput(util::CHashKey const&, std::shared_ptr<re::Buffer> const*);

	private:
		std::set<util::CHashKey> m_bufferInputs;
		std::map<util::CHashKey, std::shared_ptr<re::Buffer> const*> m_bufferOutputs;


		// Data inputs/outputs:
	public:
		bool HasRegisteredDataInput(util::CHashKey const& scriptName) const;
		bool HasRegisteredDataInput(char const* scriptName) const;
		bool HasRegisteredDataInput(std::string const& scriptName) const;

		std::set<util::CHashKey> const& GetDataInputs() const;

		void const* GetDataOutput(util::CHashKey const& scriptName) const;
		void const* GetDataOutput(char const* scriptName) const;
		void const* GetDataOutput(std::string const& scriptName) const;

	protected:
		void RegisterDataInput(util::CHashKey const&);
		void RegisterDataOutput(util::CHashKey const&, void const*);
		
	private:
		std::set<util::CHashKey> m_dataInputs;
		std::map<util::CHashKey, void const*> m_dataOutputs;


		// Graphics Events:
	public:
		void PostEvent(gr::GraphicsEvent&&);

	protected:
		virtual void HandleEvents(); // Override this
		bool HasEvents() const;
		gr::GraphicsEvent GetEvent();

	private:
		std::queue<gr::GraphicsEvent> m_events;
		mutable std::shared_mutex m_eventsMutex; // Multiple GS's running asyncronously may post events for their dependencies


	public:
		GraphicsSystem(GraphicsSystem&&) noexcept = default;
		GraphicsSystem& operator=(GraphicsSystem&&) noexcept = default;

		virtual ~GraphicsSystem() = default;

		// Override these:
		virtual void EndOfFrame() {};
		virtual void ShowImGuiWindow(); 


	protected:
		gr::GraphicsSystemManager* const m_graphicsSystemManager;


	protected:
		explicit GraphicsSystem(char const* name, gr::GraphicsSystemManager*);


	private: // No copying allowed
		GraphicsSystem() = delete;
		GraphicsSystem(GraphicsSystem const&) = delete;
		GraphicsSystem& operator=(GraphicsSystem const&) = delete;


	protected:
		// Automatic factory registration mechanism:
		template<typename T>
		static std::unique_ptr<gr::GraphicsSystem> Create(
			gr::GraphicsSystemManager* gsm, std::vector<std::pair<std::string, std::string>> const& flags);

		
		using CreateFn = std::unique_ptr<gr::GraphicsSystem>(*)(
			gr::GraphicsSystemManager*, std::vector<std::pair<std::string, std::string>> const& flags);
		static bool RegisterGS(char const* scriptName, CreateFn);


	private:
		static std::unique_ptr<std::map<std::string, CreateFn>> s_gsCreateFunctions;
		static std::unique_ptr<std::mutex> s_gsCreateFunctionsMutex;
		

	private:
		template<typename T>
		friend class IScriptableGraphicsSystem; // Required to access GraphicsSystem::Create()
	};


	template<typename T>
	std::unique_ptr<gr::GraphicsSystem> GraphicsSystem::Create(
		gr::GraphicsSystemManager* gsm,
		std::vector<std::pair<std::string, std::string>> const& flags)
	{
		std::unique_ptr<gr::GraphicsSystem> newGS;
		newGS.reset(new T(gsm));

		// Process the flags:
		newGS->RegisterFlags();

		for (auto const& flag : flags)
		{
			SEAssert(newGS->m_flags.contains(util::CHashKey::Create(flag.first)), "Flag not registered");
			newGS->m_flags.at(util::CHashKey::Create(flag.first)) = util::CHashKey::Create(flag.second);
		}

		// Register our inputs immediately. Outputs are registered once the initialization step(s) have run
		newGS->RegisterInputs();

		return newGS;
	}


	inline bool GraphicsSystem::HasFlagValue(util::CHashKey const& flagName, util::CHashKey const& valueName) const
	{
		SEAssert(m_flags.contains(flagName), "Flag with that name has not been registered");

		return m_flags.at(flagName) == valueName;
	}


	inline util::CHashKey const& GraphicsSystem::GetFlagValue(util::CHashKey const& flagName) const
	{
		SEAssert(m_flags.contains(flagName), "Flag with that name has not been registered");

		return m_flags.at(flagName);
	}


	inline void GraphicsSystem::RegisterFlag(util::CHashKey const& flagName)
	{
		m_flags.emplace(flagName, util::CHashKey("<Unset>"));
	}


	inline GraphicsSystem::TextureInputDefault GraphicsSystem::GetTextureInputDefaultType(
		util::CHashKey const& inputScriptName) const
	{
		SEAssert(HasRegisteredTextureInput(inputScriptName), "Texture input with that name has not been registered");
		return m_textureInputs.at(inputScriptName);
	}


	inline GraphicsSystem::TextureInputDefault GraphicsSystem::GetTextureInputDefaultType(
		char const* inputScriptName) const
	{
		return GetTextureInputDefaultType(util::CHashKey::Create(inputScriptName));
	}


	inline GraphicsSystem::TextureInputDefault GraphicsSystem::GetTextureInputDefaultType(
		std::string const& inputScriptName) const
	{
		return GetTextureInputDefaultType(inputScriptName.c_str());
	}


	inline bool GraphicsSystem::HasRegisteredTextureInput(util::CHashKey const& inputScriptName) const
	{
		return m_textureInputs.contains(inputScriptName);
	}


	inline bool GraphicsSystem::HasRegisteredTextureInput(char const* inputScriptName) const
	{
		return HasRegisteredTextureInput(util::CHashKey::Create(inputScriptName));
	}


	inline bool GraphicsSystem::HasRegisteredTextureInput(std::string const& inputScriptName) const
	{
		return HasRegisteredTextureInput(inputScriptName.c_str());
	}


	inline std::map<util::CHashKey, GraphicsSystem::TextureInputDefault> const& GraphicsSystem::GetTextureInputs() const
	{
		return m_textureInputs;
	}


	inline core::InvPtr<re::Texture> const* GraphicsSystem::GetTextureOutput(util::CHashKey const& scriptName) const
	{
		// Note: It's possible for GS's with multiple initialization steps to hit this if its first initialization step
		// runs before a GS it is dependent on has been initialized (because we (currently) just initialize in the order
		// the initialization steps are defined in)
		SEAssert(m_textureOutputs.contains(scriptName),
			"No texture output with the given script name was registered by the GS");
		return m_textureOutputs.at(scriptName);
	}


	inline core::InvPtr<re::Texture> const* GraphicsSystem::GetTextureOutput(char const* scriptName) const
	{
		return GetTextureOutput(util::CHashKey::Create(scriptName));
	}


	inline core::InvPtr<re::Texture> const* GraphicsSystem::GetTextureOutput(std::string const& scriptName) const
	{
		return GetTextureOutput(scriptName.c_str());
	}


	inline void GraphicsSystem::RegisterTextureInput(
		util::CHashKey const& scriptName, TextureInputDefault fallbackDefault /*= TextureInputDefault::None*/)
	{
		// This function might be called multiple times by the same GS (e.g. if it has multiple initialization steps)
		SEAssert(!m_textureInputs.contains(scriptName) || m_textureInputs.at(scriptName) == fallbackDefault,
			"Texture dependency has already been registered and populated");
		m_textureInputs.emplace(scriptName, fallbackDefault);
	}


	inline void GraphicsSystem::RegisterTextureOutput(
		util::CHashKey const& scriptName, core::InvPtr<re::Texture> const* outputTex)
	{
		// Note: It's possible for the texture to be null here (e.g. for GS's with multiple initialization steps). This
		// is fine long as everything exists the last time a GS calls this function.
		m_textureOutputs[scriptName] = outputTex;
	}


	inline bool GraphicsSystem::HasRegisteredBufferInput(util::CHashKey const& scriptName) const
	{
		return m_bufferInputs.contains(scriptName);
	}


	inline bool GraphicsSystem::HasRegisteredBufferInput(char const* scriptName) const
	{
		return HasRegisteredBufferInput(util::CHashKey::Create(scriptName));
	}


	inline bool GraphicsSystem::HasRegisteredBufferInput(std::string const& scriptName) const
	{
		return HasRegisteredBufferInput(scriptName.c_str());
	}


	inline std::set<util::CHashKey> const& GraphicsSystem::GetBufferInputs() const
	{
		return m_bufferInputs;
	}


	inline std::shared_ptr<re::Buffer> const* GraphicsSystem::GetBufferOutput(util::CHashKey const& scriptName) const
	{
		SEAssert(m_bufferOutputs.contains(scriptName),
			"No Buffer output with the given script name was registered by the GS");
		return m_bufferOutputs.at(scriptName);
	}


	inline std::shared_ptr<re::Buffer> const* GraphicsSystem::GetBufferOutput(char const* scriptName) const
	{
		return GetBufferOutput(util::CHashKey::Create(scriptName));
	}


	inline std::shared_ptr<re::Buffer> const* GraphicsSystem::GetBufferOutput(std::string const& scriptName) const
	{
		return GetBufferOutput(scriptName.c_str());
	}


	inline void GraphicsSystem::RegisterBufferInput(util::CHashKey const& scriptName)
	{
		m_bufferInputs.emplace(scriptName);
	}


	inline void GraphicsSystem::RegisterBufferOutput(
		util::CHashKey const& scriptName, std::shared_ptr<re::Buffer> const* buffer)
	{
		m_bufferOutputs.emplace(scriptName, buffer);
	}


	inline bool GraphicsSystem::HasRegisteredDataInput(util::CHashKey const& scriptName) const
	{
		return m_dataInputs.contains(scriptName);
	}


	inline bool GraphicsSystem::HasRegisteredDataInput(char const* scriptName) const
	{
		return HasRegisteredDataInput(util::CHashKey::Create(scriptName));
	}


	inline bool GraphicsSystem::HasRegisteredDataInput(std::string const& scriptName) const
	{
		return HasRegisteredDataInput(scriptName.c_str());
	}


	inline std::set<util::CHashKey> const& GraphicsSystem::GetDataInputs() const
	{
		return m_dataInputs;
	}


	inline void const* GraphicsSystem::GetDataOutput(util::CHashKey const& scriptName) const
	{
		SEAssert(m_dataOutputs.contains(scriptName), "No data output with the given name has been registered");
		return m_dataOutputs.at(scriptName);
	}


	inline void const* GraphicsSystem::GetDataOutput(char const* scriptName) const
	{
		return GetDataOutput(util::CHashKey::Create(scriptName));
	}


	inline void const* GraphicsSystem::GetDataOutput(std::string const& scriptName) const
	{
		return GetDataOutput(scriptName.c_str());
	}


	inline void GraphicsSystem::RegisterDataInput(util::CHashKey const& scriptName)
	{
		m_dataInputs.emplace(scriptName);
	}


	inline void GraphicsSystem::RegisterDataOutput(util::CHashKey const& scriptName, void const* data)
	{
		m_dataOutputs.emplace(scriptName, data);
	}


	inline void GraphicsSystem::PostEvent(gr::GraphicsEvent&& newEvent)
	{
		{
			std::unique_lock writeLock(m_eventsMutex);

			m_events.emplace(std::forward<gr::GraphicsEvent>(newEvent));
		}
	}


	inline bool GraphicsSystem::HasEvents() const
	{
		{
			std::shared_lock readLock(m_eventsMutex);
			return m_events.empty() == false;
		}
	}


	inline gr::GraphicsEvent GraphicsSystem::GetEvent()
	{
		{
			std::unique_lock writeLock(m_eventsMutex);

			SEAssert(!m_events.empty(), "No events to get");
			const gr::GraphicsEvent nextEvent = m_events.front();
			m_events.pop();
			return nextEvent;
		}
	}


	// ---



	// Scriptable rendering pipeline self-registration interface
	// By inheriting from this interface, a GraphicsSystem will automatically be able to be created by name
	template<typename T>
	class IScriptableGraphicsSystem
	{
	public:
		// Use a static_cast to prevent the compiler from optimizing our static registration variable away
		IScriptableGraphicsSystem() { static_cast<void*>(&s_isRegistered); }

	private:
		static bool s_isRegistered;
	};


	template<typename T>
	bool IScriptableGraphicsSystem<T>::s_isRegistered =
		GraphicsSystem::RegisterGS(T::GetScriptName(), gr::GraphicsSystem::Create<T>);
}


// Helper macros: Cut down on the boilerplate required to build maps of runtime functions callable by name

#define RETURN_RUNTIME_BINDINGS(...) \
	return gr::GraphicsSystem::RuntimeBindings{__VA_ARGS__};

#define INIT_PIPELINE(...) \
	.m_initPipelineFunctions = {__VA_ARGS__},

#define PRE_RENDER(...) \
	.m_preRenderFunctions = {__VA_ARGS__},

#define INIT_PIPELINE_FN(gsClassName, memberFuncName) \
	{util::ToLower(#memberFuncName), std::bind(&gsClassName::memberFuncName, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)}

#define PRE_RENDER_FN(gsClassName, memberFuncName) \
	{util::ToLower(#memberFuncName), std::bind(&gsClassName::memberFuncName, this)},