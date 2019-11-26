/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_generate.c - General telephony tone generation.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: tone_generate.c,v 1.43 2008/07/02 14:48:26 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/dc_restore.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/tone_generate.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

#define ms_to_samples(t)            (((t)*SAMPLE_RATE)/1000)

void make_tone_gen_descriptor(tone_gen_descriptor_t *s,
                              int f1,
                              int l1,
                              int f2,
                              int l2,
                              int d1,
                              int d2,
                              int d3,
                              int d4,
                              int repeat)
{
    memset(s, 0, sizeof(*s));
    if (f1)
    {
        s->tone[0].phase_rate = dds_phase_ratef((float) f1);
        if (f2 < 0)
            s->tone[0].phase_rate = -s->tone[0].phase_rate;
        s->tone[0].gain = dds_scaling_dbm0f((float) l1);
    }
    if (f2)
    {
        s->tone[1].phase_rate = dds_phase_ratef((float) abs(f2));
        s->tone[1].gain = (f2 < 0)  ?  (float) l2/100.0f  :  dds_scaling_dbm0f((float) l2);
    }

    s->duration[0] = d1*SAMPLE_RATE/1000;
    s->duration[1] = d2*SAMPLE_RATE/1000;
    s->duration[2] = d3*SAMPLE_RATE/1000;
    s->duration[3] = d4*SAMPLE_RATE/1000;

    s->repeat = repeat;
}
/*- End of function --------------------------------------------------------*/

tone_gen_state_t *tone_gen_init(tone_gen_state_t *s, tone_gen_descriptor_t *t)
{
    int i;

    if (s == NULL)
        return NULL;
    for (i = 0;  i < 4;  i++)
    {
        s->tone[i] = t->tone[i];
        s->phase[i] = 0;
    }

    for (i = 0;  i < 4;  i++)
        s->duration[i] = t->duration[i];
    s->repeat = t->repeat;

    s->current_section = 0;
    s->current_position = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

int tone_gen(tone_gen_state_t *s, int16_t amp[], int max_samples)
{
    int samples;
    int limit;
    float xamp;
    int i;

    if (s->current_section < 0)
        return  0;

    for (samples = 0;  samples < max_samples;  )
    {
        limit = samples + s->duration[s->current_section] - s->current_position;
        if (limit > max_samples)
            limit = max_samples;
        
        s->current_position += (limit - samples);
        if (s->current_section & 1)
        {
            /* A silent section */
            for (  ;  samples < limit;  samples++)
                amp[samples] = 0;
        }
        else
        {
            if (s->tone[0].phase_rate < 0)
            {
                for (  ;  samples < limit;  samples++)
                {
                    /* There must be two, and only two tones */
                    xamp = dds_modf(&s->phase[0], -s->tone[0].phase_rate, s->tone[0].gain, 0)
                         *(1.0f + dds_modf(&s->phase[1], s->tone[1].phase_rate, s->tone[1].gain, 0));
                    amp[samples] = (int16_t) lrintf(xamp);
                }
            }
            else
            {
                for (  ;  samples < limit;  samples++)
                {
                    xamp = 0.0f;
                    for (i = 0;  i < 4;  i++)
                    {
                        if (s->tone[i].phase_rate == 0)
                            break;
                        xamp += dds_modf(&s->phase[i], s->tone[i].phase_rate, s->tone[i].gain, 0);
                    }
                    /* Saturation of the answer is the right thing at this point.
                       However, we are normally generating well controlled tones,
                       that cannot clip. So, the overhead of doing saturation is
                       a waste of valuable time. */
                    amp[samples] = (int16_t) lrintf(xamp);
                }
            }
        }
        if (s->current_position >= s->duration[s->current_section])
        {
            s->current_position = 0;
            if (++s->current_section > 3  ||  s->duration[s->current_section] == 0)
            {
                if (!s->repeat)
                {
                    /* Force a quick exit */
                    s->current_section = -1;
                    break;
                }
                s->current_section = 0;
            }
        }
    }
    return samples;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
