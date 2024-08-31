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

import org.testng.Assert;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;

import java.io.File;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Map;
import java.util.Objects;

/*
 * @test
 * @run testng/othervm
 *      --add-opens=java.base/java.net=ALL-UNNAMED
 *      --add-opens=java.base/jdk.internal.loader=ALL-UNNAMED
 *      -XX:+UnlockExperimentalVMOptions
 *      -XX:+UseClassLoaderResourceCache
 *      -XX:DumpClassLoaderResourceCacheFile=clrct.log
 *      ClassLoaderResourceCacheTest
 * @run testng/othervm
 *      --add-opens=java.base/java.net=ALL-UNNAMED
 *      --add-opens=java.base/jdk.internal.loader=ALL-UNNAMED
 *      -XX:+UnlockExperimentalVMOptions
 *      -XX:+UseClassLoaderResourceCache
 *      -XX:LoadClassLoaderResourceCacheFile=clrct.log
 *      ClassLoaderResourceCacheTest
 */
public class ClassLoaderResourceCacheTest {
    private URL[] urls;
    private URLClassLoader loader;
    private Object loaderCache;

    @BeforeMethod
    public void initLoader() {
        String classpath = System.getProperty("java.class.path");
        String[] paths = classpath.split(File.pathSeparator);
        urls = Arrays.stream(paths).map(s -> {
            if (s.endsWith(".jar")) {
                s = "jar:file:" + s + "!/";
            } else {
                s = "file:" + s + "/";
            }
            try {
                return new URL(s);
            } catch (MalformedURLException e) {
                e.printStackTrace();
            }
            return null;
        }).filter(Objects::nonNull).sorted(Comparator.comparing(URL::toExternalForm)).toArray(URL[]::new);
        if (urls.length >= 2 && urls[0].toExternalForm().endsWith("ClassLoaderResourceCacheTest.d/")) {
            URL tmp = urls[0];
            urls[0] = urls[1];
            urls[1] = tmp;
        }
        loader = new URLClassLoader(ClassLoaderResourceCacheTest.class.getSimpleName(), urls, null);
        loaderCache = Access.getClassLoaderResourceCache(loader);
    }

    @Test
    public void testIsFeatureOn() {
        Assert.assertTrue(Boolean.getBoolean("jdk.jbooster.clrcache.enable"));
        String dumpFilePath = System.getProperty("jdk.jbooster.clrcache.dump");
        String loadFilePath = System.getProperty("jdk.jbooster.clrcache.load");
        if (dumpFilePath != null) {
            Assert.assertFalse(new File(dumpFilePath).isFile());
        }
        if (loadFilePath != null) {
            Assert.assertTrue(new File(loadFilePath).isFile());
        }
    }

    @Test
    public void testLoaderURLs() {
        Assert.assertNotNull(loaderCache);
        Arrays.stream(urls).forEach(url -> System.out.println("URLClassLoader url: " + url));
        Assert.assertTrue(urls[0].toExternalForm().endsWith("URLClassLoader/"));
        Assert.assertTrue(urls[1].toExternalForm().endsWith("ClassLoaderResourceCacheTest.d/"));
        int i;
        for (i = 2; i < urls.length; ++i) {
            if (urls[i].toExternalForm().contains("testng")) {
                break;
            }
        }
        Assert.assertTrue(i < urls.length, "\"testng-xxx.jar\" must be in the paths!");
    }

    @Test
    public void testCacheResource() {
        String cs1 = ClassLoaderResourceCacheTest.class.getName().replace('.', '/') + ".class";
        String cs2 = "com/huawei/Nonexistent.class";
        String cs3 = "org/testng/Assert.class";
        URL res;
        Object entry;

        // Try to access three resources.

        Assert.assertEquals(Access.getResourceUrlCacheMap(loaderCache).size(), 0);

        res = loader.getResource(cs1);
        Assert.assertNotNull(res);
        Assert.assertTrue(res.toExternalForm().contains("ClassLoaderResourceCacheTest.d"));

        res = loader.getResource(cs2);
        Assert.assertNull(res);

        res = loader.getResource(cs3);
        Assert.assertNotNull(res);
        Assert.assertTrue(res.toExternalForm().contains("lib/testng-"));

        // Check cache entries.

        Assert.assertEquals(Access.getResourceUrlCacheMap(loaderCache).size(), 3);

        entry = Access.getResourceCacheEntry(loaderCache, cs1);
        Assert.assertNotNull(entry);
        Assert.assertTrue(Access.getResourceCacheEntryURL(entry).toExternalForm().contains("ClassLoaderResourceCacheTest.d"));
        Assert.assertTrue(Access.getResourceCacheEntryIndex(entry) >= 1);

        entry = Access.getResourceCacheEntry(loaderCache, cs2);
        Assert.assertNotNull(entry);
        Assert.assertNull(Access.getResourceCacheEntryURL(entry));
        Assert.assertEquals(Access.getResourceCacheEntryIndex(entry), -1);

        entry = Access.getResourceCacheEntry(loaderCache, cs3);
        Assert.assertNotNull(entry);
        Assert.assertTrue(Access.getResourceCacheEntryURL(entry).toExternalForm().contains("lib/testng-"));
        Assert.assertTrue(Access.getResourceCacheEntryIndex(entry) >= 2);

        // Try to access the three resources again.

        res = loader.getResource(cs1);
        Assert.assertNotNull(res);
        Assert.assertTrue(res.toExternalForm().contains("ClassLoaderResourceCacheTest.d"));

        res = loader.getResource(cs2);
        Assert.assertNull(res);

        res = loader.getResource(cs3);
        Assert.assertNotNull(res);
        Assert.assertTrue(res.toExternalForm().contains("lib/testng-"));

        // Recheck cache entries.

        Assert.assertEquals(Access.getResourceUrlCacheMap(loaderCache).size(), 3);
    }

