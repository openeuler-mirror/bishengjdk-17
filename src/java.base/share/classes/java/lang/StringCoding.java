/*
 * Copyright (c) 2000, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

package java.lang;

import jdk.internal.vm.annotation.ForceInline;
import jdk.internal.vm.annotation.IntrinsicCandidate;

/**
 * Utility class for string encoding and decoding.
 */
class StringCoding {

    private StringCoding() { }

    @ForceInline
    static boolean isNotContinuation(int b) {
        return (b & 0xc0) != 0x80;
    }

    @ForceInline
    static boolean isMalformed3(int b1, int b2, int b3) {
        return (b1 == (byte)0xe0 && (b2 & 0xe0) == 0x80) ||
                (b2 & 0xc0) != 0x80 || (b3 & 0xc0) != 0x80;
    }

    @ForceInline
    static boolean isMalformed3_2(int b1, int b2) {
        return (b1 == (byte)0xe0 && (b2 & 0xe0) == 0x80) ||
                (b2 & 0xc0) != 0x80;
    }

    @ForceInline
    static boolean isMalformed4(int b2, int b3, int b4) {
        return (b2 & 0xc0) != 0x80 || (b3 & 0xc0) != 0x80 ||
                (b4 & 0xc0) != 0x80;
    }

    @ForceInline
    static boolean isMalformed4_2(int b1, int b2) {
        return (b1 == 0xf0 && (b2 < 0x90 || b2 > 0xbf)) ||
                (b1 == 0xf4 && (b2 & 0xf0) != 0x80) ||
                (b2 & 0xc0) != 0x80;
    }

    @ForceInline
    static char decode2(int b1, int b2) {
        return (char)(((b1 << 6) ^ b2) ^
                (((byte) 0xC0 << 6) ^
                        ((byte) 0x80 << 0)));
    }

    @ForceInline
    static char decode3(int b1, int b2, int b3) {
        return (char)((b1 << 12) ^
                (b2 <<  6) ^
                (b3 ^
                        (((byte) 0xE0 << 12) ^
                                ((byte) 0x80 <<  6) ^
                                ((byte) 0x80 <<  0))));
    }

    @ForceInline
    static int decode4(int b1, int b2, int b3, int b4) {
        return ((b1 << 18) ^
                (b2 << 12) ^
                (b3 <<  6) ^
                (b4 ^
                        (((byte) 0xF0 << 18) ^
                                ((byte) 0x80 << 12) ^
                                ((byte) 0x80 <<  6) ^
                                ((byte) 0x80 <<  0))));
    }

    @IntrinsicCandidate
    public static boolean hasNegatives(byte[] ba, int off, int len) {
        for (int i = off; i < off + len; i++) {
            if (ba[i] < 0) {
                return true;
            }
        }
        return false;
    }

    @IntrinsicCandidate
    public static int implEncodeISOArray(byte[] sa, int sp,
                                         byte[] da, int dp, int len) {
        int i = 0;
        for (; i < len; i++) {
            char c = StringUTF16.getChar(sa, sp++);
            if (c > '\u00FF')
                break;
            da[dp++] = (byte)c;
        }
        return i;
    }

    @IntrinsicCandidate
    public static int implEncodeAsciiArray(char[] sa, int sp,
                                           byte[] da, int dp, int len)
    {
        int i = 0;
        for (; i < len; i++) {
            char c = sa[sp++];
            if (c >= '\u0080')
                break;
            da[dp++] = (byte)c;
        }
        return i;
    }

