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

import jdk.internal.loader.URLClassPath;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Method;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.nio.file.attribute.PosixFilePermission;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

import sun.security.action.GetBooleanAction;
import sun.security.action.GetIntegerAction;
import sun.security.action.GetPropertyAction;

/**
 * We cache the mapping of "resource name -> resource url" to
 * accelerate resource finding.
 * Used only by {@link java.net.URLClassLoader}
 */
final class ClassLoaderResourceCache {
    public static final int NO_IDX = -1;
    public static final String NULL_OBJ = "<null>";

    private static final boolean ENABLE_CACHE = GetBooleanAction.privilegedGetProperty("jdk.jbooster.clrcache.enable");
    private static final String DUMP_CACHE_FILE = GetPropertyAction.privilegedGetProperty("jdk.jbooster.clrcache.dump");
    private static final String LOAD_CACHE_FILE = GetPropertyAction.privilegedGetProperty("jdk.jbooster.clrcache.load");
    private static final int MAX_CACHE_SIZE = GetIntegerAction.privilegedGetProperty("jdk.jbooster.clrcache.size", 2000);
    private static final boolean VERBOSE_CACHE_FILE = GetBooleanAction.privilegedGetProperty("jdk.jbooster.clrcache.verbose");

    private static final List<ClassLoaderResourceCache> cachesToDump;
    private static final Map<ClassLoaderKey, LoadedCacheData> cachesToLoad;

    static {
        if ((DUMP_CACHE_FILE != null || LOAD_CACHE_FILE != null) && !ENABLE_CACHE) {
            System.err.println("Please set loader.cache.enable to true!");
            System.exit(1);
        }

        if (DUMP_CACHE_FILE != null) {
            cachesToDump = Collections.synchronizedList(new ArrayList<>());
            registerDumpShutdownHookPrivileged();
        } else {
            cachesToDump = null;
        }

        if (LOAD_CACHE_FILE != null) {
            // This map will never be modified after its initialization.
            // So it doesn't have to be thread-safe.
            cachesToLoad = new HashMap<>();
            loadPrivileged(LOAD_CACHE_FILE);
        } else {
            cachesToLoad = null;
        }
    }

    public static boolean isEnabled() {
        return ENABLE_CACHE;
    }

    public static ClassLoaderResourceCache createIfEnabled(URLClassLoader holder, URL[] urls) {
        return isEnabled() ? new ClassLoaderResourceCache(holder, urls) : null;
    }

    public static URL findResource(URLClassPath ucp, String name, boolean check,
                                   String cachedURLString, int cachedIndex,
                                   int[] resIndex) {
        return URLClassPathUtil.findResource(ucp, name, check, cachedURLString, cachedIndex, resIndex);
    }

    public static URL findResource(URLClassPath ucp, String name, boolean check, int[] resIndex) {
        return URLClassPathUtil.findResource(ucp, name, check, resIndex);
    }

