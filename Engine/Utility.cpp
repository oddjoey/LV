#include "Utility.h"

namespace Utility
{
	std::vector<char> ReadFile(const std::string& filepath)
	{
		std::ifstream file(filepath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) 
			throw std::runtime_error("Failed to open file!");

		std::vector<char> buffer(file.tellg());

		file.seekg(0);
		file.read(buffer.data(), buffer.size());
		file.close();

		return buffer;
	}
}