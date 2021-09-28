
if (!(ptrs_.vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(getInstanceProcAddr(instance_, "vkAllocateCommandBuffers")))) {
  return Result("Vulkan: Unable to load vkAllocateCommandBuffers pointer");
}

if (!(ptrs_.vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(getInstanceProcAddr(instance_, "vkAllocateDescriptorSets")))) {
  return Result("Vulkan: Unable to load vkAllocateDescriptorSets pointer");
}

if (!(ptrs_.vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(getInstanceProcAddr(instance_, "vkAllocateMemory")))) {
  return Result("Vulkan: Unable to load vkAllocateMemory pointer");
}

if (!(ptrs_.vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(getInstanceProcAddr(instance_, "vkBeginCommandBuffer")))) {
  return Result("Vulkan: Unable to load vkBeginCommandBuffer pointer");
}

if (!(ptrs_.vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(getInstanceProcAddr(instance_, "vkBindBufferMemory")))) {
  return Result("Vulkan: Unable to load vkBindBufferMemory pointer");
}

if (!(ptrs_.vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(getInstanceProcAddr(instance_, "vkBindImageMemory")))) {
  return Result("Vulkan: Unable to load vkBindImageMemory pointer");
}

if (!(ptrs_.vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(getInstanceProcAddr(instance_, "vkCmdBeginRenderPass")))) {
  return Result("Vulkan: Unable to load vkCmdBeginRenderPass pointer");
}

if (!(ptrs_.vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(getInstanceProcAddr(instance_, "vkCmdBindDescriptorSets")))) {
  return Result("Vulkan: Unable to load vkCmdBindDescriptorSets pointer");
}

if (!(ptrs_.vkCmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(getInstanceProcAddr(instance_, "vkCmdBindIndexBuffer")))) {
  return Result("Vulkan: Unable to load vkCmdBindIndexBuffer pointer");
}

if (!(ptrs_.vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(getInstanceProcAddr(instance_, "vkCmdBindPipeline")))) {
  return Result("Vulkan: Unable to load vkCmdBindPipeline pointer");
}

if (!(ptrs_.vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(getInstanceProcAddr(instance_, "vkCmdBindVertexBuffers")))) {
  return Result("Vulkan: Unable to load vkCmdBindVertexBuffers pointer");
}

if (!(ptrs_.vkCmdClearAttachments = reinterpret_cast<PFN_vkCmdClearAttachments>(getInstanceProcAddr(instance_, "vkCmdClearAttachments")))) {
  return Result("Vulkan: Unable to load vkCmdClearAttachments pointer");
}

if (!(ptrs_.vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(getInstanceProcAddr(instance_, "vkCmdCopyBuffer")))) {
  return Result("Vulkan: Unable to load vkCmdCopyBuffer pointer");
}

if (!(ptrs_.vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(getInstanceProcAddr(instance_, "vkCmdCopyBufferToImage")))) {
  return Result("Vulkan: Unable to load vkCmdCopyBufferToImage pointer");
}

if (!(ptrs_.vkCmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(getInstanceProcAddr(instance_, "vkCmdCopyImageToBuffer")))) {
  return Result("Vulkan: Unable to load vkCmdCopyImageToBuffer pointer");
}

if (!(ptrs_.vkCmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(getInstanceProcAddr(instance_, "vkCmdDispatch")))) {
  return Result("Vulkan: Unable to load vkCmdDispatch pointer");
}

if (!(ptrs_.vkCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(getInstanceProcAddr(instance_, "vkCmdDraw")))) {
  return Result("Vulkan: Unable to load vkCmdDraw pointer");
}

if (!(ptrs_.vkCmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(getInstanceProcAddr(instance_, "vkCmdDrawIndexed")))) {
  return Result("Vulkan: Unable to load vkCmdDrawIndexed pointer");
}

if (!(ptrs_.vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(getInstanceProcAddr(instance_, "vkCmdEndRenderPass")))) {
  return Result("Vulkan: Unable to load vkCmdEndRenderPass pointer");
}

if (!(ptrs_.vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(getInstanceProcAddr(instance_, "vkCmdPipelineBarrier")))) {
  return Result("Vulkan: Unable to load vkCmdPipelineBarrier pointer");
}

if (!(ptrs_.vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(getInstanceProcAddr(instance_, "vkCmdPushConstants")))) {
  return Result("Vulkan: Unable to load vkCmdPushConstants pointer");
}

