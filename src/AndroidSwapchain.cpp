// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "AndroidSwapchain.h"

#if defined(__ANDROID__)

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

namespace bluevk {
extern PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
}

namespace Halogen {
namespace {

PFN_vkCreateSwapchainKHR s_pCreateSwapchain = nullptr;

VkResult VKAPI_CALL createSwapchainTransparent(VkDevice device,
    VkSwapchainCreateInfoKHR const *pCreateInfo,
    VkAllocationCallbacks const *pAllocator,
    VkSwapchainKHR *pSwapchain)
{
    if (!s_pCreateSwapchain || !pCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCompositeAlphaFlagBitsKHR const requested = pCreateInfo->compositeAlpha;
    if (requested == VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
        || requested == VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        return s_pCreateSwapchain(device, pCreateInfo, pAllocator, pSwapchain);
    }

    VkCompositeAlphaFlagBitsKHR const aPreferred[] = {
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
    };
    for (VkCompositeAlphaFlagBitsKHR const mode : aPreferred) {
        VkSwapchainCreateInfoKHR info = *pCreateInfo;
        info.compositeAlpha = mode;
        VkResult const result =
            s_pCreateSwapchain(device, &info, pAllocator, pSwapchain);
        if (result == VK_SUCCESS) {
            return result;
        }
    }

    return s_pCreateSwapchain(device, pCreateInfo, pAllocator, pSwapchain);
}

}

void installTransparentSwapchainHook()
{
    if (s_pCreateSwapchain) {
        return;
    }
    if (!bluevk::vkCreateSwapchainKHR) {
        return;
    }
    s_pCreateSwapchain = bluevk::vkCreateSwapchainKHR;
    bluevk::vkCreateSwapchainKHR = createSwapchainTransparent;
}

}

#endif
