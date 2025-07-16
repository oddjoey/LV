#pragma once

#include <fstream>
#include <vector>
#include <string>

namespace Utility
{
	// Function to read the contents of a file into a string
	std::vector<char> ReadFile(const std::string& filepath);
}