// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT
//
// Compiled with -fno-rtti so it can subclass Filament's VulkanPlatformAndroid
// (which is built without RTTI). Do not include this from RTTI translation units.

#include <backend/platforms/VulkanPlatformAndroid.h>

#include <cstdint>

struct HALOGEN_VULKAN_CREATE
{
    void *pUser = nullptr;
    uint64_t (*fnCreateInstance)(void *, const void *) = nullptr;
    uint64_t (*fnSelectPhysicalDevice)(void *, uint64_t) = nullptr;
    uint64_t (*fnCreateDevice)(void *, const void *, uint64_t) = nullptr;
};

static HALOGEN_VULKAN_CREATE g_VulkanCreate{};

extern "C" {

__attribute__((visibility("default")))
void halogenSetVulkanCreate(const HALOGEN_VULKAN_CREATE *p)
{
    if (p)
        g_VulkanCreate = *p;
    else
        g_VulkanCreate = HALOGEN_VULKAN_CREATE{};
}

} // extern "C"

class HalogenXrVulkanPlatform : public filament::backend::VulkanPlatformAndroid
{
public:
    VkInstance createVkInstance(VkInstanceCreateInfo const &createInfo) noexcept override
    {
        if (g_VulkanCreate.fnCreateInstance) {
            uint64_t n = g_VulkanCreate.fnCreateInstance(
                g_VulkanCreate.pUser, &createInfo);
            return reinterpret_cast<VkInstance>(static_cast<uintptr_t>(n));
        }
        return VulkanPlatformAndroid::createVkInstance(createInfo);
    }

    VkPhysicalDevice selectVkPhysicalDevice(VkInstance instance) noexcept override
    {
        if (g_VulkanCreate.fnSelectPhysicalDevice) {
            uint64_t n = g_VulkanCreate.fnSelectPhysicalDevice(
                g_VulkanCreate.pUser,
                static_cast<uint64_t>(reinterpret_cast<uintptr_t>(instance)));
            mPhysical = reinterpret_cast<VkPhysicalDevice>(
                static_cast<uintptr_t>(n));
            return mPhysical;
        }
        mPhysical = VulkanPlatformAndroid::selectVkPhysicalDevice(instance);
        return mPhysical;
    }

    VkDevice createVkDevice(VkDeviceCreateInfo const &createInfo) noexcept override
    {
        if (g_VulkanCreate.fnCreateDevice) {
            uint64_t nPhys = static_cast<uint64_t>(
                reinterpret_cast<uintptr_t>(mPhysical));
            uint64_t n = g_VulkanCreate.fnCreateDevice(
                g_VulkanCreate.pUser, &createInfo, nPhys);
            return reinterpret_cast<VkDevice>(static_cast<uintptr_t>(n));
        }
        return VulkanPlatformAndroid::createVkDevice(createInfo);
    }

private:
    VkPhysicalDevice mPhysical = VK_NULL_HANDLE;
};

filament::backend::VulkanPlatform *halogenCreateXrVulkanPlatform()
{
    return new HalogenXrVulkanPlatform();
}

bool halogenHasVulkanCreateHooks()
{
    return g_VulkanCreate.fnCreateInstance != nullptr;
}
