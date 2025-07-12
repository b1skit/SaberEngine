// © 2025 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	class Inventory;
}

namespace load
{
	void ImportGLTFFile(std::string const& filePath);

	void GenerateDefaultGLTFMaterial();
}