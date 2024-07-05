/*                                                                                                                                           
 * Copyright (c) 2023, Huawei Technologies Co., Ltd. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Huawei designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Huawei in the LICENSE file that accompanied this code.
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
 * Please visit https://gitee.com/openeuler/bgmprovider if you need additional
 * information or have any questions.
 */

package org.openeuler.security.openssl;

import javax.crypto.KeyGeneratorSpi;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidParameterException;
import java.security.SecureRandom;
import java.security.spec.AlgorithmParameterSpec;

public class KAESM4KeyGenerator extends KeyGeneratorSpi {
    private byte[] key;
    private int keySize = 16; // default keysize (in number of bytes)
    private SecureRandom random;

    @Override
    protected void engineInit(SecureRandom random) {
        this.random = random;
    }

    @Override
    protected void engineInit(AlgorithmParameterSpec params, SecureRandom random)
            throws InvalidAlgorithmParameterException {
        throw new InvalidAlgorithmParameterException
                ("SM4 key generation does not take any parameters");
    }

    @Override
    protected void engineInit(int keysize, SecureRandom random) {
        if (keysize != 128) {
            throw new InvalidParameterException("SM4 requires a 128 bit key");
        }
        this.keySize = keysize / 8;
        engineInit(random);
    }

    private static class SecureRandomHolder {
        static final SecureRandom RANDOM = new SecureRandom();
    }

    @Override
    protected SecretKey engineGenerateKey() {
        key = new byte[keySize];
        if (random == null) {
            random = SecureRandomHolder.RANDOM;
        }
        random.nextBytes(key);
        return new SecretKeySpec(key, "SM4");
    }
}
