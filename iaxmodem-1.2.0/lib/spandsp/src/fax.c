//#define LOG_FAX_AUDIO
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax.c - Analogue line ITU T.30 FAX transfer processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2005, 2006 Steve Underwood
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
 * $Id: fax.c,v 1.72 2008/07/25 13:56:54 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "floating_fudge.h"
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#if defined(LOG_FAX_AUDIO)
#include <unistd.h>
#endif
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/dc_restore.h"
#include "spandsp/power_meter.h"
#include "spandsp/complex.h"
#include "spandsp/tone_generate.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/silence_gen.h"
#include "spandsp/fsk.h"
#include "spandsp/v29rx.h"
#include "spandsp/v29tx.h"
#include "spandsp/v27ter_rx.h"
#include "spandsp/v27ter_tx.h"
#include "spandsp/v17rx.h"
#include "spandsp/v17tx.h"
#include "spandsp/t4.h"

#include "spandsp/t30_fcf.h"
#include "spandsp/t35.h"
#include "spandsp/t30.h"
#include "spandsp/t30_api.h"
#include "spandsp/t30_logging.h"

#include "spandsp/fax_modems.h"
#include "spandsp/fax.h"

#define HDLC_FRAMING_OK_THRESHOLD       5

static void fax_send_hdlc(void *user_data, const uint8_t *msg, int len)
{
    fax_state_t *s;

    s = (fax_state_t *) user_data;
    
    hdlc_tx_frame(&(s->fe.modems.hdlc_tx), msg, len);
}
/*- End of function --------------------------------------------------------*/

static void hdlc_underflow_handler(void *user_data)
{
    t30_state_t *s;

    s = (t30_state_t *) user_data;
    t30_front_end_status(s, T30_FRONT_END_SEND_STEP_COMPLETE);
}
/*- End of function --------------------------------------------------------*/

