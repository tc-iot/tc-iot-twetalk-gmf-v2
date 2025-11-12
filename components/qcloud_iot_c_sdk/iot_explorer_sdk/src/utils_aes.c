/**
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2022 THL A29 Limited, a Tencent company.All rights reserved.
 *
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file utils_aes.c
 * @brief @ref mbedtls/aes.c
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-10-24
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-10-24 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "utils_aes.h"

#ifdef ENABLE_AUTH_NO_TLS

/**
 * \brief The AES context-type definition.
 */
typedef struct {
    int       nr; /*!< The number of rounds. */
    uint32_t *rk; /*!< AES round keys. */
    uint32_t  buf[68];
} utils_aes_context;

/*
 * 32-bit integer manipulation macros (little endian)
 */
#define GET_UINT32_LE(n, b, i)                                                                        \
    {                                                                                                 \
        (n) = ((uint32_t)(b)[(i)]) | ((uint32_t)(b)[(i) + 1] << 8) | ((uint32_t)(b)[(i) + 2] << 16) | \
              ((uint32_t)(b)[(i) + 3] << 24);                                                         \
    }

#define PUT_UINT32_LE(n, b, i)                              \
    {                                                       \
        (b)[(i)]     = (unsigned char)(((n)) & 0xFF);       \
        (b)[(i) + 1] = (unsigned char)(((n) >> 8) & 0xFF);  \
        (b)[(i) + 2] = (unsigned char)(((n) >> 16) & 0xFF); \
        (b)[(i) + 3] = (unsigned char)(((n) >> 24) & 0xFF); \
    }

/*
 * Forward S-box & tables
 */
static unsigned char FSb[256];
static uint32_t      FT0[256];

/*
 * Reverse S-box & tables
 */
static unsigned char RSb[256];
static uint32_t      RT0[256];

/*
 * Round constants
 */
static uint32_t RCON[10];

/*
 * Tables generation code
 */
#define ROTL8(x)  (((x) << 8) & 0xFFFFFFFF) | ((x) >> 24)
#define XTIME(x)  (((x) << 1) ^ (((x)&0x00000080) ? 0x0000001B : 0x00000000))
#define MUL(x, y) (((x) && (y)) ? pow[(log[(x)] + log[(y)]) % 255] : 0)

static int sg_aes_init_done = 0;

static void _aes_gen_tables(void)
{
    int i, x, y, z;
    int pow[256];
    int log[256];

    /*
     * compute pow and log tables over GF(2^8)
     */
    for (i = 0, x = 1; i < 256; i++) {
        pow[i] = x;
        log[x] = i;
        x      = (x ^ XTIME(x)) & 0xFF;
    }

    /*
     * calculate the round constants
     */
    for (i = 0, x = 1; i < 10; i++) {
        RCON[i] = (uint32_t)x;
        x       = XTIME(x) & 0xFF;
    }

    /*
     * generate the forward and reverse S-boxes
     */
    FSb[0x00] = 0x63;
    RSb[0x63] = 0x00;

    for (i = 1; i < 256; i++) {
        x = pow[255 - log[i]];

        y = x;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y ^ 0x63;

        FSb[i] = (unsigned char)x;
        RSb[x] = (unsigned char)i;
    }

    /*
     * generate the forward and reverse tables
     */
    for (i = 0; i < 256; i++) {
        x = FSb[i];
        y = XTIME(x) & 0xFF;
        z = (y ^ x) & 0xFF;

        FT0[i] = ((uint32_t)y) ^ ((uint32_t)x << 8) ^ ((uint32_t)x << 16) ^ ((uint32_t)z << 24);

        x = RSb[i];

        RT0[i] = ((uint32_t)MUL(0x0E, x)) ^ ((uint32_t)MUL(0x09, x) << 8) ^ ((uint32_t)MUL(0x0D, x) << 16) ^
                 ((uint32_t)MUL(0x0B, x) << 24);
    }
}