    @SuppressWarnings("removal")
    private static void registerDumpShutdownHookPrivileged() {
        AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
            Runtime.getRuntime().addShutdownHook(new Thread(() -> {
                AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
                    dump(DUMP_CACHE_FILE);
                    return null;
                });
            }));
            return null;
        });
    }

    @SuppressWarnings("removal")
    private static void loadPrivileged(String filePath) {
        AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
            load(filePath);
            return null;
        });
    }

    /**
     * Dump all class loader resource caches to a file.
     *
     * @param filePath The file to store the cache
     */
    private static void dump(String filePath) {
        File file = new File(filePath);
        // Do not re-dump if the cache file already exists.
        if (file.isFile()) {
            return;
        }

        // Treat the tmp file as a file lock (see JBoosterManager::calc_tmp_cache_path()).
        String tmpFilePath = filePath.concat(".tmp");
        File tmpFile = new File(tmpFilePath);
        try {
            // Create the tmp file. Skip dump if the tmp file already exists
            // (meaning someone else is dumping) or fails to be created.
            if (!tmpFile.createNewFile()) {
                return;
            }

            // Double check if the cache file already exists.
            if (file.isFile()) {
                // Release the tmp file lock.
                tmpFile.delete();
                return;
            }
        } catch (IOException e) {
            e.printStackTrace();
            return;
        }

        try (PrintWriter writer = new PrintWriter(tmpFile)) {
            // synchronized to avoid ConcurrentModificationException
            synchronized (cachesToDump) {
                for (ClassLoaderResourceCache cache : cachesToDump) {
                    writeCache(writer, cache);
                }
            }
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            return;
        }

        boolean renameSuccessful = false;
        try {
            // Do not rename if the target file already exists.
            // Theoretically, the target file cannot exist.
            if (!file.isFile()) {
                Set<PosixFilePermission> perms = new HashSet<PosixFilePermission>();
                perms.add(PosixFilePermission.OWNER_READ);
                Files.setPosixFilePermissions(tmpFile.toPath(), perms);
                Files.move(tmpFile.toPath(), file.toPath(), StandardCopyOption.ATOMIC_MOVE);
                renameSuccessful = true;
            }
        } catch (AtomicMoveNotSupportedException e) {
            System.err.println("The file system does not support atomic move in the same dir?");
            e.printStackTrace();
        } catch (FileAlreadyExistsException e) {
            System.err.println("The file already exists? Should be a bug.");
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (!renameSuccessful) {
                // Release the tmp file lock if the renaming fails.
                tmpFile.delete();
            }
        }
    }

    private static void writeCache(PrintWriter writer, ClassLoaderResourceCache cache) {
        String holderClassName = cache.holderClassName;
        String holderName = cache.holderName;
        writer.println("L|" + holderClassName
                + "|" + (holderName == null ? NULL_OBJ : holderName)
                + "|" + cache.originalURLsHash);
        synchronized (cache.resourceUrlCache) {
            for (Map.Entry<String, ResourceCacheEntry> e : cache.resourceUrlCache.entrySet()) {
                String resourceName = e.getKey();
                ResourceCacheEntry entry = e.getValue();
                if (entry.isFound()) {
                    writer.println("E|" + resourceName + "|" + entry.getIndex()
                            + (VERBOSE_CACHE_FILE ? ("|" + entry.getURL().toExternalForm()) : ""));
                } else {
                    writer.println("E|" + resourceName + "|" + NO_IDX + (VERBOSE_CACHE_FILE ? ("|" + NULL_OBJ) : ""));
                }
            }
        }
    }

    /**
     * Load all class loader resource caches from a file.
     *
     * @param filePath The file that stores the cache
     */
    private static void load(String filePath) {
        try (FileReader fr = new FileReader(filePath);
             BufferedReader br = new BufferedReader(fr)) {
            String line;
            LoadedCacheData cacheData = null;
            while ((line = br.readLine()) != null) {
                if (line.startsWith("L|")) {
                    ClassLoaderKey key = readCacheLoader(line);
                    cacheData = new LoadedCacheData();
                    cachesToLoad.put(key, cacheData);
                } else if (line.startsWith("E|")) {
                    readCacheEntry(line, cacheData);
                } else {
                    System.err.println("Unknown line: " + line);
                    System.exit(1);
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
            System.exit(1);
        }
    }

    private static ClassLoaderKey readCacheLoader(String line) {
        String[] sp = line.split("\\|");
        if (sp.length != 4) {
            System.err.println("Unknown line: " + line);
            System.exit(1);
        }
        return new ClassLoaderKey(sp[1], NULL_OBJ.equals(sp[2]) ? null : sp[2], Integer.parseInt(sp[3]));
    }

    private static void readCacheEntry(String line, LoadedCacheData cacheData) {
        String[] sp = line.split("\\|", VERBOSE_CACHE_FILE ? 4 : 3);
        if (sp.length != (VERBOSE_CACHE_FILE ? 4 : 3)) {
            System.err.println("Unknown line: " + line);
            System.exit(1);
        }
        int idx = Integer.parseInt(sp[2]);
        if (idx == NO_IDX && (VERBOSE_CACHE_FILE ? NULL_OBJ.equals(sp[3]) : true)) {
            cacheData.addLoadedResourceCacheEntry(sp[1], null, NO_IDX);
        } else {
            cacheData.addLoadedResourceCacheEntry(sp[1], (VERBOSE_CACHE_FILE ? sp[3] : null), idx);
        }
    }

    private static <K, V> Map<K, V> createCacheMap() {
        return Collections.synchronizedMap(new LinkedHashMap<>(MAX_CACHE_SIZE, 0.75f, true) {
            @Override
            protected boolean removeEldestEntry(Map.Entry<K, V> eldest) {
                return size() >= MAX_CACHE_SIZE;
            }
        });
    }

    private static int fastHashOfURLs(URL[] urls) {
        if (urls.length == 0) return 0;
        return urls.length ^ urls[0].hashCode();
    }

    private final String holderClassName;
    private final String holderName;

    private final int originalURLsHash;

    private final Map<String, ResourceCacheEntry> resourceUrlCache;

    // Stores the cached data loaded form the cache file. We use the
    // index to find the url of the resource quickly. We didn't choose
    // to generate the url from the url string and just put it into
    // the resourceUrlCache because (1) creating a url costs much time;
    // (2) we'd better check that our cache entry is correct.
    private final Map<String, LoadedResourceCacheEntry> loadedResourceUrlCache;

    private final Map<String, ClassNotFoundException> classNotFoundExceptionCache;

    private ClassLoaderResourceCache(URLClassLoader holder, URL[] urls) {
        this.holderClassName = holder.getClass().getName();
        this.holderName = holder.getName();
        this.originalURLsHash = fastHashOfURLs(urls);
        this.resourceUrlCache = createCacheMap();
        this.classNotFoundExceptionCache = createCacheMap();

        Map<String, LoadedResourceCacheEntry> loadedMap = Collections.emptyMap();
        if (cachesToLoad != null) {
            ClassLoaderKey key = new ClassLoaderKey(holderClassName, holderName, originalURLsHash);
            LoadedCacheData loadedCacheData = cachesToLoad.get(key);
            if (loadedCacheData != null) {
                loadedMap = loadedCacheData.getLoadedResourceUrlCache();
            }
        }
        this.loadedResourceUrlCache = loadedMap;

        if (cachesToDump != null) {
            cachesToDump.add(this);
        }
    }

    /**
     * Throws the exception if cached.
     *
     * @param name the resource name
     * @throws ClassNotFoundException if the exception is cached
     */
    public void fastClassNotFoundException(String name) throws ClassNotFoundException {
        ClassNotFoundException classNotFoundException = classNotFoundExceptionCache.get(name);
        if (classNotFoundException != null) {
            throw classNotFoundException;
        }
    }

    /**
     * Puts the cache.
     *
     * @param name      the resource name
     * @param exception the value to cache
     */
    public void cacheClassNotFoundException(String name, ClassNotFoundException exception) {
        classNotFoundExceptionCache.put(name, exception);
    }

    /**
     * Gets the cache loaded from a file.
     *
     * @param name the resource name
     * @return the cached value
     */
    public LoadedResourceCacheEntry getLoadedResourceCache(String name) {
        return loadedResourceUrlCache.get(name);
    }

    /**
     * Gets the cache.
     *
     * @param name the resource name
     * @return the cached value
     */
    public ResourceCacheEntry getResourceCache(String name) {
        return resourceUrlCache.get(name);
    }

    /**
     * Puts the cache.
     *
     * @param name the resource name
     * @param url  the url to cache
     * @param idx  the index of url to cache
     */
    public void cacheResourceUrl(String name, URL url, int idx) {
        resourceUrlCache.put(name, new ResourceCacheEntry(url, url == null ? NO_IDX : idx));
    }

    /**
     * Clears the caches. No call site yet.
     */
    public void clearCache() {
        classNotFoundExceptionCache.clear();
        resourceUrlCache.clear();
    }

    /**
     * The key to identify a class loader.
     */
    private static class ClassLoaderKey {
        private final String className;
        private final String name;
        private final int urlsHash;

        public ClassLoaderKey(String className, String name, int urlsHash) {
            this.className = className;
            this.name = name;
            this.urlsHash = urlsHash;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (o instanceof ClassLoaderKey that) {
                return Objects.equals(className, that.className)
                        && Objects.equals(name, that.name)
                        && Objects.equals(urlsHash, that.urlsHash);
            }
            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(className, name, urlsHash);
        }
    }

    /**
     * The cached resource info.
     */
    public static class ResourceCacheEntry {
        private final URL url;
        private final int index;

        public ResourceCacheEntry(URL url, int index) {
            this.url = url;
            this.index = index;
        }

        public boolean isFound() {
            return url != null;
        }

        public URL getURL() {
            return url;
        }

        public int getIndex() {
            return index;
        }
    }

    public static class LoadedResourceCacheEntry {
        private final String urlString;
        private final int index;

        public LoadedResourceCacheEntry(String urlString, int index) {
            this.urlString = urlString;
            this.index = index;
        }

        public String getURLString() {
            return urlString;
        }

        public int getIndex() {
            return index;
        }
    }

    private static class LoadedCacheData {
        private final Map<String, LoadedResourceCacheEntry> loadedResourceUrlCache;

        public LoadedCacheData() {
            this.loadedResourceUrlCache = new HashMap<>();
        }

        public Map<String, LoadedResourceCacheEntry> getLoadedResourceUrlCache() {
            return loadedResourceUrlCache;
        }

        public void addLoadedResourceCacheEntry(String name, String urlString, int index) {
            loadedResourceUrlCache.put(name, new LoadedResourceCacheEntry(urlString, index));
        }
    }
}

/**
 * We don't want to add new public methods in URLClassPath. So we add two
 * private method (findResourceWithIndex) and use method handle to invoke
 * them.
 */
@SuppressWarnings("removal")
class URLClassPathUtil {
    private static final MethodHandle resourceFinder1;
    private static final MethodHandle resourceFinder2;

    static {
        MethodHandle mh1 = null;
        MethodHandle mh2 = null;
        try {
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            Method m1 = URLClassPath.class.getDeclaredMethod("findResourceWithIndex", String.class, boolean.class, String.class, int.class, int[].class);
            Method m2 = URLClassPath.class.getDeclaredMethod("findResourceWithIndex", String.class, boolean.class, int[].class);
            AccessController.doPrivileged(
                    (PrivilegedAction<Void>) () -> {
                        m1.setAccessible(true);
                        m2.setAccessible(true);
                        return null;
                    });
            mh1 = lookup.unreflect(m1);
            mh2 = lookup.unreflect(m2);
        } catch (NoSuchMethodException | IllegalAccessException e) {
            e.printStackTrace();
            System.exit(1);
        }
        resourceFinder1 = mh1;
        resourceFinder2 = mh2;
    }

    public static URL findResource(URLClassPath ucp, String name, boolean check,
                                   String cachedURLString, int cachedIndex,
                                   int[] resIndex) {
        try {
            return (URL) resourceFinder1.invoke(ucp, name, check, cachedURLString, cachedIndex, resIndex);
        } catch (Throwable throwable) {
            throwable.printStackTrace();
            System.exit(1);
        }
        return null;
    }

    public static URL findResource(URLClassPath ucp, String name, boolean check, int[] resIndex) {
        try {
            return (URL) resourceFinder2.invoke(ucp, name, check, resIndex);
        } catch (Throwable throwable) {
            throwable.printStackTrace();
            System.exit(1);
        }
        return null;
    }
}
