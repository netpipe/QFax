/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fsk_tests.c - Tests for the low speed FSK modem code (V.21, V.23, etc.).
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
 * $Id: fsk_tests.c,v 1.45 2008/07/16 17:01:49 steveu Exp $
 */

/*! \page fsk_tests_page FSK modem tests
\section fsk_tests_page_sec_1 What does it do?
These tests allow either:

 - An FSK transmit modem to feed an FSK receive modem, of the same type,
   through a telephone line model. BER testing is then used to evaluate
   performance under various line conditions. This is effective for testing
   the basic performance of the receive modem. It is also the only test mode
   provided for evaluating the transmit modem.

 - An FSK receive modem is used to decode FSK audio, stored in a wave file.
   This is good way to evaluate performance with audio recorded from other
   models of modem, and with real world problematic telephone lines.

\section fsk_tests_page_sec_2 How does it work?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <audiofile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define BLOCK_LEN           160

#define OUTPUT_FILE_NAME    "fsk.wav"

char *decode_test_file = NULL;
both_ways_line_model_state_t *model;
int rx_bits = 0;
int cutoff_test_carrier = FALSE;

static int rx_status(void *user_data, int status)
{
    printf("FSK rx status is %d\n", status);
    switch (status)
    {
    case PUTBIT_TRAINING_FAILED:
        printf("Training failed\n");
        break;
    case PUTBIT_TRAINING_SUCCEEDED:
        printf("Training succeeded\n");
        break;
    case PUTBIT_CARRIER_UP:
        printf("Carrier up\n");
        break;
    case PUTBIT_CARRIER_DOWN:
        printf("Carrier down\n");
        break;
    default:
        printf("Eh! - %d\n", status);
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int tx_status(void *user_data, int status)
{
    switch (status)
    {
    case MODEM_TX_STATUS_DATA_EXHAUSTED:
        printf("FSK tx data exhausted\n");
        break;
    case MODEM_TX_STATUS_SHUTDOWN_COMPLETE:
        printf("FSK tx shutdown complete\n");
        break;
    default:
        printf("FSK tx status is %d\n", status);
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        rx_status(user_data, bit);
        return;
    }

    printf("Rx bit %d - %d\n", rx_bits++, bit);
}
/*- End of function --------------------------------------------------------*/

static int cutoff_test_rx_status(void *user_data, int status)
{
    printf("FSK rx status is %d\n", status);
    switch (status)
    {
    case PUTBIT_TRAINING_FAILED:
        printf("Training failed\n");
        break;
    case PUTBIT_TRAINING_SUCCEEDED:
        printf("Training succeeded\n");
        break;
    case PUTBIT_CARRIER_UP:
        //printf("Carrier up\n");
        cutoff_test_carrier = TRUE;
        break;
    case PUTBIT_CARRIER_DOWN:
        //printf("Carrier down\n");
        cutoff_test_carrier = FALSE;
        break;
    default:
        printf("Eh! - %d\n", status);
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void cutoff_test_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        cutoff_test_rx_status(user_data, bit);
        return;
    }
}
/*- End of function --------------------------------------------------------*/