#undef ROTL8
#define ROTL8(x)  ((uint32_t)((x) << 8) + (uint32_t)((x) >> 24))
#define ROTL16(x) ((uint32_t)((x) << 16) + (uint32_t)((x) >> 16))
#define ROTL24(x) ((uint32_t)((x) << 24) + (uint32_t)((x) >> 8))

#define AES_RT0(idx) RT0[idx]
#define AES_RT1(idx) ROTL8(RT0[idx])
#define AES_RT2(idx) ROTL16(RT0[idx])
#define AES_RT3(idx) ROTL24(RT0[idx])

#define AES_FT0(idx) FT0[idx]
#define AES_FT1(idx) ROTL8(FT0[idx])
#define AES_FT2(idx) ROTL16(FT0[idx])
#define AES_FT3(idx) ROTL24(FT0[idx])

static int _utils_aes_setkey_enc(utils_aes_context *ctx, const unsigned char *key, unsigned int keybits)
{
    unsigned int i;
    uint32_t    *RK;

    switch (keybits) {
        case 128:
            ctx->nr = 10;
            break;
        case 192:
            ctx->nr = 12;
            break;
        case 256:
            ctx->nr = 14;
            break;
        default:
            return -1;
    }

    if (sg_aes_init_done == 0) {
        _aes_gen_tables();
        sg_aes_init_done = 1;
    }

    ctx->rk = RK = ctx->buf;

    for (i = 0; i < (keybits >> 5); i++) {
        GET_UINT32_LE(RK[i], key, i << 2);
    }

    switch (ctx->nr) {
        case 10:

            for (i = 0; i < 10; i++, RK += 4) {
                RK[4] = RK[0] ^ RCON[i] ^ ((uint32_t)FSb[(RK[3] >> 8) & 0xFF]) ^
                        ((uint32_t)FSb[(RK[3] >> 16) & 0xFF] << 8) ^ ((uint32_t)FSb[(RK[3] >> 24) & 0xFF] << 16) ^
                        ((uint32_t)FSb[(RK[3]) & 0xFF] << 24);

                RK[5] = RK[1] ^ RK[4];
                RK[6] = RK[2] ^ RK[5];
                RK[7] = RK[3] ^ RK[6];
            }
            break;

        case 12:

            for (i = 0; i < 8; i++, RK += 6) {
                RK[6] = RK[0] ^ RCON[i] ^ ((uint32_t)FSb[(RK[5] >> 8) & 0xFF]) ^
                        ((uint32_t)FSb[(RK[5] >> 16) & 0xFF] << 8) ^ ((uint32_t)FSb[(RK[5] >> 24) & 0xFF] << 16) ^
                        ((uint32_t)FSb[(RK[5]) & 0xFF] << 24);

                RK[7]  = RK[1] ^ RK[6];
                RK[8]  = RK[2] ^ RK[7];
                RK[9]  = RK[3] ^ RK[8];
                RK[10] = RK[4] ^ RK[9];
                RK[11] = RK[5] ^ RK[10];
            }
            break;

        case 14:

            for (i = 0; i < 7; i++, RK += 8) {
                RK[8] = RK[0] ^ RCON[i] ^ ((uint32_t)FSb[(RK[7] >> 8) & 0xFF]) ^
                        ((uint32_t)FSb[(RK[7] >> 16) & 0xFF] << 8) ^ ((uint32_t)FSb[(RK[7] >> 24) & 0xFF] << 16) ^
                        ((uint32_t)FSb[(RK[7]) & 0xFF] << 24);

                RK[9]  = RK[1] ^ RK[8];
                RK[10] = RK[2] ^ RK[9];
                RK[11] = RK[3] ^ RK[10];

                RK[12] = RK[4] ^ ((uint32_t)FSb[(RK[11]) & 0xFF]) ^ ((uint32_t)FSb[(RK[11] >> 8) & 0xFF] << 8) ^
                         ((uint32_t)FSb[(RK[11] >> 16) & 0xFF] << 16) ^ ((uint32_t)FSb[(RK[11] >> 24) & 0xFF] << 24);

                RK[13] = RK[5] ^ RK[12];
                RK[14] = RK[6] ^ RK[13];
                RK[15] = RK[7] ^ RK[14];
            }
            break;
    }

    return (0);
}

