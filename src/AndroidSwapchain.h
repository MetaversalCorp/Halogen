// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#if defined(__ANDROID__)

namespace Halogen {

// Filament 1.71's Vulkan swapchain ignores SwapChain::CONFIG_TRANSPARENT and
// creates an OPAQUE/INHERIT VkSwapchainKHR. Android's compositor then discards
// framebuffer alpha, so a 0-alpha clear cannot show a camera view underneath.
// Wrap bluevk's vkCreateSwapchainKHR and prefer PRE_MULTIPLIED (then
// POST_MULTIPLIED) composite alpha. Call before Engine::createSwapChain.
void installTransparentSwapchainHook();

}

#endif