if (!(ptrs_.vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(getInstanceProcAddr(instance_, "vkCreateBuffer")))) {
  return Result("Vulkan: Unable to load vkCreateBuffer pointer");
}

if (!(ptrs_.vkCreateBufferView = reinterpret_cast<PFN_vkCreateBufferView>(getInstanceProcAddr(instance_, "vkCreateBufferView")))) {
  return Result("Vulkan: Unable to load vkCreateBufferView pointer");
}

if (!(ptrs_.vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(getInstanceProcAddr(instance_, "vkCreateCommandPool")))) {
  return Result("Vulkan: Unable to load vkCreateCommandPool pointer");
}

if (!(ptrs_.vkCreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(getInstanceProcAddr(instance_, "vkCreateComputePipelines")))) {
  return Result("Vulkan: Unable to load vkCreateComputePipelines pointer");
}

if (!(ptrs_.vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(getInstanceProcAddr(instance_, "vkCreateDescriptorPool")))) {
  return Result("Vulkan: Unable to load vkCreateDescriptorPool pointer");
}

if (!(ptrs_.vkCreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(getInstanceProcAddr(instance_, "vkCreateDescriptorSetLayout")))) {
  return Result("Vulkan: Unable to load vkCreateDescriptorSetLayout pointer");
}

if (!(ptrs_.vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(getInstanceProcAddr(instance_, "vkCreateFence")))) {
  return Result("Vulkan: Unable to load vkCreateFence pointer");
}

if (!(ptrs_.vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(getInstanceProcAddr(instance_, "vkCreateFramebuffer")))) {
  return Result("Vulkan: Unable to load vkCreateFramebuffer pointer");
}

if (!(ptrs_.vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(getInstanceProcAddr(instance_, "vkCreateGraphicsPipelines")))) {
  return Result("Vulkan: Unable to load vkCreateGraphicsPipelines pointer");
}

if (!(ptrs_.vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(getInstanceProcAddr(instance_, "vkCreateImage")))) {
  return Result("Vulkan: Unable to load vkCreateImage pointer");
}

if (!(ptrs_.vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(getInstanceProcAddr(instance_, "vkCreateImageView")))) {
  return Result("Vulkan: Unable to load vkCreateImageView pointer");
}

if (!(ptrs_.vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(getInstanceProcAddr(instance_, "vkCreatePipelineLayout")))) {
  return Result("Vulkan: Unable to load vkCreatePipelineLayout pointer");
}

if (!(ptrs_.vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(getInstanceProcAddr(instance_, "vkCreateRenderPass")))) {
  return Result("Vulkan: Unable to load vkCreateRenderPass pointer");
}

if (!(ptrs_.vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(getInstanceProcAddr(instance_, "vkCreateShaderModule")))) {
  return Result("Vulkan: Unable to load vkCreateShaderModule pointer");
}

if (!(ptrs_.vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(getInstanceProcAddr(instance_, "vkDestroyBuffer")))) {
  return Result("Vulkan: Unable to load vkDestroyBuffer pointer");
}

if (!(ptrs_.vkDestroyBufferView = reinterpret_cast<PFN_vkDestroyBufferView>(getInstanceProcAddr(instance_, "vkDestroyBufferView")))) {
  return Result("Vulkan: Unable to load vkDestroyBufferView pointer");
}

if (!(ptrs_.vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(getInstanceProcAddr(instance_, "vkDestroyCommandPool")))) {
  return Result("Vulkan: Unable to load vkDestroyCommandPool pointer");
}

if (!(ptrs_.vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(getInstanceProcAddr(instance_, "vkDestroyDescriptorPool")))) {
  return Result("Vulkan: Unable to load vkDestroyDescriptorPool pointer");
}

if (!(ptrs_.vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(getInstanceProcAddr(instance_, "vkDestroyDescriptorSetLayout")))) {
  return Result("Vulkan: Unable to load vkDestroyDescriptorSetLayout pointer");
}

if (!(ptrs_.vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(getInstanceProcAddr(instance_, "vkDestroyFence")))) {
  return Result("Vulkan: Unable to load vkDestroyFence pointer");
}

if (!(ptrs_.vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(getInstanceProcAddr(instance_, "vkDestroyFramebuffer")))) {
  return Result("Vulkan: Unable to load vkDestroyFramebuffer pointer");
}