static void reporter(void *user_data, int reason, bert_results_t *results)
{
    int channel;

    channel = (int) (intptr_t) user_data;
    switch (reason)
    {
    case BERT_REPORT_SYNCED:
        printf("%d: BERT report synced\n", channel);
        break;
    case BERT_REPORT_UNSYNCED:
        printf("%d: BERT report unsync'ed\n", channel);
        break;
    case BERT_REPORT_REGULAR:
        printf("%d: BERT report regular - %d bits, %d bad bits, %d resyncs\n", channel, results->total_bits, results->bad_bits, results->resyncs);
        break;
    case BERT_REPORT_GT_10_2:
        printf("%d: BERT report > 1 in 10^2\n", channel);
        break;
    case BERT_REPORT_LT_10_2:
        printf("%d: BERT report < 1 in 10^2\n", channel);
        break;
    case BERT_REPORT_LT_10_3:
        printf("%d: BERT report < 1 in 10^3\n", channel);
        break;
    case BERT_REPORT_LT_10_4:
        printf("%d: BERT report < 1 in 10^4\n", channel);
        break;
    case BERT_REPORT_LT_10_5:
        printf("%d: BERT report < 1 in 10^5\n", channel);
        break;
    case BERT_REPORT_LT_10_6:
        printf("%d: BERT report < 1 in 10^6\n", channel);
        break;
    case BERT_REPORT_LT_10_7:
        printf("%d: BERT report < 1 in 10^7\n", channel);
        break;
    default:
        printf("%d: BERT report reason %d\n", channel, reason);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    fsk_tx_state_t caller_tx;
    fsk_rx_state_t caller_rx;
    fsk_tx_state_t answerer_tx;
    fsk_rx_state_t answerer_rx;
    bert_state_t caller_bert;
    bert_state_t answerer_bert;
    bert_results_t bert_results;
    power_meter_t caller_meter;
    power_meter_t answerer_meter;
    int16_t caller_amp[BLOCK_LEN];
    int16_t answerer_amp[BLOCK_LEN];
    int16_t caller_model_amp[BLOCK_LEN];
    int16_t answerer_model_amp[BLOCK_LEN];
    int16_t out_amp[2*BLOCK_LEN];
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;    
    int i;
    int j;
    int samples;
    int test_bps;
    int noise_level;
    int noise_sweep;
    int bits_per_test;
    int line_model_no;
    int modem_under_test_1;
    int modem_under_test_2;
    int modems_set;
    int log_audio;
    int channel_codec;
    int rbs_pattern;
    int on_at;
    int off_at;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_tx;
    int opt;

    channel_codec = MUNGE_CODEC_NONE;
    rbs_pattern = 0;
    line_model_no = 0;
    decode_test_file = NULL;
    noise_sweep = FALSE;
    modem_under_test_1 = FSK_V21CH1;
    modem_under_test_2 = FSK_V21CH2;
    log_audio = FALSE;
    modems_set = 0;
    while ((opt = getopt(argc, argv, "c:dlm:nr:s:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            channel_codec = atoi(optarg);
            break;
        case 'd':
            decode_test_file = optarg;
            break;
        case 'l':
            log_audio = TRUE;
            break;
        case 'm':
            line_model_no = atoi(optarg);
            break;
        case 'n':
            noise_sweep = TRUE;
            break;
        case 'r':
            rbs_pattern = atoi(optarg);
            break;
        case 's':
            switch (modems_set++)
            {
            case 0:
                modem_under_test_1 = atoi(optarg);
                break;
            case 1:
                modem_under_test_2 = atoi(optarg);
                break;
            }
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    if (modem_under_test_1 >= 0)
        printf("Modem channel 1 is '%s'\n", preset_fsk_specs[modem_under_test_1].name);
    if (modem_under_test_2 >= 0)
        printf("Modem channel 2 is '%s'\n", preset_fsk_specs[modem_under_test_2].name);

    filesetup = AF_NULL_FILESETUP;
    outhandle = AF_NULL_FILEHANDLE;

    if (log_audio)
    {
        if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
        {
            fprintf(stderr, "    Failed to create file setup\n");
            exit(2);
        }
        afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
        afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
        afInitFileFormat(filesetup, AF_FILE_WAVE);
        afInitChannels(filesetup, AF_DEFAULT_TRACK, 2);
        if ((outhandle = afOpenFile(OUTPUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME);
            exit(2);
        }
    }
    noise_level = -200;
    bits_per_test = 0;
    inhandle = NULL;

    memset(caller_amp, 0, sizeof(*caller_amp));
    memset(answerer_amp, 0, sizeof(*answerer_amp));
    memset(caller_model_amp, 0, sizeof(*caller_model_amp));
    memset(answerer_model_amp, 0, sizeof(*answerer_model_amp));
    power_meter_init(&caller_meter, 7);
    power_meter_init(&answerer_meter, 7);

    if (decode_test_file)
    {
        if ((inhandle = afOpenFile(decode_test_file, "r", NULL)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open wave file '%s'\n", decode_test_file);
            exit(2);
        }
        fsk_rx_init(&caller_rx, &preset_fsk_specs[modem_under_test_1], TRUE, put_bit, NULL);
        fsk_rx_set_modem_status_handler(&caller_rx, rx_status, (void *) &caller_rx);
        test_bps = preset_fsk_specs[modem_under_test_1].baud_rate;

        for (;;)
        {
            samples = afReadFrames(inhandle,
                                   AF_DEFAULT_TRACK,
                                   caller_model_amp,
                                   BLOCK_LEN);
            if (samples < BLOCK_LEN)
                break;
            for (i = 0;  i < samples;  i++)
                power_meter_update(&caller_meter, caller_model_amp[i]);
            fsk_rx(&caller_rx, caller_model_amp, samples);
        }

        if (afCloseFile(inhandle) != 0)
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", decode_test_file);
            exit(2);
        }
    }
    else
    {
        printf("Test cutoff level\n");
        fsk_rx_init(&caller_rx, &preset_fsk_specs[modem_under_test_1], TRUE, cutoff_test_put_bit, NULL);
        fsk_rx_signal_cutoff(&caller_rx, -30.0f);
        fsk_rx_set_modem_status_handler(&caller_rx, cutoff_test_rx_status, (void *) &caller_rx);
        on_at = 0;
        for (i = -40;  i < -25;  i++)
        {
            make_tone_gen_descriptor(&tone_desc,
                                     1500,
                                     i,
                                     0,
                                     0,
                                     1,
                                     0,
                                     0,
                                     0,
                                     TRUE);
            tone_gen_init(&tone_tx, &tone_desc);
            for (j = 0;  j < 10;  j++)
            {
                samples = tone_gen(&tone_tx, caller_model_amp, 160);
                fsk_rx(&caller_rx, caller_model_amp, samples);
            }
            if (cutoff_test_carrier)
               break;
        }
        on_at = i;
        off_at = 0;
        for (  ;  i > -40;  i--)
        {
            make_tone_gen_descriptor(&tone_desc,
                                     1500,
                                     i,
                                     0,
                                     0,
                                     1,
                                     0,
                                     0,
                                     0,
                                     TRUE);
            tone_gen_init(&tone_tx, &tone_desc);
            for (j = 0;  j < 10;  j++)
            {
                samples = tone_gen(&tone_tx, caller_model_amp, 160);
                fsk_rx(&caller_rx, caller_model_amp, samples);
            }
            if (!cutoff_test_carrier)
                break;
        }
        off_at = i;
        printf("Carrier on at %d, off at %d\n", on_at, off_at);
        if (on_at < -29  ||  on_at > -26  
            ||
            off_at < -35  ||  off_at > -31)
        {
            printf("Tests failed.\n");
            exit(2);
        }
                
        printf("Test with BERT\n");
        test_bps = preset_fsk_specs[modem_under_test_1].baud_rate;
        if (modem_under_test_1 >= 0)
        {
            fsk_tx_init(&caller_tx, &preset_fsk_specs[modem_under_test_1], (get_bit_func_t) bert_get_bit, &caller_bert);
            fsk_tx_set_modem_status_handler(&caller_tx, tx_status, (void *) &caller_tx);
            fsk_rx_init(&answerer_rx, &preset_fsk_specs[modem_under_test_1], TRUE, (put_bit_func_t) bert_put_bit, &answerer_bert);
            fsk_rx_set_modem_status_handler(&answerer_rx, rx_status, (void *) &answerer_rx);
        }
        if (modem_under_test_2 >= 0)
        {
            fsk_tx_init(&answerer_tx, &preset_fsk_specs[modem_under_test_2], (get_bit_func_t) bert_get_bit, &answerer_bert);
            fsk_tx_set_modem_status_handler(&answerer_tx, tx_status, (void *) &answerer_tx);
            fsk_rx_init(&caller_rx, &preset_fsk_specs[modem_under_test_2], TRUE, (put_bit_func_t) bert_put_bit, &caller_bert);
            fsk_rx_set_modem_status_handler(&caller_rx, rx_status, (void *) &caller_rx);
        }
        test_bps = preset_fsk_specs[modem_under_test_1].baud_rate;

        bits_per_test = 500000;
        noise_level = -24;

        bert_init(&caller_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
        bert_set_report(&caller_bert, 100000, reporter, (void *) (intptr_t) 1);
        bert_init(&answerer_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
        bert_set_report(&answerer_bert, 100000, reporter, (void *) (intptr_t) 2);
        if ((model = both_ways_line_model_init(line_model_no, (float) noise_level, line_model_no, noise_level, channel_codec, rbs_pattern)) == NULL)
        {
            fprintf(stderr, "    Failed to create line model\n");
            exit(2);
        }

        for (;;)
        {
            samples = fsk_tx(&caller_tx, caller_amp, BLOCK_LEN);
            for (i = 0;  i < samples;  i++)
                power_meter_update(&caller_meter, caller_amp[i]);
            samples = fsk_tx(&answerer_tx, answerer_amp, BLOCK_LEN);
            for (i = 0;  i < samples;  i++)
                power_meter_update(&answerer_meter, answerer_amp[i]);
            both_ways_line_model(model,
                                 caller_model_amp,
                                 caller_amp,
                                 answerer_model_amp,
                                 answerer_amp,
                                 samples);

            //printf("Powers %10.5fdBm0 %10.5fdBm0\n", power_meter_current_dbm0(&caller_meter), power_meter_current_dbm0(&answerer_meter));

            fsk_rx(&answerer_rx, caller_model_amp, samples);
            for (i = 0;  i < samples;  i++)
                out_amp[2*i] = caller_model_amp[i];
            for (  ;  i < BLOCK_LEN;  i++)
                out_amp[2*i] = 0;

            fsk_rx(&caller_rx, answerer_model_amp, samples);
            for (i = 0;  i < samples;  i++)
                out_amp[2*i + 1] = answerer_model_amp[i];
            for (  ;  i < BLOCK_LEN;  i++)
                out_amp[2*i + 1] = 0;
        
            if (log_audio)
            {
                outframes = afWriteFrames(outhandle,
                                          AF_DEFAULT_TRACK,
                                          out_amp,
                                          BLOCK_LEN);
                if (outframes != BLOCK_LEN)
                {
                    fprintf(stderr, "    Error writing wave file\n");
                    exit(2);
                }
            }

            if (samples < BLOCK_LEN)
            {
                bert_result(&caller_bert, &bert_results);
                fprintf(stderr, "%ddB AWGN, %d bits, %d bad bits, %d resyncs\n", noise_level, bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
                if (!noise_sweep)
                {
                    if (bert_results.total_bits != bits_per_test - 43
                        ||
                        bert_results.bad_bits != 0
                        ||
                        bert_results.resyncs != 0)
                    {
                        printf("Tests failed.\n");
                        exit(2);
                    }
                }
                bert_result(&answerer_bert, &bert_results);
                fprintf(stderr, "%ddB AWGN, %d bits, %d bad bits, %d resyncs\n", noise_level, bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
                if (!noise_sweep)
                {
                    if (bert_results.total_bits != bits_per_test - 43
                        ||
                        bert_results.bad_bits != 0
                        ||
                        bert_results.resyncs != 0)
                    {
                        printf("Tests failed.\n");
                        exit(2);
                    }
                    break;
                }
    
                /* Put a little silence between the chunks in the file. */
                memset(out_amp, 0, sizeof(out_amp));
                if (log_audio)
                {
                    for (i = 0;  i < 200;  i++)
                    {
                        outframes = afWriteFrames(outhandle,
                                                  AF_DEFAULT_TRACK,
                                                  out_amp,
                                                  BLOCK_LEN);
                    }
                }
                if (modem_under_test_1 >= 0)
                {
                    fsk_tx_init(&caller_tx, &preset_fsk_specs[modem_under_test_1], (get_bit_func_t) bert_get_bit, &caller_bert);
                    fsk_tx_set_modem_status_handler(&caller_tx, tx_status, (void *) &caller_tx);
                    fsk_rx_init(&answerer_rx, &preset_fsk_specs[modem_under_test_1], TRUE, (put_bit_func_t) bert_put_bit, &answerer_bert);
                    fsk_rx_set_modem_status_handler(&answerer_rx, rx_status, (void *) &answerer_rx);
                }
                if (modem_under_test_2 >= 0)
                {
                    fsk_tx_init(&answerer_tx, &preset_fsk_specs[modem_under_test_2], (get_bit_func_t) bert_get_bit, &answerer_bert);
                    fsk_tx_set_modem_status_handler(&answerer_tx, tx_status, (void *) &answerer_tx);
                    fsk_rx_init(&caller_rx, &preset_fsk_specs[modem_under_test_2], TRUE, (put_bit_func_t) bert_put_bit, &caller_bert);
                    fsk_rx_set_modem_status_handler(&caller_rx, rx_status, (void *) &caller_rx);
                }
                noise_level++;
                if ((model = both_ways_line_model_init(line_model_no, (float) noise_level, line_model_no, noise_level, channel_codec, 0)) == NULL)
                {
                    fprintf(stderr, "    Failed to create line model\n");
                    exit(2);
                }
                bert_init(&caller_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
                bert_set_report(&caller_bert, 100000, reporter, (void *) (intptr_t) 1);
                bert_init(&answerer_bert, bits_per_test, BERT_PATTERN_ITU_O152_11, test_bps, 20);
                bert_set_report(&answerer_bert, 100000, reporter, (void *) (intptr_t) 2);
            }
        }
        printf("Tests passed.\n");
    }
    if (log_audio)
    {
        if (afCloseFile(outhandle) != 0)
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME);
            exit(2);
        }
        afFreeFileSetup(filesetup);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
