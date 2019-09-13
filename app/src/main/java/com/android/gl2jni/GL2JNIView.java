/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.gl2jni;
/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writilng, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.PixelFormat;
import android.graphics.BitmapFactory;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.os.SystemClock;

import java.io.InputStream;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

/**
 * A simple GLSurfaceView sub-class that demonstrate how to perform
 * OpenGL ES 2.0 rendering into a GL Surface. Note the following important
 * details:
 *
 * - The class must use a custom context factory to enable 2.0 rendering.
 *   See ContextFactory class definition below.
 *
 * - The class must use a custom EGLConfigChooser to be able to select
 *   an EGLConfig that supports 2.0. This is done by providing a config
 *   specification to eglChooseConfig() that has the attribute
 *   EGL10.ELG_RENDERABLE_TYPE containing the EGL_OPENGL_ES2_BIT flag
 *   set. See ConfigChooser class definition below.
 *
 * - The class must select the surface's format, then choose an EGLConfig
 *   that matches it exactly (with regards to red/green/blue/alpha channels
 *   bit depths). Failure to do so would result in an EGL_BAD_MATCH error.
 */
class GL2JNIView extends GLSurfaceView {
    private static String TAG = "GL2JNIView";
    private static final boolean DEBUG = false;

    private static float dx = 0;
    private static float dy = 0;
    private static float dangle = 0;
    private static float scale = 1;
    private static boolean enableTrans = false;

    private static int screen_width = 100;
    private static int screen_height = 100;

    private static Resources res;

    private static int[] TEXTURES_RESOURCES = {
            R.raw.barrel,
            R.raw.bench,
            R.raw.cargo_wo,
            R.raw.house_col,
            R.raw.house_nor,
            R.raw.house_spec,
            R.raw.house2_col,
            R.raw.house2_nor,
            R.raw.house2_spec,
            R.raw.tent_col,
            R.raw.tent_nor,
            R.raw.tent_spec,
            R.raw.wagen_1,
            R.raw.ground_grass_3264_4062_small,
            R.raw.tuktuk_texture,
            R.raw.tuktuk_texture,
    };

    private static String[] TEXTURES_NAMES = {
            "barrel.jpg",
            "bench.jpg",
            "cargo_wo.jpg",
            "house_col",
            "house_nor",
            "house_spec",
            "house2_col",
            "house2_nor",
            "house2_spec",
            "tent_col",
            "tent_nor",
            "tent_spec",
            "wagen_1.jpg",
            "ground_grass_3264_4062_small.jpg",
            "1.BMP",
            "2.BMP",
    };

    private static int[] MODELS_RESOURCES = {
            R.raw.wagen,
            R.raw.house,
            R.raw.house2,
            R.raw.model_barrel,
            R.raw.model_bench,
            R.raw.model_box,
            R.raw.tent,
            R.raw.mount,
            R.raw.tuktuk,
    };

    private static String[] MODELS_NAMES = {
            "wagen",
            "house",
            "house2",
            "barrel",
            "bench",
            "box",
            "tent",
            "mount",
            "tuktuk",
    };

    private static boolean[] MODELS_EXTERNAL = {
            false,
            true,
            true,
            false,
            false,
            false,
            true,
            false,
            false,
    };

    public GL2JNIView(Context context) {
        super(context);
        init(true, 0, 0);
    }

    public GL2JNIView(Context context, boolean translucent, int depth, int stencil) {
        super(context);
        init(translucent, depth, stencil);
    }

    private void init(boolean translucent, int depth, int stencil) {

        /* By default, GLSurfaceView() creates a RGB_565 opaque surface.
         * If we want a translucent one, we should change the surface's
         * format here, using PixelFormat.TRANSLUCENT for GL Surfaces
         * is interpreted as any 32-bit surface with alpha by SurfaceFlinger.
         */
        if (translucent) {
            this.getHolder().setFormat(PixelFormat.TRANSLUCENT);
        }

        /* Setup the context factory for 2.0 rendering.
         * See ContextFactory class definition below
         */
        setEGLContextFactory(new ContextFactory());

        /* We need to choose an EGLConfig that matches the format of
         * our surface exactly. This is going to be done in our
         * custom config chooser. See ConfigChooser class definition
         * below.
         */
//        setEGLConfigChooser( translucent ?
//                             new ConfigChooser(8, 8, 8, 8, depth, stencil) :
//                             new ConfigChooser(5, 6, 5, 0, depth, stencil) );

        setEGLConfigChooser(true);

        res = getResources();

        loadThePreload();

        new Thread(new LoadingThread()).start();

        /* Set the renderer responsible for frame rendering */
        setRenderer(new Renderer());
    }

