/*
 * Copyright (c) 2020, 2023, Huawei Technologies Co., Ltd. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package java.net;

import jdk.internal.loader.Resource;
import jdk.internal.loader.URLClassPath;

import java.io.IOException;
import java.io.InputStream;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Method;

import sun.security.action.GetBooleanAction;

/**
 * The Aggressive CDS plugin for {@link java.net.URLClassLoader}.
 */
final class AggressiveCDSPlugin {
    private static final boolean IS_ENABLED = GetBooleanAction.privilegedGetProperty("jdk.jbooster.aggressivecds.load");

    /**
     * Check whether Aggressive CDS is enabled.
     *
     * @return Is Aggressive CDS enabled
     */
    public static boolean isEnabled() {
        return IS_ENABLED;
    }

    /**
     * Define the class by Aggressive CDS. The class is trusted and shared,
     *
     * @param   loader The class loader of the class (should be URLClassLoader)
     * @param   name The name of the class
     * @return  The defined class, or null if not found
     */
    public static Class<?> defineTrustedSharedClass(URLClassLoader loader, String name) {
        return ClassLoaderUtil.defineClass3(loader, name);
    }

    /**
     * get URL from URLClassPath by Aggressive CDS.
     *
     * @param   ucp The URLClassPath
     * @param   urlNoFragString The name string of the url
     * @return  The URL, or null if not found
     */
    public static URL getURLFromURLClassPath(URLClassPath ucp, String urlNoFragString) {
        return URLClassPathTool.getURL(ucp, urlNoFragString);
    }

    /**
     * Finds the byte code with the specified name on the URL search path.
     * This method is invoked only in C++ ({@code SystemDictionaryShared::get_byte_code_from_cache}).
     *
     * @param   loader The class loader of the class
     * @param   name The name of the class
     * @return  Byte code of the resource, or {@code null} if not found
     * @throws  IOException Resource.getBytes()
     */
    private static byte[] getByteCodeFromCache(URLClassLoader loader, String name) throws IOException {
        String path = name.replace('.', '/').concat(".class");
        Resource resource = getResourceFromCache(loader, path);
        if (resource == null) {
            return null;
        } else {
            return resource.getBytes();
        }
    }

    /**
     * Finds the byte code with the specified name on the URL search path.
     * This method is invoked only in C++.
     *
     * @param   loader The class loader of the class
     * @param   name The name of the class
     * @return  The resource in cache
     */
    private static Resource getResourceFromCache(URLClassLoader loader, final String name) {
        URL url = loader.findResource(name);
        if (url == null) {
            return null;
        }
        final URLConnection uc;
        try {
            uc = url.openConnection();
        } catch (IOException e) {
            return null;
        }
        return new Resource() {
            @Override
            public String getName() {
                return name;
            }

            @Override
            public URL getURL() {
                return url;
            }

            @Override
            public URL getCodeSourceURL() {
                return url;
            }

            @Override
            public InputStream getInputStream() throws IOException {
                return uc.getInputStream();
            }

            @Override
            public int getContentLength() throws IOException {
                return uc.getContentLength();
            }
        };
    }
}

/**
 * We don't want to add new public methods in {@link java.lang.ClassLoader}. So we add
 * a private method (defineClass3) and use a method handle to invoke it.
 */
final class ClassLoaderUtil {
    private static final MethodHandle classDefiner3;

    static {
        MethodHandle mh3 = null;
        try {
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            Method m3 = ClassLoader.class.getDeclaredMethod("defineClass3", ClassLoader.class, String.class);
            m3.setAccessible(true);
            mh3 = lookup.unreflect(m3);
        } catch (NoSuchMethodException | IllegalAccessException e) {
            e.printStackTrace();
            System.exit(1);
        }
        classDefiner3 = mh3;
    }

    public static Class<?> defineClass3(ClassLoader loader, String name) {
        try {
            return (Class<?>) classDefiner3.invoke(loader, name);
        } catch (Throwable throwable) {
            throwable.printStackTrace();
            System.exit(1);
        }
        return null;
    }
}

/**
 * We don't want to add new public methods in {@link jdk.internal.loader.URLClassPath}. So we add
 * a private method (getURL) and use a method handle to invoke it.
 */
final class URLClassPathTool {
    private static final MethodHandle getURLMethodHandle;

    static {
        MethodHandle getURL = null;
        try {
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            Method getURLMethod = URLClassPath.class.getDeclaredMethod("getURL", String.class);
            getURLMethod.setAccessible(true);
            getURL = lookup.unreflect(getURLMethod);
        } catch (NoSuchMethodException | IllegalAccessException e) {
            e.printStackTrace();
            System.exit(1);
        }
        getURLMethodHandle = getURL;
    }

    public static URL getURL(URLClassPath ucp, String urlNoFragString) {
        try {
            return (URL) getURLMethodHandle.invoke(ucp, urlNoFragString);
        } catch (Throwable throwable) {
            throwable.printStackTrace();
            System.exit(1);
        }
        return null;
    }
}