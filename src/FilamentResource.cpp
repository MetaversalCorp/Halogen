// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "FilamentResource.h"

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/IndirectLight.h>
#include <filament/Material.h>
#include <filament/Renderer.h>
#include <filament/RenderTarget.h>
#include <filament/SwapChain.h>
#include <filament/Texture.h>
#include <filament/View.h>

namespace Halogen {

template <typename T>
void FilamentResource<T>::reset(T *ptr) {
    destroy();
    mPtr = ptr;
}

template <typename T>
void FilamentResource<T>::destroy() {
    if (mPtr) {
        if (mDeleter) {
            mDeleter();
        } else if constexpr (requires(filament::Engine *e, T *p) {
            e->destroy(p);
        }) {
            mEngine->destroy(mPtr);
        }
        mPtr = nullptr;
    }
}

// Explicit instantiations for all Filament types we manage
template class FilamentResource<filament::Camera>;
template class FilamentResource<filament::IndirectLight>;
template class FilamentResource<filament::View>;
template class FilamentResource<filament::SwapChain>;
template class FilamentResource<filament::Texture>;
template class FilamentResource<filament::RenderTarget>;
template class FilamentResource<filament::Material>;
template class FilamentResource<filament::Renderer>;

}
