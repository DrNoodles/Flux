#include "FileService.h"

#include <tinyfiledialogs/tinyfiledialogs.h>

#include <fstream>
#include <iostream>


std::string FileService::TexturePicker()
{
	return FilePicker("Pick Texture", { "*.png", "*.tga", "*.jpg" }, ".png .tga .jpg");
}


std::string FileService::ModelPicker()
{
	return FilePicker("Pick Model", { "*.obj", "*.fbx", "*.gltf", "*.glb" }, ".obj .fbx .gltf .glb");
}


std::string FileService::FilePicker(const std::string& title, const std::vector<std::string>& filterPatterns,
	const std::string& filterDescription)
{
	// Transform to C style string
	auto cFilterPatterns = std::vector<const char*>(filterPatterns.size());
	for (auto i = 0; i < filterPatterns.size(); ++i)
	{
		cFilterPatterns[i] = filterPatterns[i].c_str();
	}

	const int isMultiSelect = 0;
	const auto* path = tinyfd_openFileDialog(title.c_str(), nullptr, (int)filterPatterns.size(), cFilterPatterns.data(),
		filterDescription.c_str(), isMultiSelect);
	if (path == nullptr)
	{
		std::cout << "FilePicker() - Nothing selected\n";
		return "";
	}

	std::cout << "FilePicker() - Loading: " << path << std::endl;

	// Take first half to dir and second to file

	return std::string{ path };
}


std::vector<char> FileService::ReadFile(const std::string& path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary); // ate starts reading from eof

	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file " + path);
	}

	const size_t filesize = (size_t)file.tellg();
	std::vector<char> buffer(filesize);
	file.seekg(0);
	file.read(buffer.data(), filesize);
	file.close();

	return buffer;
}

std::tuple<std::string, std::string> FileService::SplitPathAsDirAndFilename(const std::string& path)
{
	// TODO safety check this actually exists :)
	const size_t idx = path.find_last_of("/\\");
	const auto directory = path.substr(0, idx + 1);
	const auto filename = path.substr(idx + 1);
	
	return { directory, filename };
}
