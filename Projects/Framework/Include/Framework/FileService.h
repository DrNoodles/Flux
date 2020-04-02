#pragma once

#include <vector>
#include <string>

class FileService final
{
public:
	static std::string TexturePicker();
	static std::string ModelPicker();
	static std::string FilePicker(const std::string& title, const std::vector<std::string>& filterPatterns,
		const std::string& filterDescription);
	static std::vector<char> ReadFile(const std::string& path);
	static std::tuple<std::string, std::string> SplitPathAsDirAndFilename(const std::string& path);
};