static int dummy_rx(void *user_data, const int16_t amp[], int len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v17_rx(void *user_data, const int16_t amp[], int len)
{
    fax_state_t *s;

    s = (fax_state_t *) user_data;
    v17_rx(&(s->fe.modems.v17_rx), amp, len);
    fsk_rx(&(s->fe.modems.v21_rx), amp, len);
    if (s->t30.rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.17 + V.21 to V.17 (%.2fdBm0)\n", v17_rx_signal_power(&(s->fe.modems.v17_rx)));
        s->fe.rx_handler = (span_rx_handler_t *) &v17_rx;
        s->fe.rx_user_data = &(s->fe.modems.v17_rx);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v27ter_rx(void *user_data, const int16_t amp[], int len)
{
    fax_state_t *s;

    s = (fax_state_t *) user_data;
    v27ter_rx(&(s->fe.modems.v27ter_rx), amp, len);
    fsk_rx(&(s->fe.modems.v21_rx), amp, len);
    if (s->t30.rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.27ter + V.21 to V.27ter (%.2fdBm0)\n", v27ter_rx_signal_power(&(s->fe.modems.v27ter_rx)));
        s->fe.rx_handler = (span_rx_handler_t *) &v27ter_rx;
        s->fe.rx_user_data = &(s->fe.modems.v27ter_rx);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int early_v29_rx(void *user_data, const int16_t amp[], int len)
{
    fax_state_t *s;

    s = (fax_state_t *) user_data;
    v29_rx(&(s->fe.modems.v29_rx), amp, len);
    fsk_rx(&(s->fe.modems.v21_rx), amp, len);
    if (s->t30.rx_trained)
    {
        /* The fast modem has trained, so we no longer need to run the slow
           one in parallel. */
        span_log(&s->logging, SPAN_LOG_FLOW, "Switching from V.29 + V.21 to V.29 (%.2fdBm0)\n", v29_rx_signal_power(&(s->fe.modems.v29_rx)));
        s->fe.rx_handler = (span_rx_handler_t *) &v29_rx;
        s->fe.rx_user_data = &(s->fe.modems.v29_rx);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int fax_rx(fax_state_t *s, int16_t *amp, int len)
{
    int i;

#if defined(LOG_FAX_AUDIO)
    if (s->fe.fax_audio_rx_log >= 0)
        write(s->fe.fax_audio_rx_log, amp, len*sizeof(int16_t));
#endif
    for (i = 0;  i < len;  i++)
        amp[i] = dc_restore(&(s->fe.modems.dc_restore), amp[i]);
    s->fe.rx_handler(s->fe.rx_user_data, amp, len);
    t30_timer_update(&(s->t30), len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int set_next_tx_type(fax_state_t *s)
{
    if (s->fe.next_tx_handler)
    {
        s->fe.tx_handler = s->fe.next_tx_handler;
        s->fe.tx_user_data = s->fe.next_tx_user_data;
        s->fe.next_tx_handler = NULL;
        return 0;
    }
    /* If there is nothing else to change to, so use zero length silence */
    silence_gen_alter(&(s->fe.modems.silence_gen), 0);
    s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
    s->fe.tx_user_data = &(s->fe.modems.silence_gen);
    s->fe.next_tx_handler = NULL;
    s->fe.transmit = FALSE;
    return -1;
}
/*- End of function --------------------------------------------------------*/

int fax_tx(fax_state_t *s, int16_t *amp, int max_len)
{
    int len;
#if defined(LOG_FAX_AUDIO)
    int required_len;
    
    required_len = max_len;
#endif
    len = 0;
    if (s->fe.transmit)
    {
        while ((len += s->fe.tx_handler(s->fe.tx_user_data, amp + len, max_len - len)) < max_len)
        {
            /* Allow for a change of tx handler within a block */
            if (set_next_tx_type(s)  &&  s->fe.current_tx_type != T30_MODEM_NONE  &&  s->fe.current_tx_type != T30_MODEM_DONE)
                t30_front_end_status(&(s->t30), T30_FRONT_END_SEND_STEP_COMPLETE);
            if (!s->fe.transmit)
            {
                if (s->fe.transmit_on_idle)
                {
                    /* Pad to the requested length with silence */
                    memset(amp + len, 0, (max_len - len)*sizeof(int16_t));
                    len = max_len;        
                }
                break;
            }
        }
    }
    else
    {
        if (s->fe.transmit_on_idle)
        {
            /* Pad to the requested length with silence */
            memset(amp, 0, max_len*sizeof(int16_t));
            len = max_len;        
        }
    }
#if defined(LOG_FAX_AUDIO)
    if (s->fe.fax_audio_tx_log >= 0)
    {
        if (len < required_len)
            memset(amp + len, 0, (required_len - len)*sizeof(int16_t));
        write(s->fe.fax_audio_tx_log, amp, required_len*sizeof(int16_t));
    }
#endif
    return len;
}
/*- End of function --------------------------------------------------------*/

static void fax_set_rx_type(void *user_data, int type, int short_train, int use_hdlc)
{
    fax_state_t *s;
    put_bit_func_t put_bit_func;
    void *put_bit_user_data;

    s = (fax_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set rx type %d\n", type);
    if (s->fe.current_rx_type == type)
        return;
    s->fe.current_rx_type = type;
    if (use_hdlc)
    {
        put_bit_func = (put_bit_func_t) hdlc_rx_put_bit;
        put_bit_user_data = (void *) &(s->fe.modems.hdlc_rx);
        hdlc_rx_init(&(s->fe.modems.hdlc_rx), FALSE, FALSE, HDLC_FRAMING_OK_THRESHOLD, t30_hdlc_accept, &(s->t30));
    }
    else
    {
        put_bit_func = t30_non_ecm_put_bit;
        put_bit_user_data = (void *) &(s->t30);
    }
    switch (type)
    {
    case T30_MODEM_V21:
        if (s->fe.flush_handler)
            s->fe.flush_handler(s, s->fe.flush_user_data, 3);
        fsk_rx_init(&(s->fe.modems.v21_rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_put_bit, put_bit_user_data);
        fsk_rx_signal_cutoff(&(s->fe.modems.v21_rx), -45.5);
        s->fe.rx_handler = (span_rx_handler_t *) &fsk_rx;
        s->fe.rx_user_data = &(s->fe.modems.v21_rx);
        break;
    case T30_MODEM_V27TER_2400:
        v27ter_rx_restart(&(s->fe.modems.v27ter_rx), 2400, FALSE);
        v27ter_rx_set_put_bit(&(s->fe.modems.v27ter_rx), put_bit_func, put_bit_user_data);
        s->fe.rx_handler = (span_rx_handler_t *) &early_v27ter_rx;
        s->fe.rx_user_data = s;
        break;
    case T30_MODEM_V27TER_4800:
        v27ter_rx_restart(&(s->fe.modems.v27ter_rx), 4800, FALSE);
        v27ter_rx_set_put_bit(&(s->fe.modems.v27ter_rx), put_bit_func, put_bit_user_data);
        s->fe.rx_handler = (span_rx_handler_t *) &early_v27ter_rx;
        s->fe.rx_user_data = s;
        break;
    case T30_MODEM_V29_7200:
        v29_rx_restart(&(s->fe.modems.v29_rx), 7200, FALSE);
        v29_rx_set_put_bit(&(s->fe.modems.v29_rx), put_bit_func, put_bit_user_data);
        s->fe.rx_handler = (span_rx_handler_t *) &early_v29_rx;
        s->fe.rx_user_data = s;
        break;
    case T30_MODEM_V29_9600:
        v29_rx_restart(&(s->fe.modems.v29_rx), 9600, FALSE);
        v29_rx_set_put_bit(&(s->fe.modems.v29_rx), put_bit_func, put_bit_user_data);
        s->fe.rx_handler = (span_rx_handler_t *) &early_v29_rx;
        s->fe.rx_user_data = s;
        break;
    case T30_MODEM_V17_7200:
        v17_rx_restart(&(s->fe.modems.v17_rx), 7200, short_train);
        v17_rx_set_put_bit(&(s->fe.modems.v17_rx), put_bit_func, put_bit_user_data);
        s->fe.rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->fe.rx_user_data = s;
        break;
    case T30_MODEM_V17_9600:
        v17_rx_restart(&(s->fe.modems.v17_rx), 9600, short_train);
        v17_rx_set_put_bit(&(s->fe.modems.v17_rx), put_bit_func, put_bit_user_data);
        s->fe.rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->fe.rx_user_data = s;
        break;
    case T30_MODEM_V17_12000:
        v17_rx_restart(&(s->fe.modems.v17_rx), 12000, short_train);
        v17_rx_set_put_bit(&(s->fe.modems.v17_rx), put_bit_func, put_bit_user_data);
        s->fe.rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->fe.rx_user_data = s;
        break;
    case T30_MODEM_V17_14400:
        v17_rx_restart(&(s->fe.modems.v17_rx), 14400, short_train);
        v17_rx_set_put_bit(&(s->fe.modems.v17_rx), put_bit_func, put_bit_user_data);
        s->fe.rx_handler = (span_rx_handler_t *) &early_v17_rx;
        s->fe.rx_user_data = s;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
    default:
        s->fe.rx_handler = (span_rx_handler_t *) &dummy_rx;
        s->fe.rx_user_data = s;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void fax_set_tx_type(void *user_data, int type, int short_train, int use_hdlc)
{
    fax_state_t *s;
    tone_gen_descriptor_t tone_desc;
    get_bit_func_t get_bit_func;
    void *get_bit_user_data;

    s = (fax_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set tx type %d\n", type);
    if (s->fe.current_tx_type == type)
        return;
    if (use_hdlc)
    {
        get_bit_func = (get_bit_func_t) hdlc_tx_get_bit;
        get_bit_user_data = (void *) &(s->fe.modems.hdlc_tx);
    }
    else
    {
        get_bit_func = t30_non_ecm_get_bit;
        get_bit_user_data = (void *) &(s->t30);
    }
    switch (type)
    {
    case T30_MODEM_PAUSE:
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(short_train));
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = NULL;
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_CNG:
        /* 0.5s of 1100Hz+-38Hz + 3.0s of silence repeating. Timing +-15% */
        make_tone_gen_descriptor(&tone_desc,
                                 1100,
                                 -11,
                                 0,
                                 0,
                                 500,
                                 3000,
                                 0,
                                 0,
                                 TRUE);
        tone_gen_init(&(s->fe.modems.tone_gen), &tone_desc);
        s->fe.tx_handler = (span_tx_handler_t *) &tone_gen;
        s->fe.tx_user_data = &(s->fe.modems.tone_gen);
        s->fe.next_tx_handler = NULL;
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_CED:
        /* 0.2s of silence, then 2.6s to 4s of 2100Hz+-15Hz tone, then 75ms of silence. The 75ms of silence
           will be inserted by the pre V.21 pause we use for any switch to V.21. */
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(200));
        make_tone_gen_descriptor(&tone_desc,
                                 2100,
                                 -11,
                                 0,
                                 0,
                                 2600,
                                 0,
                                 0,
                                 0,
                                 FALSE);
        tone_gen_init(&(s->fe.modems.tone_gen), &tone_desc);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &tone_gen;
        s->fe.next_tx_user_data = &(s->fe.modems.tone_gen);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_V21:
        fsk_tx_init(&(s->fe.modems.v21_tx), &preset_fsk_specs[FSK_V21CH2], get_bit_func, get_bit_user_data);
        /* The spec says 1s +-15% of preamble. So, the minimum is 32 octets. */
        hdlc_tx_flags(&(s->fe.modems.hdlc_tx), 32);
        /* Pause before switching from phase C, as per T.30 5.3.2.2. If we omit this, the receiver
           might not see the carrier fall between the high speed and low speed sections. In practice,
           a 75ms gap before any V.21 transmission is harmless, adds little to the overall length of
           a call, and ensures the receiving end is ready. */
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(75));
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &fsk_tx;
        s->fe.next_tx_user_data = &(s->fe.modems.v21_tx);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_V27TER_2400:
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(75));
        v27ter_tx_restart(&(s->fe.modems.v27ter_tx), 2400, s->fe.use_tep);
        v27ter_tx_set_get_bit(&(s->fe.modems.v27ter_tx), get_bit_func, get_bit_user_data);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &v27ter_tx;
        s->fe.next_tx_user_data = &(s->fe.modems.v27ter_tx);
        hdlc_tx_flags(&(s->fe.modems.hdlc_tx), 60);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_V27TER_4800:
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(75));
        v27ter_tx_restart(&(s->fe.modems.v27ter_tx), 4800, s->fe.use_tep);
        v27ter_tx_set_get_bit(&(s->fe.modems.v27ter_tx), get_bit_func, get_bit_user_data);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &v27ter_tx;
        s->fe.next_tx_user_data = &(s->fe.modems.v27ter_tx);
        hdlc_tx_flags(&(s->fe.modems.hdlc_tx), 120);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_V29_7200:
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(75));
        v29_tx_restart(&(s->fe.modems.v29_tx), 7200, s->fe.use_tep);
        v29_tx_set_get_bit(&(s->fe.modems.v29_tx), get_bit_func, get_bit_user_data);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &v29_tx;
        s->fe.next_tx_user_data = &(s->fe.modems.v29_tx);
        hdlc_tx_flags(&(s->fe.modems.hdlc_tx), 180);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_V29_9600:
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(75));
        v29_tx_restart(&(s->fe.modems.v29_tx), 9600, s->fe.use_tep);
        v29_tx_set_get_bit(&(s->fe.modems.v29_tx), get_bit_func, get_bit_user_data);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &v29_tx;
        s->fe.next_tx_user_data = &(s->fe.modems.v29_tx);
        hdlc_tx_flags(&(s->fe.modems.hdlc_tx), 240);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_V17_7200:
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(75));
        v17_tx_restart(&(s->fe.modems.v17_tx), 7200, s->fe.use_tep, short_train);
        v17_tx_set_get_bit(&(s->fe.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &v17_tx;
        s->fe.next_tx_user_data = &(s->fe.modems.v17_tx);
        hdlc_tx_flags(&(s->fe.modems.hdlc_tx), 180);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_V17_9600:
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(75));
        v17_tx_restart(&(s->fe.modems.v17_tx), 9600, s->fe.use_tep, short_train);
        v17_tx_set_get_bit(&(s->fe.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &v17_tx;
        s->fe.next_tx_user_data = &(s->fe.modems.v17_tx);
        hdlc_tx_flags(&(s->fe.modems.hdlc_tx), 240);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_V17_12000:
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(75));
        v17_tx_restart(&(s->fe.modems.v17_tx), 12000, s->fe.use_tep, short_train);
        v17_tx_set_get_bit(&(s->fe.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &v17_tx;
        s->fe.next_tx_user_data = &(s->fe.modems.v17_tx);
        hdlc_tx_flags(&(s->fe.modems.hdlc_tx), 300);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_V17_14400:
        silence_gen_alter(&(s->fe.modems.silence_gen), ms_to_samples(75));
        v17_tx_restart(&(s->fe.modems.v17_tx), 14400, s->fe.use_tep, short_train);
        v17_tx_set_get_bit(&(s->fe.modems.v17_tx), get_bit_func, get_bit_user_data);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = (span_tx_handler_t *) &v17_tx;
        s->fe.next_tx_user_data = &(s->fe.modems.v17_tx);
        hdlc_tx_flags(&(s->fe.modems.hdlc_tx), 360);
        s->fe.transmit = TRUE;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
        /* Fall through */
    default:
        silence_gen_alter(&(s->fe.modems.silence_gen), 0);
        s->fe.tx_handler = (span_tx_handler_t *) &silence_gen;
        s->fe.tx_user_data = &(s->fe.modems.silence_gen);
        s->fe.next_tx_handler = NULL;
        s->fe.transmit = FALSE;
        break;
    }
    s->fe.current_tx_type = type;
}
/*- End of function --------------------------------------------------------*/

void fax_set_transmit_on_idle(fax_state_t *s, int transmit_on_idle)
{
    s->fe.transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

void fax_set_tep_mode(fax_state_t *s, int use_tep)
{
    s->fe.use_tep = use_tep;
}
/*- End of function --------------------------------------------------------*/

t30_state_t *fax_get_t30_state(fax_state_t *s)
{
    return &s->t30;
}
/*- End of function --------------------------------------------------------*/

fax_state_t *fax_init(fax_state_t *s, int calling_party)
{
    if (s == NULL)
    {
        if ((s = (fax_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }

    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "FAX");
    t30_init(&(s->t30),
             calling_party,
             fax_set_rx_type,
             (void *) s,
             fax_set_tx_type,
             (void *) s,
             fax_send_hdlc,
             (void *) s);
    t30_set_supported_modems(&(s->t30), T30_SUPPORT_V27TER | T30_SUPPORT_V29);
    hdlc_rx_init(&(s->fe.modems.hdlc_rx), FALSE, FALSE, HDLC_FRAMING_OK_THRESHOLD, t30_hdlc_accept, &(s->t30));
    fsk_rx_init(&(s->fe.modems.v21_rx), &preset_fsk_specs[FSK_V21CH2], TRUE, (put_bit_func_t) hdlc_rx_put_bit, &(s->fe.modems.hdlc_rx));
    fsk_rx_signal_cutoff(&(s->fe.modems.v21_rx), -45.5);
    hdlc_tx_init(&(s->fe.modems.hdlc_tx), FALSE, 2, FALSE, hdlc_underflow_handler, &(s->t30));
    fsk_tx_init(&(s->fe.modems.v21_tx), &preset_fsk_specs[FSK_V21CH2], (get_bit_func_t) hdlc_tx_get_bit, &(s->fe.modems.hdlc_tx));
    v17_rx_init(&(s->fe.modems.v17_rx), 14400, t30_non_ecm_put_bit, &(s->t30));
    v17_tx_init(&(s->fe.modems.v17_tx), 14400, s->fe.use_tep, t30_non_ecm_get_bit, &(s->t30));
    v29_rx_init(&(s->fe.modems.v29_rx), 9600, t30_non_ecm_put_bit, &(s->t30));
    v29_rx_signal_cutoff(&(s->fe.modems.v29_rx), -45.5);
    v29_tx_init(&(s->fe.modems.v29_tx), 9600, s->fe.use_tep, t30_non_ecm_get_bit, &(s->t30));
    v27ter_rx_init(&(s->fe.modems.v27ter_rx), 4800, t30_non_ecm_put_bit, &(s->t30));
    v27ter_tx_init(&(s->fe.modems.v27ter_tx), 4800, s->fe.use_tep, t30_non_ecm_get_bit, &(s->t30));
    silence_gen_init(&(s->fe.modems.silence_gen), 0);
    dc_restore_init(&(s->fe.modems.dc_restore));
    t30_restart(&(s->t30));
#if defined(LOG_FAX_AUDIO)
    {
        char buf[100 + 1];
        struct tm *tm;
        time_t now;

        time(&now);
        tm = localtime(&now);
        sprintf(buf,
                "/tmp/fax-rx-audio-%p-%02d%02d%02d%02d%02d%02d",
                s,
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        s->fe.fax_audio_rx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        sprintf(buf,
                "/tmp/fax-tx-audio-%p-%02d%02d%02d%02d%02d%02d",
                s,
                tm->tm_year%100,
                tm->tm_mon + 1,
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec);
        s->fe.fax_audio_tx_log = open(buf, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    }
#endif
    return s;
}
/*- End of function --------------------------------------------------------*/

int fax_release(fax_state_t *s)
{
    t30_release(&s->t30);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int fax_free(fax_state_t *s)
{
    t30_release(&s->t30);
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

void fax_set_flush_handler(fax_state_t *s, fax_flush_handler_t *handler, void *user_data)
{
    s->fe.flush_handler = handler;
    s->fe.flush_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
