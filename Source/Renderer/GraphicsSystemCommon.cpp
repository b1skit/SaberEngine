// © 2024 Adam Badke. All rights reserved.
#include "GraphicsSystemCommon.h"

#include "Core/Assert.h"


namespace gr
{
	uint32_t GetLightDataBufferIdx(LightDataBufferIdxMap const& lightDataBufferIdxMap, gr::RenderDataID lightID)
	{
		SEAssert(lightDataBufferIdxMap.contains(lightID), "Light ID not found, was the light registered?");
		return lightDataBufferIdxMap.at(lightID);
	}
}