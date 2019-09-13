/*
 * Copyright (C) 2007 The Android Open Source Project
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

// Wrapper for native library

public class GL2JNILib {

    static {
     System.loadLibrary("gl2jni");
    }

    public static native void init();
    public static native void step(float dx, float dy, float dangle, float scale);
    public static native void setTotalBytes(int total);
    public static native void resize(int width, int height);
    public static native void loadModel(String name, byte[] buffer, boolean external);
    public static native void loadBMP(String filename, byte[] buffer);
    public static native void loadTGA(String filename, byte[] buffer);
    public static native void loadDDS(String filename, byte[] buffer);
    public static native void loadRAW(String filename, int width, int height, int src_size, int[] buffer);
    public static native void doneLoadingTextures();
    public static native void doneLoadingModels();
}
