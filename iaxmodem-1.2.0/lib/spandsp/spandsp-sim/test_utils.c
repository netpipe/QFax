/*
 * SpanDSP - a series of DSP components for telephony
 *
 * test_utils.c - Utility routines for module tests.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: test_utils.c,v 1.6 2008/07/02 14:48:25 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <time.h>
#include <fcntl.h>
#include <audiofile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

struct codec_munge_state_s
{
    int munging_codec;
    g726_state_t g726_enc_state;
    g726_state_t g726_dec_state;
    int rbs_pattern;
    int sequence;
};

struct complexify_state_s
{
    float history[128];
    int ptr;
};

complexify_state_t *complexify_init(void)
{
    complexify_state_t *s;
    int i;

    if ((s = (complexify_state_t *) malloc(sizeof(*s))))
    {
        s->ptr = 0;
        for (i = 0;  i < 128;  i++)
            s->history[i] = 0.0f;
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

void complexify_release(complexify_state_t *s)
{
    free(s);
}
/*- End of function --------------------------------------------------------*/

complexf_t complexify(complexify_state_t *s, int16_t amp)
{
#define HILBERT_GAIN    1.569546344
    static const float hilbert_coeffs[] =
    {
        +0.0012698413f, +0.0013489483f,
        +0.0015105196f, +0.0017620440f,
        +0.0021112899f, +0.0025663788f,
        +0.0031358856f, +0.0038289705f,
        +0.0046555545f, +0.0056265487f,
        +0.0067541562f, +0.0080522707f,
        +0.0095370033f, +0.0112273888f,
        +0.0131463382f, +0.0153219442f,
        +0.0177892941f, +0.0205930381f,
        +0.0237910974f, +0.0274601544f,
        +0.0317040029f, +0.0366666667f,
        +0.0425537942f, +0.0496691462f,
        +0.0584802574f, +0.0697446887f,
        +0.0847739823f, +0.1060495199f,
        +0.1388940865f, +0.1971551103f,
        +0.3316207267f, +0.9994281838f,
    };
    float famp;
    int i;
    int j;
    int k;
    complexf_t res;

    s->history[s->ptr] = amp;
    i = s->ptr - 63;
    if (i < 0)
        i += 128;
    res.re = s->history[i];

    famp = 0.0f;
    j = s->ptr - 126;
    if (j < 0)
        j += 128;
    for (i = 0, k = s->ptr;  i < 32;  i++)
    {
        famp += (s->history[k] - s->history[j])*hilbert_coeffs[i];
        j += 2;
        if (j >= 128)
            j -= 128;
        k -= 2;
        if (k < 0)
            k += 128;
    }
    res.im = famp/HILBERT_GAIN;

    if (++s->ptr >= 128)
        s->ptr = 0;
    return res;
}
/*- End of function --------------------------------------------------------*/

codec_munge_state_t *codec_munge_init(int codec, int info)
{
    codec_munge_state_t *s;
    
    if ((s = (codec_munge_state_t *) malloc(sizeof(*s))))
    {
        switch (codec)
        {
        case MUNGE_CODEC_G726_40K:
            g726_init(&s->g726_enc_state, 40000, G726_ENCODING_LINEAR, G726_PACKING_NONE);
            g726_init(&s->g726_dec_state, 40000, G726_ENCODING_LINEAR, G726_PACKING_NONE);
            s->munging_codec = MUNGE_CODEC_G726_32K;
            break;
        case MUNGE_CODEC_G726_32K:
            g726_init(&s->g726_enc_state, 32000, G726_ENCODING_LINEAR, G726_PACKING_NONE);
            g726_init(&s->g726_dec_state, 32000, G726_ENCODING_LINEAR, G726_PACKING_NONE);
            s->munging_codec = MUNGE_CODEC_G726_32K;
            break;
        case MUNGE_CODEC_G726_24K:
            g726_init(&s->g726_enc_state, 24000, G726_ENCODING_LINEAR, G726_PACKING_NONE);
            g726_init(&s->g726_dec_state, 24000, G726_ENCODING_LINEAR, G726_PACKING_NONE);
            s->munging_codec = MUNGE_CODEC_G726_32K;
            break;
        case MUNGE_CODEC_G726_16K:
            g726_init(&s->g726_enc_state, 16000, G726_ENCODING_LINEAR, G726_PACKING_NONE);
            g726_init(&s->g726_dec_state, 16000, G726_ENCODING_LINEAR, G726_PACKING_NONE);
            s->munging_codec = MUNGE_CODEC_G726_32K;
            break;
        default:
            s->munging_codec = codec;
            break;
        }
        s->sequence = 0;
        s->rbs_pattern = info;
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

void codec_munge_release(codec_munge_state_t *s)
{
    free(s);
}
/*- End of function --------------------------------------------------------*/

void codec_munge(codec_munge_state_t *s, int16_t amp[], int len)
{
    uint8_t law;
    uint8_t adpcmdata[160];
    int i;
    int adpcm;
    int x;

    switch (s->munging_codec)
    {
    case MUNGE_CODEC_NONE:
        /* Do nothing */
        break;
    case MUNGE_CODEC_ALAW:
        for (i = 0;  i < len;  i++)
        {
            law = linear_to_alaw(amp[i]);
            amp[i] = alaw_to_linear(law);
        }
        break;
    case MUNGE_CODEC_ULAW:
        for (i = 0;  i < len;  i++)
        {
            law = linear_to_ulaw(amp[i]);
            if (s->rbs_pattern & (1 << s->sequence))
            {
                /* Strip the bottom bit at the RBS rate */
                law &= 0xFE;
            }
            amp[i] = ulaw_to_linear(law);
        }
        break;
    case MUNGE_CODEC_G726_32K:
        /* This could actually be any of the G.726 rates */
        for (i = 0;  i < len;  i += x)
        {
            x = (len - i >= 160)  ?  160  :  (len - i);
            adpcm = g726_encode(&s->g726_enc_state, adpcmdata, amp + i, x);
            g726_decode(&s->g726_dec_state, amp + i, adpcmdata, adpcm);
        }
        break;
    }
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
