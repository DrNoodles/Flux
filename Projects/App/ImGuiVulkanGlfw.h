#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

class VulkanService;

class ImGuiVulkanGlfw
{
private:
	VulkanService* _vk = nullptr;
	VkDescriptorPool _imguiDescriptorPool = nullptr;

public:
	ImGuiVulkanGlfw(GLFWwindow* window, VulkanService* vk);
	~ImGuiVulkanGlfw();
	
	ImGuiVulkanGlfw() = delete;
	ImGuiVulkanGlfw(const ImGuiVulkanGlfw&) = delete;
	ImGuiVulkanGlfw& operator=(const ImGuiVulkanGlfw&) = delete;
	ImGuiVulkanGlfw(ImGuiVulkanGlfw&&) = delete;
	ImGuiVulkanGlfw& operator=(ImGuiVulkanGlfw&&) = delete;
};

