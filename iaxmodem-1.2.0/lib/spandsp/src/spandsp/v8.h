/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v8.h - V.8 modem negotiation processing.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: v8.h,v 1.22 2008/05/14 15:41:25 steveu Exp $
 */
 
/*! \file */

/*! \page v8_page The V.8 modem negotiation protocol
\section v8_page_sec_1 What does it do?
The V.8 specification defines a procedure to be used as PSTN modem answer phone calls,
which allows the modems to negotiate the optimum modem standard, which both ends can
support.

\section v8_page_sec_2 How does it work?
At startup the modems communicate using the V.21 standard at 300 bits/second. They
exchange simple messages about their capabilities, and choose the modem standard they
will use for data communication. The V.8 protocol then terminates, and the modems
being negotiating and training with their chosen modem standard.
*/

#if !defined(_SPANDSP_V8_H_)
#define _SPANDSP_V8_H_

typedef struct v8_result_s v8_result_t;

typedef void (v8_result_handler_t)(void *user_data, v8_result_t *result);

enum v8_call_function_e
{
    V8_CALL_TBS = 0,
    V8_CALL_H324,
    V8_CALL_V18,
    V8_CALL_T101,
    V8_CALL_T30_TX,
    V8_CALL_T30_RX,
    V8_CALL_V_SERIES,
    V8_CALL_FUNCTION_EXTENSION
};

enum v8_modulation_e
{
    V8_MOD_V17          = (1 << 0),     /* V.17 half-duplex */
    V8_MOD_V21          = (1 << 1),     /* V.21 duplex */
    V8_MOD_V22          = (1 << 2),     /* V.22/V22.bis duplex */
    V8_MOD_V23HALF      = (1 << 3),     /* V.23 half-duplex */
    V8_MOD_V23          = (1 << 4),     /* V.23 duplex */
    V8_MOD_V26BIS       = (1 << 5),     /* V.23 duplex */
    V8_MOD_V26TER       = (1 << 6),     /* V.23 duplex */
    V8_MOD_V27TER       = (1 << 7),     /* V.23 duplex */
    V8_MOD_V29          = (1 << 8),     /* V.29 half-duplex */
    V8_MOD_V32          = (1 << 9),     /* V.32/V32.bis duplex */
    V8_MOD_V34HALF      = (1 << 10),    /* V.34 half-duplex */
    V8_MOD_V34          = (1 << 11),    /* V.34 duplex */
    V8_MOD_V90          = (1 << 12),    /* V.90 duplex */
    V8_MOD_V92          = (1 << 13),    /* V.92 duplex */

    V8_MOD_FAILED       = (1 << 15),    /* Indicates failure to negotiate */
};

enum v8_protocol_e
{
    V8_PROTOCOL_NONE = 0,
    V8_PROTOCOL_LAPM_V42 = 1,
    V8_PROTOCOL_EXTENSION = 7
};

enum v8_pstn_access_e
{
    V8_PSTN_ACCESS_CALL_DCE_CELLULAR = 0x20,
    V8_PSTN_ACCESS_ANSWER_DCE_CELLULAR = 0x40,
    V8_PSTN_ACCESS_DCE_ON_DIGTIAL = 0x80
};

enum v8_pcm_modem_availability_e
{
    V8_PSTN_PCM_MODEM_V90_V92_ANALOGUE = 0x20,
    V8_PSTN_PCM_MODEM_V90_V92_DIGITAL = 0x40,
    V8_PSTN_PCM_MODEM_V91 = 0x80
};

typedef struct
{
    /*! \brief TRUE if we are the calling modem */
    int caller;
    /*! \brief The current state of the V8 protocol */
    int state;
    int negotiation_timer;
    int ci_timer;
    int ci_count;
    fsk_tx_state_t v21tx;
    fsk_rx_state_t v21rx;
    queue_state_t *tx_queue;
    modem_connect_tones_tx_state_t ansam_tx;
    modem_connect_tones_rx_state_t ansam_rx;

    v8_result_handler_t *result_handler;
    void *result_handler_user_data;

    /*! \brief Modulation schemes available at this end. */
    int available_modulations;
    int common_modulations;
    int negotiated_modulation;
    int far_end_modulations;
    
    int call_function;
    int protocol;
    int pstn_access;
    int nsf_seen;
    int pcm_modem_availability;
    int t66_seen;

    /* V8 data parsing */
    unsigned int bit_stream;
    int bit_cnt;
    /* Indicates the type of message coming up */
    int preamble_type;
    uint8_t rx_data[64];
    int rx_data_ptr;
    
    /*! \brief a reference copy of the last CM or JM message, used when
               testing for matches. */
    uint8_t cm_jm_data[64];
    int cm_jm_count;
    int got_cm_jm;
    int got_cj;
    int zero_byte_count;
    /*! \brief Error and flow logging control */
    logging_state_t logging;
} v8_state_t;

struct v8_result_s
{
    int call_function;
    int available_modulations;
    int negotiated_modulation;
    int protocol;
    int pstn_access;
    int nsf_seen;
    int pcm_modem_availability;
    int t66_seen;
};

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Initialise a V.8 context.
    \brief Initialise a V.8 context.
    \param s The V.8 context.
    \param caller TRUE if caller mode, else answerer mode.
    \param available_modulations A bitwise list of the modulation schemes to be
           advertised as available here.
    \param result_handler The callback routine used to handle the results of negotiation.
    \param user_data An opaque pointer passed to the result_handler routine.
    \return A pointer to the V.8 context, or NULL if there was a problem. */
v8_state_t *v8_init(v8_state_t *s,
                    int caller,
                    int available_modulations,
                    v8_result_handler_t *result_handler,
                    void *user_data);

/*! Release a V.8 context.
    \brief Release a V.8 context.
    \param s The V.8 context.
    \return 0 for OK. */
int v8_release(v8_state_t *s);

/*! Generate a block of V.8 audio samples.
    \brief Generate a block of V.8 audio samples.
    \param s The V.8 context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int v8_tx(v8_state_t *s, int16_t *amp, int max_len);

/*! Process a block of received V.8 audio samples.
    \brief Process a block of received V.8 audio samples.
    \param s The V.8 context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
*/
int v8_rx(v8_state_t *s, const int16_t *amp, int len);

/*! Log the list of supported modulations.
    \brief Log the list of supported modulations.
    \param s The V.8 context.
    \param modulation_schemes The list of supported modulations. */
void v8_log_supported_modulations(v8_state_t *s, int modulation_schemes);

const char *v8_call_function_to_str(int call_function);
const char *v8_modulation_to_str(int modulation_scheme);
const char *v8_protocol_to_str(int protocol);
const char *v8_pstn_access_to_str(int pstn_access);
const char *v8_pcm_modem_availability_to_str(int pcm_modem_availability);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
