#pragma once

#include "SceneData.h"


namespace platform
{
	class SceneData
	{
	public:
		static void (*PostProcessLoadedData)(fr::SceneData&);
	};
}