/*
 * AES key schedule (decryption)
 */
static int _utils_aes_setkey_dec(utils_aes_context *ctx, const unsigned char *key, unsigned int keybits)
{
    int               i, j, ret;
    utils_aes_context cty = {0};
    uint32_t         *RK;
    uint32_t         *SK;

    ctx->rk = RK = ctx->buf;

    /* Also checks keybits */
    if ((ret = _utils_aes_setkey_enc(&cty, key, keybits)) != 0) {
        return (ret);
    }

    ctx->nr = cty.nr;

    SK = cty.rk + cty.nr * 4;

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;

    for (i = ctx->nr - 1, SK -= 8; i > 0; i--, SK -= 8) {
        for (j = 0; j < 4; j++, SK++) {
            *RK++ = AES_RT0(FSb[(*SK) & 0xFF]) ^ AES_RT1(FSb[(*SK >> 8) & 0xFF]) ^ AES_RT2(FSb[(*SK >> 16) & 0xFF]) ^
                    AES_RT3(FSb[(*SK >> 24) & 0xFF]);
        }
    }

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    return (ret);
}

#define AES_FROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3)                                                           \
    do {                                                                                                     \
        (X0) = *RK++ ^ AES_FT0(((Y0)) & 0xFF) ^ AES_FT1(((Y1) >> 8) & 0xFF) ^ AES_FT2(((Y2) >> 16) & 0xFF) ^ \
               AES_FT3(((Y3) >> 24) & 0xFF);                                                                 \
                                                                                                             \
        (X1) = *RK++ ^ AES_FT0(((Y1)) & 0xFF) ^ AES_FT1(((Y2) >> 8) & 0xFF) ^ AES_FT2(((Y3) >> 16) & 0xFF) ^ \
               AES_FT3(((Y0) >> 24) & 0xFF);                                                                 \
                                                                                                             \
        (X2) = *RK++ ^ AES_FT0(((Y2)) & 0xFF) ^ AES_FT1(((Y3) >> 8) & 0xFF) ^ AES_FT2(((Y0) >> 16) & 0xFF) ^ \
               AES_FT3(((Y1) >> 24) & 0xFF);                                                                 \
                                                                                                             \
        (X3) = *RK++ ^ AES_FT0(((Y3)) & 0xFF) ^ AES_FT1(((Y0) >> 8) & 0xFF) ^ AES_FT2(((Y1) >> 16) & 0xFF) ^ \
               AES_FT3(((Y2) >> 24) & 0xFF);                                                                 \
    } while (0)

#define AES_RROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3)                                                           \
    do {                                                                                                     \
        (X0) = *RK++ ^ AES_RT0(((Y0)) & 0xFF) ^ AES_RT1(((Y3) >> 8) & 0xFF) ^ AES_RT2(((Y2) >> 16) & 0xFF) ^ \
               AES_RT3(((Y1) >> 24) & 0xFF);                                                                 \
                                                                                                             \
        (X1) = *RK++ ^ AES_RT0(((Y1)) & 0xFF) ^ AES_RT1(((Y0) >> 8) & 0xFF) ^ AES_RT2(((Y3) >> 16) & 0xFF) ^ \
               AES_RT3(((Y2) >> 24) & 0xFF);                                                                 \
                                                                                                             \
        (X2) = *RK++ ^ AES_RT0(((Y2)) & 0xFF) ^ AES_RT1(((Y1) >> 8) & 0xFF) ^ AES_RT2(((Y0) >> 16) & 0xFF) ^ \
               AES_RT3(((Y3) >> 24) & 0xFF);                                                                 \
                                                                                                             \
        (X3) = *RK++ ^ AES_RT0(((Y3)) & 0xFF) ^ AES_RT1(((Y2) >> 8) & 0xFF) ^ AES_RT2(((Y1) >> 16) & 0xFF) ^ \
               AES_RT3(((Y0) >> 24) & 0xFF);                                                                 \
    } while (0)

/*
 * AES-ECB block encryption
 */
