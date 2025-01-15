// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Presentation/Load_Common.h"


namespace core
{
	class Inventory;
}

namespace load
{
	void ImportGLTFFile(core::Inventory* inventory, std::string const& filePath);

	void GenerateDefaultGLTFMaterial(core::Inventory* inventory);
}