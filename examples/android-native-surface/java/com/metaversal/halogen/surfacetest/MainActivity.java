// Copyright 2026 Metaversal Corporation
// SPDX-License-Identifier: MIT

package com.metaversal.halogen.surfacetest;

import android.app.Activity;
import android.os.Bundle;
import android.view.Choreographer;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

// Mirrors the Filament gltf-viewer sample's structure: standard Activity +
// SurfaceView + SurfaceHolder.Callback. The underlying JNI call receives a
// java.view.Surface object and converts it to ANativeWindow* via
// ANativeWindow_fromSurface(env, surface) — the exact pattern Filament's
// filament-jni uses. This is the control we're comparing against our earlier
// NativeActivity-based test that crashed inside vkCreateAndroidSurfaceKHR.
public class MainActivity extends Activity implements SurfaceHolder.Callback, Choreographer.FrameCallback {
    static {
        System.loadLibrary("halogen_surface_test");
    }

    private native void nativeSurfaceCreated(Surface surface);
    private native void nativeSurfaceChanged(Surface surface, int format, int width, int height);
    private native void nativeSurfaceDestroyed();
    private native void nativeOnResume();
    private native void nativeOnPause();
    private native void nativeDoFrame();

    private Choreographer mChoreographer;
    private boolean mSurfaceReady = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        mChoreographer = Choreographer.getInstance();

        SurfaceView sv = new SurfaceView(this);
        sv.getHolder().addCallback(this);
        setContentView(sv);
    }

    @Override protected void onResume()  {
        super.onResume();
        nativeOnResume();
        if (mSurfaceReady) mChoreographer.postFrameCallback(this);
    }
    @Override protected void onPause()   {
        mChoreographer.removeFrameCallback(this);
        nativeOnPause();
        super.onPause();
    }

    @Override public void surfaceCreated(SurfaceHolder holder) {
        nativeSurfaceCreated(holder.getSurface());
        mSurfaceReady = true;
        mChoreographer.postFrameCallback(this);
    }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        nativeSurfaceChanged(holder.getSurface(), format, width, height);
    }
    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        mSurfaceReady = false;
        mChoreographer.removeFrameCallback(this);
        nativeSurfaceDestroyed();
    }

    @Override public void doFrame(long frameTimeNanos) {
        nativeDoFrame();
        if (mSurfaceReady) mChoreographer.postFrameCallback(this);
    }
}