    @Test
    public void testLoadClassFastException() {
        String cs1 = ClassLoaderResourceCacheTest.class.getName();
        String cs2 = "com.huawei.Nonexistent";
        String cs3 = "org.testng.Assert";
        Object entry;

        Class<?> c1 = null;
        Class<?> c2 = null;
        Class<?> c3 = null;

        // Try to load three classes.

        Assert.assertEquals(Access.getClassNotFoundExceptionCacheMap(loaderCache).size(), 0);

        try {
            c1 = loader.loadClass(cs1);
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
            Assert.fail();
        }

        try {
            c2 = loader.loadClass(cs2);
            Assert.fail();
        } catch (ClassNotFoundException ignored) {
        }

        try {
            c3 = loader.loadClass(cs3);
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
            Assert.fail();
        }

        Assert.assertNotNull(c1);
        Assert.assertNull(c2);
        Assert.assertNotNull(c3);

        Assert.assertEquals(Access.getClassNotFoundExceptionCacheMap(loaderCache).size(), 1);

        // Retry to load three classes.

        try {
            c1 = loader.loadClass(cs1);
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
            Assert.fail();
        }

        try {
            c2 = loader.loadClass(cs2);
            Assert.fail();
        } catch (ClassNotFoundException ignored) {
        }

        try {
            c3 = loader.loadClass(cs3);
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
            Assert.fail();
        }

        Assert.assertEquals(Access.getClassNotFoundExceptionCacheMap(loaderCache).size(), 1);
        Assert.assertEquals(Access.getResourceUrlCacheMap(loaderCache).size(), 0);
    }
}

class Access {
    private static final Field loaderCacheGetter;
    private static final Field resourceUrlCacheMapGetter;
    private static final Field classNotFoundExceptionCacheMapGetter;
    private static final MethodHandle resourceCacheGetter;
    private static final MethodHandle resourceCacheEntryURLGetter;
    private static final MethodHandle resourceCacheEntryIndexGetter;

    static {
        try {
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            Class<?> clrClass = Class.forName("java.net.ClassLoaderResourceCache");
            Class<?> rceClass = Class.forName("java.net.ClassLoaderResourceCache$ResourceCacheEntry");

            loaderCacheGetter = URLClassLoader.class.getDeclaredField("loaderCache");
            loaderCacheGetter.setAccessible(true);

            resourceUrlCacheMapGetter = clrClass.getDeclaredField("resourceUrlCache");
            resourceUrlCacheMapGetter.setAccessible(true);

            classNotFoundExceptionCacheMapGetter = clrClass.getDeclaredField("classNotFoundExceptionCache");
            classNotFoundExceptionCacheMapGetter.setAccessible(true);

            Method m;

            m = clrClass.getDeclaredMethod("getResourceCache", String.class);
            m.setAccessible(true);
            resourceCacheGetter = lookup.unreflect(m);

            m = rceClass.getMethod("getURL");
            m.setAccessible(true);
            resourceCacheEntryURLGetter = lookup.unreflect(m);

            m = rceClass.getMethod("getIndex");
            m.setAccessible(true);
            resourceCacheEntryIndexGetter = lookup.unreflect(m);
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException(e);
        }
    }

    public static Object getClassLoaderResourceCache(URLClassLoader loader) {
        try {
            return loaderCacheGetter.get(loader);
        } catch (IllegalAccessException e) {
            e.printStackTrace();
            throw new RuntimeException(e);
        }
    }

    public static Map<String, Object> getResourceUrlCacheMap(Object loaderCache) {
        try {
            @SuppressWarnings("unchecked")
            Map<String, Object> res = (Map<String, Object>) resourceUrlCacheMapGetter.get(loaderCache);
            return res;
        } catch (IllegalAccessException e) {
            e.printStackTrace();
            throw new RuntimeException(e);
        }
    }

    public static Map<String, ClassNotFoundException> getClassNotFoundExceptionCacheMap(Object loaderCache) {
        try {
            @SuppressWarnings("unchecked")
            Map<String, ClassNotFoundException> res = (Map<String, ClassNotFoundException>) classNotFoundExceptionCacheMapGetter.get(loaderCache);
            return res;
        } catch (IllegalAccessException e) {
            e.printStackTrace();
            throw new RuntimeException(e);
        }
    }

    public static Object getResourceCacheEntry(Object loaderCache, String name) {
        try {
            return resourceCacheGetter.invoke(loaderCache, name);
        } catch (Throwable throwable) {
            throwable.printStackTrace();
            throw new RuntimeException(throwable);
        }
    }

    public static URL getResourceCacheEntryURL(Object resourceCache) {
        try {
            return (URL) resourceCacheEntryURLGetter.invoke(resourceCache);
        } catch (Throwable throwable) {
            throwable.printStackTrace();
            throw new RuntimeException(throwable);
        }
    }

    public static int getResourceCacheEntryIndex(Object resourceCache) {
        try {
            return (int) resourceCacheEntryIndexGetter.invoke(resourceCache);
        } catch (Throwable throwable) {
            throwable.printStackTrace();
            throw new RuntimeException(throwable);
        }
    }
}
