#pragma once


namespace re
{
	class ParameterBlock;
}


namespace platform
{
	class ParameterBlock
	{
	public:
		struct PlatformParams
		{
			// Params contain unique GPU bindings that should not be arbitrarily copied/duplicated
			PlatformParams() = default;
			PlatformParams(PlatformParams&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams&) = delete;
			PlatformParams& operator=(PlatformParams&&) = delete;

			// API-specific GPU bindings should be destroyed here
			virtual ~PlatformParams() = 0;

			static void CreatePlatformParams(re::ParameterBlock& paramBlock);
		};

	public:
		static void (*Create)(re::ParameterBlock&);
		static void (*Update)(re::ParameterBlock&);
		static void (*Destroy)(re::ParameterBlock&);

	private:

	};

	// We need to provide a destructor implementation since it's pure virtual
	inline platform::ParameterBlock::PlatformParams::~PlatformParams() {};
}