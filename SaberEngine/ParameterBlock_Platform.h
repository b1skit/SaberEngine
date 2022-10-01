#pragma once


namespace re
{
	class PermanentParameterBlock;
}


namespace platform
{
	class PermanentParameterBlock
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

			static void CreatePlatformParams(re::PermanentParameterBlock& paramBlock);
		};

	public:
		static void (*Create)(re::PermanentParameterBlock&);
		static void (*Destroy)(re::PermanentParameterBlock&);

	private:

	};

	// We need to provide a destructor implementation since it's pure virtual
	inline platform::PermanentParameterBlock::PlatformParams::~PlatformParams() {};
}