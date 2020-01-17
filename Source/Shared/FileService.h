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


	// TODO write a method that splits the path into dir and filename
	
	//// TODO safety check this actually exists :)
	//const size_t idx = path.find_last_of("/\\");
	//const auto directory = path.substr(0, idx + 1);
	//const auto filename = path.substr(idx + 1);
};