    private static class ContextFactory implements GLSurfaceView.EGLContextFactory {
        private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
        public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) {
            Log.w(TAG, "creating OpenGL ES 2.0 context");
            checkEglError("Before eglCreateContext", egl);
            int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE };
            EGLContext context = egl.eglCreateContext(display, eglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
            checkEglError("After eglCreateContext", egl);
            return context;
        }

        public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) {
            egl.eglDestroyContext(display, context);
        }
    }

    private static void checkEglError(String prompt, EGL10 egl) {
        int error;
        while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) {
            Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
        }
    }

    private static class ConfigChooser implements GLSurfaceView.EGLConfigChooser {

        public ConfigChooser(int r, int g, int b, int a, int depth, int stencil) {
            mRedSize = r;
            mGreenSize = g;
            mBlueSize = b;
            mAlphaSize = a;
            mDepthSize = depth;
            mStencilSize = stencil;
        }

        /* This EGL config specification is used to specify 2.0 rendering.
         * We use a minimum size of 4 bits for red/green/blue, but will
         * perform actual matching in chooseConfig() below.
         */
        private static int EGL_OPENGL_ES2_BIT = 4;
        private static int[] s_configAttribs2 =
        {
            EGL10.EGL_RED_SIZE, 4,
            EGL10.EGL_GREEN_SIZE, 4,
            EGL10.EGL_BLUE_SIZE, 4,
            EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL10.EGL_NONE
        };

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {

            /* Get the number of minimally matching EGL configurations
             */
            int[] num_config = new int[1];
            egl.eglChooseConfig(display, s_configAttribs2, null, 0, num_config);

            int numConfigs = num_config[0];

            if (numConfigs <= 0) {
                throw new IllegalArgumentException("No configs match configSpec");
            }

            /* Allocate then read the array of minimally matching EGL configs
             */
            EGLConfig[] configs = new EGLConfig[numConfigs];
            egl.eglChooseConfig(display, s_configAttribs2, configs, numConfigs, num_config);

            if (DEBUG) {
                 printConfigs(egl, display, configs);
            }
            /* Now return the "best" one
             */
            return chooseConfig(egl, display, configs);
        }

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display,
                EGLConfig[] configs) {
            for(EGLConfig config : configs) {
                int d = findConfigAttrib(egl, display, config,
                        EGL10.EGL_DEPTH_SIZE, 0);
                int s = findConfigAttrib(egl, display, config,
                        EGL10.EGL_STENCIL_SIZE, 0);

                // We need at least mDepthSize and mStencilSize bits
                if (d < mDepthSize || s < mStencilSize)
                    continue;

                // We want an *exact* match for red/green/blue/alpha
                int r = findConfigAttrib(egl, display, config,
                        EGL10.EGL_RED_SIZE, 0);
                int g = findConfigAttrib(egl, display, config,
                            EGL10.EGL_GREEN_SIZE, 0);
                int b = findConfigAttrib(egl, display, config,
                            EGL10.EGL_BLUE_SIZE, 0);
                int a = findConfigAttrib(egl, display, config,
                        EGL10.EGL_ALPHA_SIZE, 0);

                if (r == mRedSize && g == mGreenSize && b == mBlueSize && a == mAlphaSize)
                    return config;
            }
            return null;
        }

        private int findConfigAttrib(EGL10 egl, EGLDisplay display,
                EGLConfig config, int attribute, int defaultValue) {

            if (egl.eglGetConfigAttrib(display, config, attribute, mValue)) {
                return mValue[0];
            }
            return defaultValue;
        }

        private void printConfigs(EGL10 egl, EGLDisplay display,
            EGLConfig[] configs) {
            int numConfigs = configs.length;
            Log.w(TAG, String.format("%d configurations", numConfigs));
            for (int i = 0; i < numConfigs; i++) {
                Log.w(TAG, String.format("Configuration %d:\n", i));
                printConfig(egl, display, configs[i]);
            }
        }

        private void printConfig(EGL10 egl, EGLDisplay display,
                EGLConfig config) {
            int[] attributes = {
                    EGL10.EGL_BUFFER_SIZE,
                    EGL10.EGL_ALPHA_SIZE,
                    EGL10.EGL_BLUE_SIZE,
                    EGL10.EGL_GREEN_SIZE,
                    EGL10.EGL_RED_SIZE,
                    EGL10.EGL_DEPTH_SIZE,
                    EGL10.EGL_STENCIL_SIZE,
                    EGL10.EGL_CONFIG_CAVEAT,
                    EGL10.EGL_CONFIG_ID,
                    EGL10.EGL_LEVEL,
                    EGL10.EGL_MAX_PBUFFER_HEIGHT,
                    EGL10.EGL_MAX_PBUFFER_PIXELS,
                    EGL10.EGL_MAX_PBUFFER_WIDTH,
                    EGL10.EGL_NATIVE_RENDERABLE,
                    EGL10.EGL_NATIVE_VISUAL_ID,
                    EGL10.EGL_NATIVE_VISUAL_TYPE,
                    0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
                    EGL10.EGL_SAMPLES,
                    EGL10.EGL_SAMPLE_BUFFERS,
                    EGL10.EGL_SURFACE_TYPE,
                    EGL10.EGL_TRANSPARENT_TYPE,
                    EGL10.EGL_TRANSPARENT_RED_VALUE,
                    EGL10.EGL_TRANSPARENT_GREEN_VALUE,
                    EGL10.EGL_TRANSPARENT_BLUE_VALUE,
                    0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
                    0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
                    0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
                    0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
                    EGL10.EGL_LUMINANCE_SIZE,
                    EGL10.EGL_ALPHA_MASK_SIZE,
                    EGL10.EGL_COLOR_BUFFER_TYPE,
                    EGL10.EGL_RENDERABLE_TYPE,
                    0x3042 // EGL10.EGL_CONFORMANT
            };
            String[] names = {
                    "EGL_BUFFER_SIZE",
                    "EGL_ALPHA_SIZE",
                    "EGL_BLUE_SIZE",
                    "EGL_GREEN_SIZE",
                    "EGL_RED_SIZE",
                    "EGL_DEPTH_SIZE",
                    "EGL_STENCIL_SIZE",
                    "EGL_CONFIG_CAVEAT",
                    "EGL_CONFIG_ID",
                    "EGL_LEVEL",
                    "EGL_MAX_PBUFFER_HEIGHT",
                    "EGL_MAX_PBUFFER_PIXELS",
                    "EGL_MAX_PBUFFER_WIDTH",
                    "EGL_NATIVE_RENDERABLE",
                    "EGL_NATIVE_VISUAL_ID",
                    "EGL_NATIVE_VISUAL_TYPE",
                    "EGL_PRESERVED_RESOURCES",
                    "EGL_SAMPLES",
                    "EGL_SAMPLE_BUFFERS",
                    "EGL_SURFACE_TYPE",
                    "EGL_TRANSPARENT_TYPE",
                    "EGL_TRANSPARENT_RED_VALUE",
                    "EGL_TRANSPARENT_GREEN_VALUE",
                    "EGL_TRANSPARENT_BLUE_VALUE",
                    "EGL_BIND_TO_TEXTURE_RGB",
                    "EGL_BIND_TO_TEXTURE_RGBA",
                    "EGL_MIN_SWAP_INTERVAL",
                    "EGL_MAX_SWAP_INTERVAL",
                    "EGL_LUMINANCE_SIZE",
                    "EGL_ALPHA_MASK_SIZE",
                    "EGL_COLOR_BUFFER_TYPE",
                    "EGL_RENDERABLE_TYPE",
                    "EGL_CONFORMANT"
            };
            int[] value = new int[1];
            for (int i = 0; i < attributes.length; i++) {
                int attribute = attributes[i];
                String name = names[i];
                if ( egl.eglGetConfigAttrib(display, config, attribute, value)) {
                    Log.w(TAG, String.format("  %s: %d\n", name, value[0]));
                } else {
                    // Log.w(TAG, String.format("  %s: failed\n", name));
                    while (egl.eglGetError() != EGL10.EGL_SUCCESS);
                }
            }
        }

        // Subclasses can adjust these values:
        protected int mRedSize;
        protected int mGreenSize;
        protected int mBlueSize;
        protected int mAlphaSize;
        protected int mDepthSize;
        protected int mStencilSize;
        private int[] mValue = new int[1];
    }


    private final float TOUCH_SCALE_FACTOR = 180.0f / 320;
    private float[] mPrevX = new float[10];
    private float[] mPrevY = new float[10];

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        // MotionEvent reports input details from the touch screen
        // and other input controls. In this case, you are only
        // interested in events where the touch position changed.

        if (e.getPointerCount() == 1)
        {
            float x = (e.getX(0)-(screen_width/2))/(float)screen_width;
            float y = ((screen_height/2)-e.getY(0))/(float)screen_height;

            switch (e.getAction())
            {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_MOVE:
                {
                    dx = x;
                    dy = y;
                    scale = 1;
                    dangle = 0;
                    enableTrans = true;

  //                  System.out.println("Scale " + scale + " rotate " + dangle + " translate " + dx + "," + dy);

                    break;
                }
            }
        }
        else
        {
//            if (e.getPointerCount() == 2)
//            {
//                float x0 = (e.getX(0)-(screen_width/2))/(float)screen_width;
//                float y0 = ((screen_height/2)-e.getY(0))/(float)screen_height;
//                float x1 = (e.getX(1)-(screen_width/2))/(float)screen_width;
//                float y1 = ((screen_height/2)-e.getY(1))/(float)screen_height;
//
//                switch (e.getAction())
//                {
//                    case MotionEvent.ACTION_MOVE:
//                    {
//                        float delta_x_old = mPrevX[1]-mPrevX[0];
//                        float delta_x_new = x1-x0;
//                        float delta_y_old = mPrevY[1]-mPrevY[0];
//                        float delta_y_new = y1-y0;
//                        float dist_old = (float)Math.sqrt(delta_x_old*delta_x_old+delta_y_old*delta_y_old);
//                        float dist_new = (float)Math.sqrt(delta_x_new*delta_x_new+delta_y_new*delta_y_new);
//                        scale = dist_new/dist_old;
//                        float sinT = ((delta_y_new*delta_x_old)-(delta_x_new*delta_y_old))/(scale*(delta_x_old*delta_x_old-delta_y_old*delta_y_old));
//                        dangle = (float)Math.asin(sinT);
//                        float cosT = (float)Math.cos(dangle);
//                        dx = x0-cosT*scale*mPrevX[0]-sinT*scale*mPrevY[0];
//                        dy = y0-cosT*scale*mPrevY[0]+sinT*scale*mPrevX[0];
//                        enableTrans = true;
//
////                        System.out.println("Scale " + scale + " rotate " + dangle + " translate " + dx + "," + dy);
//
//                        break;
//                    }
//                }
//
//                mPrevX[0] = x0;
//                mPrevX[1] = x1;
//                mPrevY[0] = y0;
//                mPrevY[1] = y1;
//            }
        }

        return true;
    }

    public static int getSize(int rid) {
        int size = 0;
        try {
            InputStream in_s;
            in_s = res.openRawResource(rid);
            size = in_s.available();
            in_s.close();
        } catch (Exception ex) {
            System.out.println("Exception: " + ex.getMessage());
        }
        return size;
    }

    static byte[] b = new byte[1024*1024];

    public static void loadModels() {
        try {
            for (int i = 0 ; i < MODELS_RESOURCES.length ; i++)
            {
                InputStream in_s = res.openRawResource(MODELS_RESOURCES[i]);
                in_s.read(b);
                GL2JNILib.loadModel(MODELS_NAMES[i],b,MODELS_EXTERNAL[i]);
            }
            GL2JNILib.doneLoadingModels();
        } catch (Exception ex) {
            System.out.println("Exception: " + ex.getMessage());
        }
    }

    static int[] pixels = new int[4096*4096];

    public static void loadTextures() {
        try {
            for (int i = 0 ; i < TEXTURES_RESOURCES.length ; i++)
            {
                Bitmap btmp = BitmapFactory.decodeResource(res,TEXTURES_RESOURCES[i]);
                int w = btmp.getWidth();
                int h = btmp.getHeight();
                btmp.getPixels(pixels,0,w,0,0,w,h);
                GL2JNILib.loadRAW(TEXTURES_NAMES[i],w,h,getSize(TEXTURES_RESOURCES[i]),pixels);
            }
            GL2JNILib.doneLoadingTextures();
        } catch (Exception ex) {
            System.out.println("Exception: " + ex.getMessage());
        }
    }

    public static void loadThePreload() {
        try {
            Bitmap btmp = BitmapFactory.decodeResource(res,R.raw.obj_tyre_d);
            int w = btmp.getWidth();
            int h = btmp.getHeight();
            btmp.getPixels(pixels,0,w,0,0,w,h);
            GL2JNILib.loadRAW("OBJ_TYRE.TGA",w,h,0,pixels);

            InputStream in_s = res.openRawResource(R.raw.tire);
            in_s.read(b);
            GL2JNILib.loadModel("tire",b,false);
        } catch (Exception ex) {
            System.out.println("Exception: " + ex.getMessage());
        }
    }

    private class LoadingThread implements Runnable {
        @Override
        public void run() {
            int size = 0;
            for (int i = 0 ; i < TEXTURES_RESOURCES.length ; i++)
                size += getSize(TEXTURES_RESOURCES[i]);
            for (int i = 0 ; i < MODELS_RESOURCES.length ; i++)
                size += getSize(MODELS_RESOURCES[i]);
            GL2JNILib.setTotalBytes(size);

            loadTextures();
            loadModels();
        }
    }

    private static class Renderer implements GLSurfaceView.Renderer {
        public void onDrawFrame(GL10 gl)
        {
            if (enableTrans == false)
            {
                dx = 0;
                dy = 0;
                dangle = 0;
                scale = 1;
            }
            GL2JNILib.step(dx,dy,dangle,scale);
            enableTrans = false;
        }

        public void onSurfaceChanged(GL10 gl, int width, int height) {
            screen_width = width;
            screen_height = height;
            GL2JNILib.resize(width, height);
        }

        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            GL2JNILib.init();
        }
    }
}