if (!(ptrs_.vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(getInstanceProcAddr(instance_, "vkDestroyImage")))) {
  return Result("Vulkan: Unable to load vkDestroyImage pointer");
}

if (!(ptrs_.vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(getInstanceProcAddr(instance_, "vkDestroyImageView")))) {
  return Result("Vulkan: Unable to load vkDestroyImageView pointer");
}

if (!(ptrs_.vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(getInstanceProcAddr(instance_, "vkDestroyPipeline")))) {
  return Result("Vulkan: Unable to load vkDestroyPipeline pointer");
}

if (!(ptrs_.vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(getInstanceProcAddr(instance_, "vkDestroyPipelineLayout")))) {
  return Result("Vulkan: Unable to load vkDestroyPipelineLayout pointer");
}

if (!(ptrs_.vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(getInstanceProcAddr(instance_, "vkDestroyRenderPass")))) {
  return Result("Vulkan: Unable to load vkDestroyRenderPass pointer");
}

if (!(ptrs_.vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(getInstanceProcAddr(instance_, "vkDestroyShaderModule")))) {
  return Result("Vulkan: Unable to load vkDestroyShaderModule pointer");
}

if (!(ptrs_.vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(getInstanceProcAddr(instance_, "vkEndCommandBuffer")))) {
  return Result("Vulkan: Unable to load vkEndCommandBuffer pointer");
}

if (!(ptrs_.vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(getInstanceProcAddr(instance_, "vkFreeCommandBuffers")))) {
  return Result("Vulkan: Unable to load vkFreeCommandBuffers pointer");
}

if (!(ptrs_.vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(getInstanceProcAddr(instance_, "vkFreeMemory")))) {
  return Result("Vulkan: Unable to load vkFreeMemory pointer");
}

if (!(ptrs_.vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(getInstanceProcAddr(instance_, "vkGetBufferMemoryRequirements")))) {
  return Result("Vulkan: Unable to load vkGetBufferMemoryRequirements pointer");
}

if (!(ptrs_.vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(getInstanceProcAddr(instance_, "vkGetImageMemoryRequirements")))) {
  return Result("Vulkan: Unable to load vkGetImageMemoryRequirements pointer");
}

if (!(ptrs_.vkGetPhysicalDeviceFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(getInstanceProcAddr(instance_, "vkGetPhysicalDeviceFormatProperties")))) {
  return Result("Vulkan: Unable to load vkGetPhysicalDeviceFormatProperties pointer");
}

if (!(ptrs_.vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(getInstanceProcAddr(instance_, "vkGetPhysicalDeviceMemoryProperties")))) {
  return Result("Vulkan: Unable to load vkGetPhysicalDeviceMemoryProperties pointer");
}

if (!(ptrs_.vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(getInstanceProcAddr(instance_, "vkGetPhysicalDeviceProperties")))) {
  return Result("Vulkan: Unable to load vkGetPhysicalDeviceProperties pointer");
}

if (!(ptrs_.vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(getInstanceProcAddr(instance_, "vkMapMemory")))) {
  return Result("Vulkan: Unable to load vkMapMemory pointer");
}

if (!(ptrs_.vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(getInstanceProcAddr(instance_, "vkQueueSubmit")))) {
  return Result("Vulkan: Unable to load vkQueueSubmit pointer");
}

if (!(ptrs_.vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(getInstanceProcAddr(instance_, "vkResetCommandBuffer")))) {
  return Result("Vulkan: Unable to load vkResetCommandBuffer pointer");
}

if (!(ptrs_.vkResetFences = reinterpret_cast<PFN_vkResetFences>(getInstanceProcAddr(instance_, "vkResetFences")))) {
  return Result("Vulkan: Unable to load vkResetFences pointer");
}

if (!(ptrs_.vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(getInstanceProcAddr(instance_, "vkUnmapMemory")))) {
  return Result("Vulkan: Unable to load vkUnmapMemory pointer");
}

if (!(ptrs_.vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(getInstanceProcAddr(instance_, "vkUpdateDescriptorSets")))) {
  return Result("Vulkan: Unable to load vkUpdateDescriptorSets pointer");
}

if (!(ptrs_.vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(getInstanceProcAddr(instance_, "vkWaitForFences")))) {
  return Result("Vulkan: Unable to load vkWaitForFences pointer");
}