static int _utils_internal_aes_encrypt(utils_aes_context *ctx, const unsigned char input[16], unsigned char output[16])
{
    int       i;
    uint32_t *RK = ctx->rk;
    struct {
        uint32_t X[4];
        uint32_t Y[4];
    } t;

    GET_UINT32_LE(t.X[0], input, 0);
    t.X[0] ^= *RK++;
    GET_UINT32_LE(t.X[1], input, 4);
    t.X[1] ^= *RK++;
    GET_UINT32_LE(t.X[2], input, 8);
    t.X[2] ^= *RK++;
    GET_UINT32_LE(t.X[3], input, 12);
    t.X[3] ^= *RK++;

    for (i = (ctx->nr >> 1) - 1; i > 0; i--) {
        AES_FROUND(t.Y[0], t.Y[1], t.Y[2], t.Y[3], t.X[0], t.X[1], t.X[2], t.X[3]);
        AES_FROUND(t.X[0], t.X[1], t.X[2], t.X[3], t.Y[0], t.Y[1], t.Y[2], t.Y[3]);
    }

    AES_FROUND(t.Y[0], t.Y[1], t.Y[2], t.Y[3], t.X[0], t.X[1], t.X[2], t.X[3]);

    t.X[0] = *RK++ ^ ((uint32_t)FSb[(t.Y[0]) & 0xFF]) ^ ((uint32_t)FSb[(t.Y[1] >> 8) & 0xFF] << 8) ^
             ((uint32_t)FSb[(t.Y[2] >> 16) & 0xFF] << 16) ^ ((uint32_t)FSb[(t.Y[3] >> 24) & 0xFF] << 24);

    t.X[1] = *RK++ ^ ((uint32_t)FSb[(t.Y[1]) & 0xFF]) ^ ((uint32_t)FSb[(t.Y[2] >> 8) & 0xFF] << 8) ^
             ((uint32_t)FSb[(t.Y[3] >> 16) & 0xFF] << 16) ^ ((uint32_t)FSb[(t.Y[0] >> 24) & 0xFF] << 24);

    t.X[2] = *RK++ ^ ((uint32_t)FSb[(t.Y[2]) & 0xFF]) ^ ((uint32_t)FSb[(t.Y[3] >> 8) & 0xFF] << 8) ^
             ((uint32_t)FSb[(t.Y[0] >> 16) & 0xFF] << 16) ^ ((uint32_t)FSb[(t.Y[1] >> 24) & 0xFF] << 24);

    t.X[3] = *RK++ ^ ((uint32_t)FSb[(t.Y[3]) & 0xFF]) ^ ((uint32_t)FSb[(t.Y[0] >> 8) & 0xFF] << 8) ^
             ((uint32_t)FSb[(t.Y[1] >> 16) & 0xFF] << 16) ^ ((uint32_t)FSb[(t.Y[2] >> 24) & 0xFF] << 24);

    PUT_UINT32_LE(t.X[0], output, 0);
    PUT_UINT32_LE(t.X[1], output, 4);
    PUT_UINT32_LE(t.X[2], output, 8);
    PUT_UINT32_LE(t.X[3], output, 12);
    return (0);
}

/*
 * AES-ECB block decryption
 */
