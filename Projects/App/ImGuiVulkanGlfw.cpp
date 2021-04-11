
#include "ImGuiVulkanGlfw.h"
#include <Renderer/LowLevel/VulkanService.h>

ImGuiVulkanGlfw::ImGuiVulkanGlfw(GLFWwindow* window, VulkanService* vk)
	: _vk(vk)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();


	// UI Style
	{
		// Colors
		{
			ImGui::StyleColorsDark();
			auto& colors = ImGui::GetStyle().Colors;
			colors[ImGuiCol_WindowBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
			colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);

			colors[ImGuiCol_Button] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);

			colors[ImGuiCol_Border] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
			colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
			colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);

			colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 0.75f);
			colors[ImGuiCol_Header] = ImVec4(0.24f, 0.52f, 0.88f, 0.75f);

			colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.52f, 0.88f, 0.25f);
			colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.52f, 0.88f, 0.50f);
		}

		// Form
		{
			const float rounding = 3;
			ImGuiStyle& style = ImGui::GetStyle();
			//Main
			style.WindowPadding = {4, 4};
			style.FramePadding = {6, 3};
			style.ItemSpacing = {4, 3};
			style.ItemInnerSpacing = {4, 4};
			style.IndentSpacing = 21;
			style.ScrollbarSize = 9;
			style.GrabMinSize = 10;
			//Borders
			style.WindowBorderSize = 0;
			style.ChildBorderSize = 1;
			style.PopupBorderSize = 1;
			style.FrameBorderSize = 1;
			style.TabBorderSize = 0;
			//Rounding
			style.WindowRounding = 0;
			style.ChildRounding = rounding;
			style.FrameRounding = rounding;
			style.GrabRounding = 2;
		}
	}

	// Here Imgui is coupled to Glfw and Vulkan
	ImGui_ImplGlfw_InitForVulkan(window, true);


	const auto imageCount = vk->GetSwapchain().GetImageCount();

	// Create descriptor pool - from main_vulkan.cpp imgui example code
	_imguiDescriptorPool = vkh::CreateDescriptorPool({
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
		}, imageCount, vk->LogicalDevice());


	// Get the min image count
	u32 minImageCount;
	{
		// Copied from VulkanHelpers::CreateSwapchain - TODO either store minImageCount or make it a separate func
		const SwapChainSupportDetails deets = vkh::QuerySwapChainSupport(vk->PhysicalDevice(), vk->Surface());

		minImageCount = deets.Capabilities.minImageCount + 1; // 1 extra image to avoid waiting on driver
		const auto maxImageCount = deets.Capabilities.maxImageCount;
		const auto maxImageCountExists = maxImageCount != 0;
		if (maxImageCountExists && minImageCount > maxImageCount)
		{
			minImageCount = maxImageCount;
		}
	}


	// Init device info
	ImGui_ImplVulkan_InitInfo initInfo = {};
	{
		initInfo.Instance = vk->Instance();
		initInfo.PhysicalDevice = vk->PhysicalDevice();
		initInfo.Device = vk->LogicalDevice();
		initInfo.QueueFamily = vkh::FindQueueFamilies(vk->PhysicalDevice(), vk->Surface()).GraphicsAndComputeFamily.value();
		// vomit
		initInfo.Queue = vk->GraphicsQueue();
		initInfo.PipelineCache = nullptr;
		initInfo.DescriptorPool = _imguiDescriptorPool;
		initInfo.MinImageCount = minImageCount;
		initInfo.ImageCount = imageCount;
		initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT; // Swapchain renderpass is not MSAA - this will age well :P
		initInfo.Allocator = nullptr;
		initInfo.CheckVkResultFn = [](VkResult err)
		{
			if (err == VK_SUCCESS) return;
			printf("VkResult %d\n", err);
			if (err < 0)
				abort();
		};
	}

	ImGui_ImplVulkan_Init(&initInfo, vk->GetSwapchain().GetRenderPass());


	// Upload Fonts
	{
		auto* const cmdBuf = vkh::BeginSingleTimeCommands(vk->CommandPool(), vk->LogicalDevice());
		ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);
		vkh::EndSingeTimeCommands(cmdBuf, vk->CommandPool(), vk->GraphicsQueue(), vk->LogicalDevice());
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}
}

ImGuiVulkanGlfw::~ImGuiVulkanGlfw()
{
	vkDestroyDescriptorPool(_vk->LogicalDevice(), _imguiDescriptorPool, nullptr);
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}