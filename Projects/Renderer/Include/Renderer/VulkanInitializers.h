#pragma once

#include <Framework/CommonTypes.h>
#include "Framebuffer.h"
#include <vulkan/vulkan.h>


// Vulkan initialisation helpers
namespace vki
{
	inline VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet, u32 dstBinding, VkDescriptorType descriptorType, u32 descriptorCount, u32 dstArrayElement, const VkDescriptorImageInfo* pImageInfo = nullptr, const VkDescriptorBufferInfo* pBufferInfo = nullptr, const VkBufferView* pTexelBufferView = nullptr)
	{
		VkWriteDescriptorSet x = {};
		x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		x.pNext = nullptr;
		x.dstSet = dstSet;
		x.dstBinding = dstBinding; // correlates to shader binding
		x.dstArrayElement = dstArrayElement;
		x.descriptorType = descriptorType;
		x.descriptorCount = descriptorCount;
		x.pBufferInfo = pBufferInfo; // descriptor is one of buffer, image or texelbufferview
		x.pImageInfo = pImageInfo;
		x.pTexelBufferView = pTexelBufferView;
		return x;
	}

	inline VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(u32 binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, u32 descriptorCount = 1, const VkSampler* pImmutableSamplers = nullptr)
	{
		VkDescriptorSetLayoutBinding x = {};
		x.binding = binding; // correlates to shader
		x.descriptorType = descriptorType;
		x.descriptorCount = descriptorCount;
		x.stageFlags = stageFlags;
		x.pImmutableSamplers = pImmutableSamplers;
		return x;
	}

	inline VkViewport Viewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth = 0, f32 maxDepth = 1)
	{
		VkViewport viewport = {};
		viewport.x = x;
		viewport.y = y;
		viewport.width = width;
		viewport.height = height;
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		return viewport;
	}

	inline VkViewport Viewport(const VkOffset2D& offset, const VkExtent2D& extent, f32 minDepth = 0, f32 maxDepth = 1)
	{
		VkViewport viewport = {};
		viewport.x = (f32)offset.x;
		viewport.y = (f32)offset.y;
		viewport.width = (f32)extent.width;
		viewport.height = (f32)extent.height;
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		return viewport;
	}

	inline VkViewport Viewport(const VkRect2D& rect, f32 minDepth = 0, f32 maxDepth = 1)
	{
		VkViewport viewport = {};
		viewport.x = (f32)rect.offset.x;
		viewport.y = (f32)rect.offset.y;
		viewport.width = (f32)rect.extent.width;
		viewport.height = (f32)rect.extent.height;
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		return viewport;
	}

	inline VkRect2D Rect2D(const VkOffset2D& offset, const VkExtent2D& extent)
	{
		VkRect2D r = {};
		r.offset = offset;
		r.extent = extent;
		return r;
	}
	inline VkRect2D Rect2D(i32 x, i32 y, u32 width, u32 height)
	{
		VkRect2D r = {};
		r.offset.x = x;
		r.offset.y = y;
		r.extent.width = width;
		r.extent.height = height;
		return r;
	}

	inline VkOffset2D Offset2D(i32 x, i32 y)
	{
		VkOffset2D o = {};
		o.x = x;
		o.y = y;
		return o;
	}

	inline VkExtent2D Extent2D(u32 width, u32 height)
	{
		VkExtent2D x = {};
		x.width = width;
		x.height = height;
		return x;
	}

	inline VkOffset3D Offset3D(i32 x, i32 y, i32 z)
	{
		VkOffset3D o = {};
		o.x = x;
		o.y = y;
		o.z = z;
		return o;
	}

	inline VkExtent3D Extent3D(u32 width, u32 height, u32 depth)
	{
		VkExtent3D x = {};
		x.width = width;
		x.height = height;
		x.depth = depth;
		return x;
	}
	
	/**
	 VkRenderPassBeginInfo - Structure specifying render pass begin info
	 @param renderPass is the render pass to begin an instance of.
	 @param framebuffer is the framebuffer containing the attachments that are used with the render pass.
	 @param renderArea is the render area that is affected by the render pass instance, and is described in more detail below.
	 @param clearValues is a vector of VkClearValue structures that contains clear values for each attachment, if the attachment uses a loadOp value of VK_ATTACHMENT_LOAD_OP_CLEAR or if the attachment has a depth/stencil format and uses a stencilLoadOp value of VK_ATTACHMENT_LOAD_OP_CLEAR. The array is indexed by attachment number. Only elements corresponding to cleared attachments are used. Other elements of pClearValues are ignored.
	*/
	inline VkRenderPassBeginInfo RenderPassBeginInfo(VkRenderPass renderPass, VkFramebuffer framebuffer, VkRect2D renderArea, const std::vector<VkClearValue>& clearValues)
	{
		VkRenderPassBeginInfo x = {};
		x.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		x.pNext = nullptr;
		x.renderPass = renderPass;
		x.framebuffer = framebuffer;
		x.renderArea = renderArea;
		x.clearValueCount = (u32)clearValues.size();
		x.pClearValues = clearValues.data();
		return x;
	}

	inline VkRenderPassBeginInfo RenderPassBeginInfo(const FramebufferResources& framebuffer, VkRect2D renderArea)
	{
		VkRenderPassBeginInfo x = {};
		x.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		x.pNext = nullptr;
		x.renderPass = framebuffer.Desc.RenderPass;
		x.framebuffer = framebuffer.Framebuffer;
		x.renderArea = renderArea;
		x.clearValueCount = (u32)framebuffer.Desc.ClearValues.size();
		x.pClearValues = framebuffer.Desc.ClearValues.data();
		return x;
	}
	
	/**
	 VkImageSubresourceRange - Structure specifying an image subresource range
	 @param aspectMask is a bitmask of VkImageAspectFlagBits specifying which aspect(s) of the image are included in the view.
	 @param baseMipLevel is the first mipmap level accessible to the view.
	 @param levelCount is the number of mipmap levels (starting from baseMipLevel) accessible to the view.
	 @param baseArrayLayer is the first array layer accessible to the view.
	 @param layerCount is the number of array layers (starting from baseArrayLayer) accessible to the view.
	**/
	inline VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlagBits aspectMask, u32 baseMipLevel, u32 levelCount, u32 baseArrayLayer, u32 layerCount)
	{
		VkImageSubresourceRange x = {};
		x.aspectMask = aspectMask;
		x.baseMipLevel = baseMipLevel;
		x.levelCount = levelCount;
		x.baseArrayLayer = baseArrayLayer;
		x.layerCount = layerCount;
		return x;
	}

	/**
	 VkImageSubresourceLayers - Structure specifying an image subresource layers
    @param aspectMask is a combination of VkImageAspectFlagBits, selecting the color, depth and/or stencil aspects to be copied.
    @param mipLevel is the mipmap level to copy from.
    @param baseArrayLayer and layerCount are the starting layer and number of layers to copy.
	 @param layerCount is the number of array layers (starting from baseArrayLayer) accessible to the view.
	**/
	inline VkImageSubresourceLayers ImageSubresourceLayers(VkImageAspectFlagBits aspectMask, u32 mipLevel, u32 baseArrayLayer, u32 layerCount)
	{
		VkImageSubresourceLayers x = {};
		x.aspectMask = aspectMask;
		x.mipLevel = mipLevel;
		x.baseArrayLayer = baseArrayLayer;
		x.layerCount = layerCount;
		return x;
	}

	/**
	 VkCommandBufferBeginInfo - Structure specifying a command buffer begin operation
	 @param flags is a bitmask of VkCommandBufferUsageFlagBits specifying usage behavior for the command buffer.
	 @param pInheritanceInfo is a pointer to a VkCommandBufferInheritanceInfo structure, used if commandBuffer is a secondary command buffer. If this is a primary command buffer, then this value is ignored.
	 */
	inline VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags, const VkCommandBufferInheritanceInfo* pInheritanceInfo)
	{
		VkCommandBufferBeginInfo x = {};
		x.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		x.pNext = nullptr;
		x.flags = flags;
		x.pInheritanceInfo = pInheritanceInfo;
		return x;
	}

	/**
	 VkBufferCopy - Structure specifying a buffer copy operation
    @param srcOffset is the starting offset in bytes from the start of srcBuffer.
    @param dstOffset is the starting offset in bytes from the start of dstBuffer.
    @param size is the number of bytes to copy.
	 */
	inline VkBufferCopy BufferCopy(VkDeviceSize dstOffset, u64 size, u64 srcOffset)
	{
		VkBufferCopy x = {};
		x.srcOffset = srcOffset;
		x.dstOffset = dstOffset;
		x.size = size;
		return x;
	}

	/**
	 VkBufferImageCopy - Structure specifying a buffer image copy operation
    @param bufferOffset is the offset in bytes from the start of the buffer object where the image data is copied from or to.
	 @param bufferRowLength and bufferImageHeight specify in texels a subregion of a larger two- or three-dimensional image in buffer memory, and control the addressing calculations. If either of these values is zero, that aspect of the buffer memory is considered to be tightly packed according to the imageExtent.
	 @param bufferImageHeight and bufferRowLength specify in texels a subregion of a larger two- or three-dimensional image in buffer memory, and control the addressing calculations. If either of these values is zero, that aspect of the buffer memory is considered to be tightly packed according to the imageExtent.
    @param imageSubresource is a VkImageSubresourceLayers used to specify the specific image subresources of the image used for the source or destination image data.
    @param imageOffset selects the initial x, y, z offsets in texels of the sub-region of the source or destination image data.
    @param imageExtent is the size in texels of the image to copy in width, height and depth.
	*/
	inline VkBufferImageCopy BufferImageCopy(u64 bufferOffset, u32 bufferRowLength, u32 bufferImageHeight, VkImageSubresourceLayers imageSubresource, VkOffset3D imageOffset, VkExtent3D imageExtent)
	{
		VkBufferImageCopy x = {};
		x.bufferOffset = bufferOffset;
		x.bufferRowLength = bufferRowLength;
		x.bufferImageHeight = bufferImageHeight;
		x.imageSubresource = imageSubresource;
		x.imageOffset = imageOffset;
		x.imageExtent = imageExtent;
		return x;
	}

}