static int _utils_internal_aes_decrypt(utils_aes_context *ctx, const unsigned char input[16], unsigned char output[16])
{
    int       i;
    uint32_t *RK = ctx->rk;
    struct {
        uint32_t X[4];
        uint32_t Y[4];
    } t;

    GET_UINT32_LE(t.X[0], input, 0);
    t.X[0] ^= *RK++;
    GET_UINT32_LE(t.X[1], input, 4);
    t.X[1] ^= *RK++;
    GET_UINT32_LE(t.X[2], input, 8);
    t.X[2] ^= *RK++;
    GET_UINT32_LE(t.X[3], input, 12);
    t.X[3] ^= *RK++;

    for (i = (ctx->nr >> 1) - 1; i > 0; i--) {
        AES_RROUND(t.Y[0], t.Y[1], t.Y[2], t.Y[3], t.X[0], t.X[1], t.X[2], t.X[3]);
        AES_RROUND(t.X[0], t.X[1], t.X[2], t.X[3], t.Y[0], t.Y[1], t.Y[2], t.Y[3]);
    }

    AES_RROUND(t.Y[0], t.Y[1], t.Y[2], t.Y[3], t.X[0], t.X[1], t.X[2], t.X[3]);

    t.X[0] = *RK++ ^ ((uint32_t)RSb[(t.Y[0]) & 0xFF]) ^ ((uint32_t)RSb[(t.Y[3] >> 8) & 0xFF] << 8) ^
             ((uint32_t)RSb[(t.Y[2] >> 16) & 0xFF] << 16) ^ ((uint32_t)RSb[(t.Y[1] >> 24) & 0xFF] << 24);

    t.X[1] = *RK++ ^ ((uint32_t)RSb[(t.Y[1]) & 0xFF]) ^ ((uint32_t)RSb[(t.Y[0] >> 8) & 0xFF] << 8) ^
             ((uint32_t)RSb[(t.Y[3] >> 16) & 0xFF] << 16) ^ ((uint32_t)RSb[(t.Y[2] >> 24) & 0xFF] << 24);

    t.X[2] = *RK++ ^ ((uint32_t)RSb[(t.Y[2]) & 0xFF]) ^ ((uint32_t)RSb[(t.Y[1] >> 8) & 0xFF] << 8) ^
             ((uint32_t)RSb[(t.Y[0] >> 16) & 0xFF] << 16) ^ ((uint32_t)RSb[(t.Y[3] >> 24) & 0xFF] << 24);

    t.X[3] = *RK++ ^ ((uint32_t)RSb[(t.Y[3]) & 0xFF]) ^ ((uint32_t)RSb[(t.Y[2] >> 8) & 0xFF] << 8) ^
             ((uint32_t)RSb[(t.Y[1] >> 16) & 0xFF] << 16) ^ ((uint32_t)RSb[(t.Y[0] >> 24) & 0xFF] << 24);

    PUT_UINT32_LE(t.X[0], output, 0);
    PUT_UINT32_LE(t.X[1], output, 4);
    PUT_UINT32_LE(t.X[2], output, 8);
    PUT_UINT32_LE(t.X[3], output, 12);
    return (0);
}

/*
 * AES-CBC buffer encryption/decryption
 */
int utils_aes_crypt_cbc(const unsigned char *input, size_t length, const uint8_t *key, unsigned int keybits, int mode,
                        unsigned char iv[16], unsigned char *output)
{
    int               i;
    unsigned char     temp[16];
    utils_aes_context ctx = {0};

    if (length % 16) {
        return -1;
    }
    mode ? _utils_aes_setkey_dec(&ctx, key, keybits) : _utils_aes_setkey_enc(&ctx, key, keybits);

    while (length > 0) {
        if (mode) {
            memcpy(temp, input, 16);
            _utils_internal_aes_decrypt(&ctx, input, output);
            for (i = 0; i < 16; i++) output[i] = (unsigned char)(output[i] ^ iv[i]);
            memcpy(iv, temp, 16);
        } else {
            for (i = 0; i < 16; i++) output[i] = (unsigned char)(input[i] ^ iv[i]);
            _utils_internal_aes_encrypt(&ctx, input, output);
            memcpy(iv, output, 16);
        }
        input += 16;
        output += 16;
        length -= 16;
    }
    return (0);
}
#else
#include "mbedtls/aes.h"
/*
 * AES-CBC buffer encryption/decryption
 */
int utils_aes_crypt_cbc(const unsigned char *input, size_t length, const uint8_t *key, unsigned int keybits, int mode,
                        unsigned char iv[16], unsigned char *output)
{
    mbedtls_aes_context ctx = {0};
    mode ? mbedtls_aes_setkey_dec(&ctx, (const uint8_t *)key, keybits) : mbedtls_aes_setkey_enc(&ctx, key, keybits);
    return mbedtls_aes_crypt_cbc(&ctx, mode ? MBEDTLS_AES_DECRYPT : MBEDTLS_AES_ENCRYPT, length, iv, input, output);
}
#endif
