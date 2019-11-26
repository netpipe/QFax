/*
 * SpanDSP - a series of DSP components for telephony
 *
 * modem_connect_tones.c - Generation and detection of tones
 * associated with modems calling and answering calls.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: modem_connect_tones.h,v 1.17 2008/05/14 15:41:25 steveu Exp $
 */
 
/*! \file */

#if !defined(_SPANDSP_MODEM_CONNECT_TONES_H_)
#define _SPANDSP_MODEM_CONNECT_TONES_H_

/*! \page modem_connect_tones_page Modem connect tone detection

\section modem_connect_tones_page_sec_1 What does it do?
Some telephony terminal equipment, such as modems, require a channel which is as
clear as possible. They use their own echo cancellation. If the network is also
performing echo cancellation the two cancellors can end up squabbling about the
nature of the channel, with bad results. A special tone is defined which should
cause the network to disable any echo cancellation processes. This is the echo
canceller disable tone.

The tone detector's design assumes the channel is free of any DC component.

\section modem_connect_tones_page_sec_2 How does it work?
A sharp notch filter is implemented as a single bi-quad section. The presence of
the 2100Hz disable tone is detected by comparing the notched filtered energy
with the unfiltered energy. If the notch filtered energy is much lower than the
unfiltered energy, then a large proportion of the energy must be at the notch
frequency. This type of detector may seem less intuitive than using a narrow
bandpass filter to isolate the energy at the notch freqency. However, a sharp
bandpass implemented as an IIR filter rings badly. The reciprocal notch filter
is very well behaved for our purpose. 
*/

enum
{
    MODEM_CONNECT_TONES_NONE = 0,
    /*! \brief CNG tone is a pure 1100Hz tone, in 0.5s bursts, with 3s silences in between. The
               bursts repeat for as long as is required. */
    MODEM_CONNECT_TONES_FAX_CNG = 1,
    /*! \brief CED tone is a pure continuous 2100Hz+-15Hz tone for 3.3s+-0.7s. We might see FAX preamble
               instead of CED, of the FAX machine does not answer with CED. */
    MODEM_CONNECT_TONES_FAX_CED = 2,
    /*! \brief ANS tone is a pure continuous 2100Hz+-15Hz tone for 3.3s+-0.7s. Nothing else is searched for. */
    MODEM_CONNECT_TONES_ANS = 3,
    /*! \brief ANS with phase reversals tone is a 2100Hz+-15Hz tone for 3.3s+-0.7s, with a 180 degree phase
               jump every 450ms+-25ms. */
    MODEM_CONNECT_TONES_ANS_PR = 4,
    /*! \brief The ANSam tone is a version of ANS with 20% of 15Hz+-0.1Hz AM modulation, as per V.8 */
    MODEM_CONNECT_TONES_ANSAM = 5,
    /*! \brief The ANSam with phase reversals tone is a version of ANS_PR with 20% of 15Hz+-0.1Hz AM modulation,
               as per V.8 */
    MODEM_CONNECT_TONES_ANSAM_PR = 6,
    /*! \brief FAX preamble in a string of V.21 HDLC flag octets. This is only valid as a result of tone
               detection. It should not be specified as a tone type to transmit or receive. */
    MODEM_CONNECT_TONES_FAX_PREAMBLE = 7
};

/*!
    Modem connect tones generator descriptor. This defines the state
    of a single working instance of the tone generator.
*/
typedef struct
{
    int tone_type;

    int32_t tone_phase_rate;
    uint32_t tone_phase;
    int level;
    /*! \brief Countdown to the next phase hop */
    int hop_timer;
    /*! \brief Maximum duration timer */
    int duration_timer;
    uint32_t mod_phase;
    int32_t mod_phase_rate;
    int mod_level;
} modem_connect_tones_tx_state_t;

/*!
    Modem connect tones receiver descriptor. This defines the state
    of a single working instance of the tone detector.
*/
typedef struct
{
    /*! \brief The tone type being detected. */
    int tone_type;
    /*! \brief Callback routine, using to report detection of the tone. */
    tone_report_func_t tone_callback;
    /*! \brief An opaque pointer passed to tone_callback. */
    void *callback_data;

    /*! \brief The notch filter state. */
    float z1;
    float z2;
    /*! \brief The in notch power estimate */
    int notch_level;
    /*! \brief The total channel power estimate */
    int channel_level;
    /*! \brief Sample counter for the small chunks of samples, after which a test is conducted. */
    int chunk_remainder;
    /*! \brief TRUE is the tone is currently confirmed present in the audio. */
    int tone_present;
    /*! \brief */
    int tone_on;
    /*! \brief A millisecond counter, to time the duration of tone sections. */
    int tone_cycle_duration;
    /*! \brief A count of the number of good cycles of tone reversal seen. */
    int good_cycles;
    /*! \brief TRUE if the tone has been seen since the last time the user tested for it */
    int hit;
    /*! \brief A V.21 FSK modem context used when searching for FAX preamble. */
    fsk_rx_state_t v21rx;
    /*! \brief The raw (stuffed) bit stream buffer. */
    unsigned int raw_bit_stream;
    /*! \brief The current number of bits in the octet in progress. */
    int num_bits;
    /*! \brief Number of consecutive flags seen so far. */
    int flags_seen;
    /*! \brief TRUE if framing OK has been announced. */
    int framing_ok_announced;
} modem_connect_tones_rx_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Initialise an instance of the modem connect tones generator.
    \param s The context.
*/
modem_connect_tones_tx_state_t *modem_connect_tones_tx_init(modem_connect_tones_tx_state_t *s,
                                                            int tone_type);

/*! \brief Free an instance of the modem connect tones generator.
    \param s The context.
    \return 0 for OK, else -1.
*/
int modem_connect_tones_tx_free(modem_connect_tones_tx_state_t *s);

/*! \brief Generate a block of modem connect tones samples.
    \param s The context.
    \param amp An array of signal samples.
    \param len The number of samples to generate.
    \return The number of samples generated.
*/
int modem_connect_tones_tx(modem_connect_tones_tx_state_t *s,
                           int16_t amp[],
                           int len);

/*! \brief Process a block of samples through an instance of the modem connect
           tones detector.
    \param s The context.
    \param amp An array of signal samples.
    \param len The number of samples in the array.
    \return The number of unprocessed samples.
*/
int modem_connect_tones_rx(modem_connect_tones_rx_state_t *s,
                           const int16_t amp[],
                           int len);
                             
/*! \brief Test if a modem_connect tone has been detected.
    \param s The context.
    \return TRUE if tone is detected, else FALSE.
*/
int modem_connect_tones_rx_get(modem_connect_tones_rx_state_t *s);

/*! \brief Initialise an instance of the modem connect tones detector.
    \param s The context.
    \param tone_type The type of connect tone being tested for.
    \param tone_callback An optional callback routine, used to report tones
    \param user_data An opaque pointer passed to the callback routine,
    \return A pointer to the context.
*/
modem_connect_tones_rx_state_t *modem_connect_tones_rx_init(modem_connect_tones_rx_state_t *s,
                                                            int tone_type,
                                                            tone_report_func_t tone_callback,
                                                            void *user_data);

/*! \brief Free an instance of the modem connect tones detector.
    \param s The context.
    \return 0 for OK, else -1. */
int modem_connect_tones_rx_free(modem_connect_tones_rx_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
