#pragma once
#include "Renderer/LowLevel/VulkanHelpers.h"

#include <Framework/CommonTypes.h>

#include <stbi/stb_image.h>

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <iostream>

using vkh = VulkanHelpers;



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Texels
{
public:
	virtual ~Texels() = default;

	inline u32 Width() const { return _width; }
	inline u32 Height() const { return _height; }
	inline u8 Channels() const { return _channels; }
	//inline u8 BytesPerPixel() const { return _bytesPerPixel; }
	//inline u8 BytesPerChannel() const { return _bytesPerChannel; }
	inline size_t DataSize() const { return _dataSize; }
	inline const std::vector<u8>& Data() const { return _data; }

	void Load(const std::string& path)
	{
		_data = LoadType(path, _dataSize, _width, _height, _channels/*, _bytesPerChannel*/);

		//_mipLevels = (u8)std::floor(std::log2(std::max(_width, _height))) + 1;
		//_bytesPerPixel = _bytesPerChannel * _channels;
	}

	inline static u8 CalcMipLevels(u32 width, u32 height) { return (u8)std::floor(std::log2(std::max(width, height))) + 1; }

protected:
	virtual std::vector<u8> LoadType(const std::string& path,
		size_t& outDataSize,
		u32& outWidth,
		u32& outHeight,
		u8& outChannels/*,
		u8& outBytesPerChannel*/) = 0;

private:
	// Data
	u32 _width{};
	u32 _height{};
	u8 _channels{};
	//u8 _bytesPerPixel = {};
	//u8 _bytesPerChannel = {};

	std::vector<u8> _data = {};
	size_t _dataSize = {}; // The total size of the buffer

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TexelsRgbaF32 final : public Texels
{
protected:
	std::vector<u8> LoadType(const std::string& path,
		size_t& outDataSize,
		u32& outWidth,
		u32& outHeight,
		u8& outChannels/*,
		u8& outBytesPerChannel*/) override
	{
		int width, height, channelsInImage;
		const auto desiredChannels = (u8)STBI_rgb_alpha;

		const auto bytesPerChannel = sizeof(f32);
		f32* pData = stbi_loadf(path.c_str(), &width, &height, &channelsInImage, desiredChannels);

		if (!pData)
		{
			stbi_image_free(pData);
			throw std::runtime_error("Failed to load texture image: " + path);
		}

		outWidth = (u32)width;
		outHeight = (u32)height;
		outChannels = (u8)desiredChannels;
		outDataSize = (size_t)width * (size_t)height * outChannels * bytesPerChannel;

		auto data = std::vector<u8>{ (u8*)pData, (u8*)pData + outDataSize };

		stbi_image_free(pData);

		return data;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TexelsRgbaU8 final : public Texels
{
protected:
	std::vector<u8> LoadType(const std::string& path,
		size_t& outDataSize,
		u32& outWidth,
		u32& outHeight,
		u8& outChannels/*,
		u8& outBytesPerChannel*/) override
	{
		int width, height, channelsInImage;
		const auto desiredChannels = (u8)STBI_rgb_alpha;

		const auto bytesPerChannel = sizeof(u8);
		u8* pData = stbi_load(path.c_str(), &width, &height, &channelsInImage, desiredChannels);

		if (!pData)
		{
			stbi_image_free(pData);
			throw std::runtime_error("Failed to load texture image: " + path);
		}

		outWidth = (u32)width;
		outHeight = (u32)height;
		outChannels = (u8)desiredChannels;
		outDataSize = (size_t)width * (size_t)height * outChannels * bytesPerChannel;

		auto data = std::vector<u8>{ (u8*)pData, (u8*)pData + outDataSize };

		stbi_image_free(pData);

		return data;
	}
};
