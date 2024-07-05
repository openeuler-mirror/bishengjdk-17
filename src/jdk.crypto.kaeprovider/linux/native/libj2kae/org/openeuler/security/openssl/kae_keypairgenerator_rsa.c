/*
 * Copyright (c) 2023, Huawei Technologies Co., Ltd. All rights reserved.
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

#include <stdbool.h>
#include <openssl/rsa.h>
#include "kae_log.h"
#include "kae_util.h"
#include "kae_exception.h"
#include "org_openeuler_security_openssl_KAERSAKeyPairGenerator.h"
#define KAE_RSA_PARAM_SIZE 8

// rsa param index
typedef enum RSAParamIndex {
    rsaN = 0,
    rsaE,
    rsaD,
    rsaP,
    rsaQ,
    rsaDmp1,
    rsaDmq1,
    rsaIqmp
} RSAParamIndex;

// rsa param name array
static const char* rsaParamNames[] = {"n", "e", "d", "p", "q", "dmp1", "dmq1", "iqmp"};

// rsa get rsa param function list
static const BIGNUM* (* GetRSAParamFunctionList[])(const RSA*) = {
    RSA_get0_n,
    RSA_get0_e,
    RSA_get0_d,
    RSA_get0_p,
    RSA_get0_q,
    RSA_get0_dmp1,
    RSA_get0_dmq1,
    RSA_get0_iqmp
};

/*
 * New RSA and generate rsa key, follow the steps below
 * step 1.New RSA
 * step 2.Convert publicExponent to BIGNUM
 * step 3.Generate rsa key, and all key information is stored in RSA
 */
static RSA* NewRSA(JNIEnv* env, jint keySize, jbyteArray publicExponent) {
    ENGINE* kaeEngine = GetEngineByAlgorithmIndex(RSA_INDEX);
    KAE_TRACE("NewRSA: kaeEngine => %p", kaeEngine);

    // new rsa
    RSA* rsa = RSA_new_method(kaeEngine);
    if (rsa == NULL) {
        KAE_ThrowFromOpenssl(env, "RSA_new_method", KAE_ThrowRuntimeException);
        return NULL;
    }

    // convert publicExponent to BIGNUM
    BIGNUM* exponent = KAE_GetBigNumFromByteArray(env, publicExponent);
    if (exponent == NULL) {
        return NULL;
    }

    // generate rsa key
    int result_code = RSA_generate_key_ex(rsa, keySize, exponent, NULL);
    KAE_ReleaseBigNumFromByteArray(exponent);
    if (result_code <= 0) {
        RSA_free(rsa);
        KAE_ThrowFromOpenssl(env, "RSA_generate_key_ex", KAE_ThrowRuntimeException);
        return NULL;
    }
    return rsa;
}

/*
 * release RSA
 */
static void ReleaseRSA(RSA* rsa) {
    if (rsa != NULL) {
        RSA_free(rsa);
    }
}

/*
 * Set rsa key param, follow the steps below
 * step 1. Get rsa param value
 * step 2. Convert paramValue (BIGNUM) to jbyteArray
 * step 3. Set the rsa param to the param array
 */
static bool SetRSAKeyParam(JNIEnv* env, RSA* rsa, jobjectArray params, RSAParamIndex rsaParamIndex) {
    // get rsa param value
    const BIGNUM* rsaParamValue = GetRSAParamFunctionList[rsaParamIndex](rsa);
    if (rsaParamValue == NULL) {
        return false;
    }

    // Convert paramValue to jbyteArray
    jbyteArray param = KAE_GetByteArrayFromBigNum(env, rsaParamValue);
    if (param == NULL) {
        return false;
    }

    // Set the rsa param to the param array
    (*env)->SetObjectArrayElement(env, params, rsaParamIndex, param);
    return true;
}

/*
 * New rsa key params, follow the steps below
 * step 1. New rsa key param array
 * step 2. Set rsa key param
 */
static jobjectArray NewRSAKeyParams(JNIEnv* env, RSA* rsa) {
    // new param array
    jclass byteArrayClass = (*env)->FindClass(env, "[B");
    jobjectArray params = (*env)->NewObjectArray(env, KAE_RSA_PARAM_SIZE, byteArrayClass, NULL);
    if (params == NULL) {
        KAE_ThrowOOMException(env, "failed to allocate array");
        return NULL;
    }

    // set rsa key param
    RSAParamIndex paramIndex;
    for (paramIndex = rsaN; paramIndex <= rsaIqmp; paramIndex++) {
        if (!SetRSAKeyParam(env, rsa, params, paramIndex)) {
            return NULL;
        }
    }
    return params;
}

/*
 * Class:     org_openeuler_security_openssl_KAERSAKeyPairGenerator
 * Method:    nativeGenerateKeyPair
 * Signature: (I[B)[[B
 */
JNIEXPORT jobjectArray JNICALL Java_org_openeuler_security_openssl_KAERSAKeyPairGenerator_nativeGenerateKeyPair (
    JNIEnv* env, jclass cls, jint keySize, jbyteArray publicExponent) {
    if (publicExponent == NULL) {
        return NULL;
    }

    // new RSA
    RSA* rsa = NewRSA(env, keySize, publicExponent);
    if (rsa == NULL) {
        return NULL;
    }

    // new RSA Key Parameters
    jobjectArray rsaParm = NewRSAKeyParams(env, rsa);

    // release rsa
    ReleaseRSA(rsa);
    return rsaParm;
}