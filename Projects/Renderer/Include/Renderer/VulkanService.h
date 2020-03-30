#pragma once

#include "GpuTypes.h"
#include "VulkanHelpers.h"
#include "VulkanInitializers.h"

#include <Framework/CommonTypes.h>

#include <vulkan/vulkan.h>

class VulkanService
{
public:
	VulkanService(bool enableValidationLayers)
	{
		_enableValidationLayers = enableValidationLayers;
		//InitVulkan();
	}

	void CleanUp()
	{
		
	}
	
	bool _enableValidationLayers = false;
private:

};
