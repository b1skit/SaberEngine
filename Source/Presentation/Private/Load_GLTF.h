// © 2025 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	class Inventory;
}

namespace load
{
	void ImportGLTFFile(core::Inventory*, std::string const& filePath);

	void GenerateDefaultGLTFMaterial(core::Inventory*);
}