    @IntrinsicCandidate
    public static int implEncodeUtf8fromUtf16(byte[] sa, int sp, byte[] da, int dp, int sl)
    {
        while (sp < sl) {
            // ascii fast loop
            char c = StringUTF16.getChar(sa, sp);
            if (c >= '\u0080') {
                break;
            }
            da[dp++] = (byte)c;
            sp++;
        }
        while (sp < sl) {
            char c = StringUTF16.getChar(sa, sp++);
            if (c < 0x80) {
                da[dp++] = (byte)c;
            } else if (c < 0x800) {
                da[dp++] = (byte)(0xc0 | (c >> 6));
                da[dp++] = (byte)(0x80 | (c & 0x3f));
            } else if (Character.isSurrogate(c)) {
                int uc = -1;
                char c2;
                if (Character.isHighSurrogate(c) && sp < sl &&
                        Character.isLowSurrogate(c2 = StringUTF16.getChar(sa, sp))) {
                    uc = Character.toCodePoint(c, c2);
                }
                if (uc < 0) {
                    return -(sp - 1); // in case of unmappable, return index of unmappable as negative value
                } else {
                    da[dp++] = (byte)(0xf0 | ((uc >> 18)));
                    da[dp++] = (byte)(0x80 | ((uc >> 12) & 0x3f));
                    da[dp++] = (byte)(0x80 | ((uc >>  6) & 0x3f));
                    da[dp++] = (byte)(0x80 | (uc & 0x3f));
                    sp++; // 2 chars
                }
            } else {
                // 3 bytes, 16 bits
                da[dp++] = (byte)(0xe0 | ((c >> 12)));
                da[dp++] = (byte)(0x80 | ((c >>  6) & 0x3f));
                da[dp++] = (byte)(0x80 | (c & 0x3f));
            }
        }
        // if OK - return number of bytes in destination
        return dp;
    }

    /**
     * Processes characters from the source byte array and writes the
     * decoded result into the destination array.
     * <p>
     * The returned value represents the number of characters produced
     * by this invocation only, independent of the source or destination
     * offsets.
     *
     * @return the number of characters produced by this method,
     *         or a negative value if an error occurs
     */
    @IntrinsicCandidate
    public static int implDecodeUtf8ToUtf16(byte[] src, int sp, byte[] dst, int dp, int sl) {
        int staDp = dp;
        while (sp < sl) {
            int b1 = src[sp++];
            if (b1 >= 0) {
                StringUTF16.putChar(dst, dp++, (char) b1);
            } else if ((b1 >> 5) == -2 && (b1 & 0x1e) != 0) {
                if (sp < sl) {
                    int b2 = src[sp++];
                    if (isNotContinuation(b2)) {
                        // fallback to original path
                        return -(sp - 1);
                    } else {
                        StringUTF16.putChar(dst, dp++, decode2(b1, b2));
                    }
                    continue;
                }
                // fallback to original path
                return -(sp);
            } else if ((b1 >> 4) == -2) {
                if (sp + 1 < sl) {
                    int b2 = src[sp++];
                    int b3 = src[sp++];
                    if (isMalformed3(b1, b2, b3)) {
                        // fallback to original path
                        return -(sp - 3);
                    } else {
                        char c = decode3(b1, b2, b3);
                        if (Character.isSurrogate(c)) {
                            // fallback to original path
                            return -(sp - 3);
                        } else {
                            StringUTF16.putChar(dst, dp++, c);
                        }
                    }
                    continue;
                }
                if (sp < sl && isMalformed3_2(b1, src[sp])) {
                    // fallback to original path
                    return -(sp - 1);
                }
                // fallback to original path
                return -(sp - 1);
            } else if ((b1 >> 3) == -2) {
                if (sp + 2 < sl) {
                    int b2 = src[sp++];
                    int b3 = src[sp++];
                    int b4 = src[sp++];
                    int uc = decode4(b1, b2, b3, b4);
                    if (isMalformed4(b2, b3, b4) ||
                            !Character.isSupplementaryCodePoint(uc)) { // shortest form check
                        // fallback to original path
                        return -(sp - 4);
                    } else {
                        StringUTF16.putChar(dst, dp++, Character.highSurrogate(uc));
                        StringUTF16.putChar(dst, dp++, Character.lowSurrogate(uc));
                    }
                    continue;
                }
                b1 &= 0xff;
                if (b1 > 0xf4 || sp < sl && isMalformed4_2(b1, src[sp] & 0xff)) {
                    // fallback to original path
                    return -(sp - 1);
                }
                // fallback to original path
                return -(sp - 1);
            } else {
                // fallback to original path
                return -(sp - 1);
            }
        }
        return dp - staDp;
    }
}
