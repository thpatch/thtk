/*
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain this list
 *    of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce this
 *    list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <config.h>
#include <stdlib.h>
#include "rng_mt.h"

#define N 624
#define M 397
#define UPPER_MASK 0x80000000U
#define LOWER_MASK 0x7FFFFFFFU

void
rng_mt_init(
    struct rng_mt *rng,
    uint32_t seed)
{
    uint32_t *mt = rng->mt;

    mt[0] = seed;
    for (int32_t i = 1; i < N; ++i) {
        mt[i] = (0x6c078965UL * (mt[i-1] ^ (mt[i-1] >> 30)) + i);
    }
    rng->mti = N;
}

uint32_t
rng_mt_nextint(
    struct rng_mt *rng)
{
    uint32_t *mt = rng->mt;
    uint32_t ret;
    static uint32_t mag01[2] = {0x0UL, 0x9908b0dfUL};
    if (rng->mti >= N) {
        int i;
        uint32_t t;

        for (i = 0; i < N-M; ++i) {
            t = (mt[i]&UPPER_MASK) | (mt[i+1]&LOWER_MASK);
            mt[i] = mt[i+M] ^ (t>>1) ^ mag01[t&1];
        }
        for (; i < N-1; i++) {
            t = (mt[i]&UPPER_MASK) | (mt[i+1]&LOWER_MASK);
            mt[i] = mt[i+(M-N)] ^ (t>>1) ^ mag01[t&1];
        }
        t = (mt[N-1]&UPPER_MASK) | (mt[0]&LOWER_MASK);
        mt[N-1] = mt[M-1] ^ (t>>1) ^ mag01[t&1];

        rng->mti = 0;
    }

    ret = mt[rng->mti++];

    ret ^= (ret>>11);
    ret ^= (ret<<7) & 0x9d2c5680UL;
    ret ^= (ret<<15) & 0xefc60000UL;
    ret ^= (ret>>18);

    return ret;
}
