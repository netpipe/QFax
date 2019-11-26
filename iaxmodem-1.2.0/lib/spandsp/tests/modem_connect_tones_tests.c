/*
 * SpanDSP - a series of DSP components for telephony
 *
 * modem_connect_tones_tests.c
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
 * $Id: modem_connect_tones_tests.c,v 1.21 2008/05/14 15:41:25 steveu Exp $
 */

/*! \page modem_connect_tones_tests_page Modem connect tones tests
\section modem_connect_tones_rx_tests_page_sec_1 What does it do?
These tests...
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <audiofile.h>

#include "spandsp.h"

#define SAMPLES_PER_CHUNK           160

#define OUTPUT_FILE_NAME            "modem_connect_tones.wav"

#define MITEL_DIR                   "../test-data/mitel/"
#define BELLCORE_DIR                "../test-data/bellcore/"

#define FALSE 0
#define TRUE (!FALSE)

#define LEVEL_MAX                   0
#define LEVEL_MIN                   -48
#define LEVEL_MIN_ACCEPT            -43
#define LEVEL_MIN_REJECT            -44

const char *bellcore_files[] =
{
    MITEL_DIR    "mitel-cm7291-talkoff.wav",
    BELLCORE_DIR "tr-tsy-00763-1.wav",
    BELLCORE_DIR "tr-tsy-00763-2.wav",
    BELLCORE_DIR "tr-tsy-00763-3.wav",
    BELLCORE_DIR "tr-tsy-00763-4.wav",
    BELLCORE_DIR "tr-tsy-00763-5.wav",
    BELLCORE_DIR "tr-tsy-00763-6.wav",
    ""
};

enum
{
    PERFORM_TEST_1A = (1 << 1),
    PERFORM_TEST_1B = (1 << 2),
    PERFORM_TEST_1C = (1 << 3),
    PERFORM_TEST_1D = (1 << 4),
    PERFORM_TEST_2A = (1 << 5),
    PERFORM_TEST_2B = (1 << 6),
    PERFORM_TEST_2C = (1 << 7),
    PERFORM_TEST_3A = (1 << 8),
    PERFORM_TEST_3B = (1 << 9),
    PERFORM_TEST_3C = (1 << 10),
    PERFORM_TEST_4 = (1 << 11),
    PERFORM_TEST_5 = (1 << 12),
    PERFORM_TEST_6 = (1 << 13)
};

int preamble_count = 0;
int preamble_on_at = -1;
int preamble_off_at = -1;
int hits = 0;
int when = 0;

static int preamble_get_bit(void *user_data)
{
    static int bit_no = 0;
    int bit;
    
    /* Generate a section of HDLC flag octet preamble. Then generate some random
       bits, which should not look like preamble. */
    if (++preamble_count < 255)
    {
        bit = (bit_no < 2)  ?  0  :  1;
        if (++bit_no >= 8)
            bit_no = 0;
#if 0
        /* Inject some bad bits */
        if (rand()%15 == 0)
            return bit ^ 1;
#endif
    }
    else
    {
        bit = rand() & 1;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void preamble_detected(void *user_data, int on, int level, int delay)
{
    printf("Preamble declared %s at bit %d (%ddBm0)\n", (on)  ?  "on"  :  "off", preamble_count, level);
    if (on)
        preamble_on_at = preamble_count;
    else
        preamble_off_at = preamble_count;
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void ced_detected(void *user_data, int on, int level, int delay)
{
    printf("FAX CED declared %s at %d (%ddBm0)\n", (on)  ?  "on"  :  "off", when, level);
    if (on)
        hits++;
}
/*- End of function --------------------------------------------------------*/

static void cng_detected(void *user_data, int on, int level, int delay)
{
    printf("FAX CNG declared %s at %d (%ddBm0)\n", (on)  ?  "on"  :  "off", when, level);
    if (on)
        hits++;
}
/*- End of function --------------------------------------------------------*/

static void ec_dis_detected(void *user_data, int on, int level, int delay)
{
    printf("EC disable tone declared %s at %d (%ddBm0)\n", (on)  ?  "on"  :  "off", when, level);
    if (on)
        hits++;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int i;
    int j;
    int pitch;
    int level;
    int interval;
    int16_t amp[8000];
    modem_connect_tones_rx_state_t cng_rx;
    modem_connect_tones_rx_state_t ced_rx;
    modem_connect_tones_rx_state_t ans_pr_rx;
    modem_connect_tones_tx_state_t modem_tone_tx;
    awgn_state_t chan_noise_source;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int outframes;
    int frames;
    int samples;
    int hit;
    int false_hit;
    int false_miss;
    float x;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_tx;
    power_meter_t power_state;
    int power;
    int max_power;
    int level2;
    int max_level2;
    int test_list;
    int opt;
    char *decode_test_file;
    fsk_tx_state_t preamble_tx;

    test_list = 0;
    decode_test_file = NULL;
    while ((opt = getopt(argc, argv, "d:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            decode_test_file = optarg;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    argc -= optind;
    argv += optind;
    for (i = 0;  i < argc;  i++)
    {
        if (strcasecmp(argv[i], "1a") == 0)
            test_list |= PERFORM_TEST_1A;
        else if (strcasecmp(argv[i], "1b") == 0)
            test_list |= PERFORM_TEST_1B;
        else if (strcasecmp(argv[i], "1c") == 0)
            test_list |= PERFORM_TEST_1C;
        else if (strcasecmp(argv[i], "1d") == 0)
            test_list |= PERFORM_TEST_1D;
        else if (strcasecmp(argv[i], "2a") == 0)
            test_list |= PERFORM_TEST_2A;
        else if (strcasecmp(argv[i], "2b") == 0)
            test_list |= PERFORM_TEST_2B;
        else if (strcasecmp(argv[i], "2c") == 0)
            test_list |= PERFORM_TEST_2C;
        else if (strcasecmp(argv[i], "3a") == 0)
            test_list |= PERFORM_TEST_3A;
        else if (strcasecmp(argv[i], "3b") == 0)
            test_list |= PERFORM_TEST_3B;
        else if (strcasecmp(argv[i], "3c") == 0)
            test_list |= PERFORM_TEST_3C;
        else if (strcasecmp(argv[i], "4") == 0)
            test_list |= PERFORM_TEST_4;
        else if (strcasecmp(argv[i], "5") == 0)
            test_list |= PERFORM_TEST_5;
        else if (strcasecmp(argv[i], "6") == 0)
            test_list |= PERFORM_TEST_6;
        else
        {
            fprintf(stderr, "Unknown test '%s' specified\n", argv[i]);
            exit(2);
        }
    }
    if (decode_test_file == NULL  &&  test_list == 0)
        test_list = 0xFFFFFFFF;

    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    if ((outhandle = afOpenFile(OUTPUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    if ((test_list & PERFORM_TEST_1A))
    {
        printf("Test 1a: CNG generation to a file\n");
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_FAX_CNG);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      amp,
                                      samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing wave file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    
    if ((test_list & PERFORM_TEST_1B))
    {
        printf("Test 1b: CED generation to a file\n");
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_FAX_CED);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      amp,
                                      samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing wave file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_1C))
    {
        printf("Test 1c: Modulated EC-disable generation to a file\n");
        /* Some with modulation */
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_ANSAM);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      amp,
                                      samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing wave file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_1D))
    {
        printf("Test 1d: EC-disable generation to a file\n");
        /* Some without modulation */
        modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_ANS_PR);
        for (i = 0;  i < 20*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      amp,
                                      samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing wave file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endfor*/
    }
    /*endif*/
    
    if (afCloseFile(outhandle) != 0)
    {
        printf("    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }
    /*endif*/
    
    if ((test_list & PERFORM_TEST_2A))
    {
        printf("Test 2a: CNG detection with frequency\n");
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = FALSE;
        false_miss = FALSE;
        for (pitch = 600;  pitch < 1600;  pitch++)
        {
            make_tone_gen_descriptor(&tone_desc,
                                     pitch,
                                     -11,
                                     0,
                                     0,
                                     425,
                                     3000,
                                     0,
                                     0,
                                     TRUE);
            tone_gen_init(&tone_tx, &tone_desc);

            modem_connect_tones_rx_init(&cng_rx, MODEM_CONNECT_TONES_FAX_CNG, NULL, NULL);
            power_meter_init(&power_state, 5);
            power = 0;
            max_power = 0;
            level2 = 0;
            max_level2 = 0;
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = tone_gen(&tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                {
                    amp[j] += awgn(&chan_noise_source);
                    power = power_meter_update(&power_state, amp[j]);
                    if (power > max_power)
                        max_power = power;
                    /*endif*/
                    level2 += ((abs(amp[j]) - level2) >> 5);
                    if (level2 > max_level2)
                        max_level2 = level2;
                }
                /*endfor*/
                modem_connect_tones_rx(&cng_rx, amp, samples);
            }
            /*endfor*/
//printf("max power is %d %f\n", max_power, log10f((float) max_power/(32767.0f*32767.0f))*10.0f + DBM0_MAX_POWER);
//printf("level2 %d (%f)\n", max_level2, log10f((float) max_level2/32768.0f)*20.0f + DBM0_MAX_POWER);
            hit = modem_connect_tones_rx_get(&cng_rx);
            if (pitch < (1100 - 70)  ||  pitch > (1100 + 70))
            {
                if (hit == MODEM_CONNECT_TONES_FAX_CNG)
                    false_hit = TRUE;
                /*endif*/
            }
            else if (pitch > (1100 - 50)  &&  pitch < (1100 + 50))
            {
                if (hit != MODEM_CONNECT_TONES_FAX_CNG)
                    false_miss = TRUE;
                /*endif*/
            }
            /*endif*/
            if (hit)
                printf("Detected at %5dHz %12d %12d %d\n", pitch, cng_rx.channel_level, cng_rx.notch_level, hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/
    
    if ((test_list & PERFORM_TEST_2B))
    {
        printf("Test 2b: CED detection with frequency\n");
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = FALSE;
        false_miss = FALSE;
        for (pitch = 1600;  pitch < 2600;  pitch++)
        {
            make_tone_gen_descriptor(&tone_desc,
                                     pitch,
                                     -11,
                                     0,
                                     0,
                                     2600,
                                     0,
                                     0,
                                     0,
                                     FALSE);
            tone_gen_init(&tone_tx, &tone_desc);

            modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = tone_gen(&tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ced_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ced_rx);
            if (pitch < (2100 - 70)  ||  pitch > (2100 + 70))
            {
                if (hit == MODEM_CONNECT_TONES_FAX_CED)
                    false_hit = TRUE;
            }
            else if (pitch > (2100 - 50)  &&  pitch < (2100 + 50))
            {
                if (hit != MODEM_CONNECT_TONES_FAX_CED)
                    false_miss = TRUE;
            }
            /*endif*/
            if (hit)
                printf("Detected at %5dHz %12d %12d %d\n", pitch, ced_rx.channel_level, ced_rx.notch_level, hit);
            /*endif*/
        }
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_2C))
    {
        printf("Test 2c: EC disable detection with frequency\n");
        awgn_init_dbm0(&chan_noise_source, 7162534, -50.0f);
        false_hit = FALSE;
        false_miss = FALSE;
        for (pitch = 2000;  pitch < 2200;  pitch++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_ANS_PR);
            /* Fudge things for the test */
            modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
            modem_tone_tx.level = dds_scaling_dbm0(-25);
            modem_connect_tones_rx_init(&ans_pr_rx, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ans_pr_rx);
            if (pitch < (2100 - 70)  ||  pitch > (2100 + 70))
            {
                if (hit == MODEM_CONNECT_TONES_ANS_PR)
                    false_hit = TRUE;
                /*endif*/
            }
            else if (pitch > (2100 - 50)  &&  pitch < (2100 + 50))
            {
                if (hit != MODEM_CONNECT_TONES_ANS_PR)
                    false_miss = TRUE;
                /*endif*/
            }
            /*endif*/
            if (hit)
                printf("Detected at %5dHz %12d %12d %d\n", pitch, ans_pr_rx.channel_level, ans_pr_rx.notch_level, hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3A))
    {
        printf("Test 3a: CNG detection with level\n");
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = FALSE;
        false_miss = FALSE;
        for (pitch = 1062;  pitch <= 1138;  pitch += 2*38)
        {
            for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            {
                make_tone_gen_descriptor(&tone_desc,
                                         pitch,
                                         level,
                                         0,
                                         0,
                                         500,
                                         3000,
                                         0,
                                         0,
                                         TRUE);
                tone_gen_init(&tone_tx, &tone_desc);

                modem_connect_tones_rx_init(&cng_rx, MODEM_CONNECT_TONES_FAX_CNG, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = tone_gen(&tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&cng_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&cng_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit == MODEM_CONNECT_TONES_FAX_CNG)
                    {
                        printf("False hit %d at %ddB\n", hit, level);
                        false_hit = TRUE;
                    }
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != MODEM_CONNECT_TONES_FAX_CNG)
                    {
                        printf("False miss %d at %ddB\n", hit, level);
                        false_miss = TRUE;
                    }
                    /*endif*/
                }
                /*endif*/
                if (hit)
                    printf("Detected at %5dHz %4ddB %12d %12d %d\n", pitch, level, cng_rx.channel_level, cng_rx.notch_level, hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3B))
    {
        printf("Test 3b: CED detection with level\n");
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = FALSE;
        false_miss = FALSE;
        for (pitch = 2062;  pitch <= 2138;  pitch += 2*38)
        {
            for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            {
                make_tone_gen_descriptor(&tone_desc,
                                         pitch,
                                         level,
                                         0,
                                         0,
                                         2600,
                                         0,
                                         0,
                                         0,
                                         FALSE);
                tone_gen_init(&tone_tx, &tone_desc);
                modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = tone_gen(&tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&ced_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&ced_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit == MODEM_CONNECT_TONES_FAX_CED)
                        false_hit = TRUE;
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != MODEM_CONNECT_TONES_FAX_CED)
                        false_miss = TRUE;
                    /*endif*/
                }
                /*endif*/
                if (hit)
                    printf("Detected at %5dHz %4ddB %12d %12d %d\n", pitch, level, ced_rx.channel_level, ced_rx.notch_level, hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_3C))
    {
        printf("Test 3c: EC disable detection with level\n");
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = FALSE;
        false_miss = FALSE;
        for (pitch = 2062;  pitch <= 2138;  pitch += 2*38)
        {
            for (level = LEVEL_MAX;  level >= LEVEL_MIN;  level--)
            {
                /* Use the transmitter to test the receiver */
                modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_ANS_PR);
                /* Fudge things for the test */
                modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
                modem_tone_tx.level = dds_scaling_dbm0(level);
                modem_connect_tones_rx_init(&ans_pr_rx, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);
                for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
                {
                    samples = modem_connect_tones_tx(&modem_tone_tx, amp, SAMPLES_PER_CHUNK);
                    for (j = 0;  j < samples;  j++)
                        amp[j] += awgn(&chan_noise_source);
                    /*endfor*/
                    modem_connect_tones_rx(&ans_pr_rx, amp, samples);
                }
                /*endfor*/
                hit = modem_connect_tones_rx_get(&ans_pr_rx);
                if (level < LEVEL_MIN_REJECT)
                {
                    if (hit == MODEM_CONNECT_TONES_ANS_PR)
                        false_hit = TRUE;
                    /*endif*/
                }
                else if (level > LEVEL_MIN_ACCEPT)
                {
                    if (hit != MODEM_CONNECT_TONES_ANS_PR)
                        false_miss = TRUE;
                    /*endif*/
                }
                /*endif*/
                if (hit)
                    printf("Detected at %5dHz %4ddB %12d %12d %d\n", pitch, level, ans_pr_rx.channel_level, ans_pr_rx.notch_level, hit);
                /*endif*/
            }
            /*endfor*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_4))
    {
        printf("Test 4: CED detection, when stimulated with V.21 preamble\n");
        false_hit = FALSE;
        false_miss = FALSE;

        /* Send 255 bits of preamble (0.85s, the minimum specified preamble for T.30), and then
           some random bits. Check the preamble detector comes on, and goes off at reasonable times. */
        fsk_tx_init(&preamble_tx, &preset_fsk_specs[FSK_V21CH2], preamble_get_bit, NULL);
        modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED, preamble_detected, NULL);
        for (i = 0;  i < 2*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
        {
            samples = fsk_tx(&preamble_tx, amp, SAMPLES_PER_CHUNK);
            modem_connect_tones_rx(&ced_rx, amp, samples);
        }
        /*endfor*/
        for (i = 0;  i < SAMPLE_RATE/10;  i += SAMPLES_PER_CHUNK)
        {
            memset(amp, 0, sizeof(int16_t)*SAMPLES_PER_CHUNK);
            modem_connect_tones_rx(&ced_rx, amp, SAMPLES_PER_CHUNK);
        }
        /*endfor*/
        if (preamble_on_at < 40  ||  preamble_on_at > 50
            ||
            preamble_off_at < 580  ||  preamble_off_at > 620)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_5))
    {
        printf("Test 5: EC disable detection with reversal interval\n");
        awgn_init_dbm0(&chan_noise_source, 7162534, -60.0f);
        false_hit = FALSE;
        false_miss = FALSE;
        pitch = 2100;
        level = -15;
        for (interval = 400;  interval < 500;  interval++)
        {
            /* Use the transmitter to test the receiver */
            modem_connect_tones_tx_init(&modem_tone_tx, MODEM_CONNECT_TONES_ANS_PR);
            /* Fudge things for the test */
            modem_tone_tx.tone_phase_rate = dds_phase_rate(pitch);
            modem_tone_tx.level = dds_scaling_dbm0(level);
            modem_connect_tones_rx_init(&ans_pr_rx, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);
            for (i = 0;  i < 10*SAMPLE_RATE;  i += SAMPLES_PER_CHUNK)
            {
                samples = SAMPLES_PER_CHUNK;
                for (j = 0;  j < samples;  j++)
                {
                    if (--modem_tone_tx.hop_timer <= 0)
                    {
                        modem_tone_tx.hop_timer = ms_to_samples(interval);
                        modem_tone_tx.tone_phase += 0x80000000;
                    }
                    amp[j] = dds_mod(&modem_tone_tx.tone_phase, modem_tone_tx.tone_phase_rate, modem_tone_tx.level, 0);
                }
                for (j = 0;  j < samples;  j++)
                    amp[j] += awgn(&chan_noise_source);
                /*endfor*/
                modem_connect_tones_rx(&ans_pr_rx, amp, samples);
            }
            /*endfor*/
            hit = modem_connect_tones_rx_get(&ans_pr_rx);
            if (interval < (450 - 25)  ||  interval > (450 + 25))
            {
                if (hit == MODEM_CONNECT_TONES_ANS_PR)
                    false_hit = TRUE;
            }
            else if (interval > (450 - 25)  &&  interval < (450 + 25))
            {
                if (hit != MODEM_CONNECT_TONES_ANS_PR)
                    false_miss = TRUE;
            }
            /*endif*/
            if (hit)
                printf("Detected at %5dHz %4ddB %dms %12d %12d %d\n", pitch, level, interval, ans_pr_rx.channel_level, ans_pr_rx.notch_level, hit);
            /*endif*/
        }
        /*endfor*/
        if (false_hit  ||  false_miss)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if ((test_list & PERFORM_TEST_6))
    {
        /* Talk-off test */
        /* Here we use the BellCore and Mitel talk off test tapes, intended for DTMF
           detector testing. Presumably they should also have value here, but I am not
           sure. If those voice snippets were chosen to be tough on DTMF detectors, they
           might go easy on detectors looking for different pitches. However, the
           Mitel DTMF test tape is known (the hard way) to exercise 2280Hz tone
           detectors quite well. */
        printf("Test 6: Talk-off test\n");
        modem_connect_tones_rx_init(&cng_rx, MODEM_CONNECT_TONES_FAX_CNG, NULL, NULL);
        modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED, NULL, NULL);
        modem_connect_tones_rx_init(&ans_pr_rx, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);
        hits = 0;
        for (j = 0;  bellcore_files[j][0];  j++)
        {
            if ((inhandle = afOpenFile(bellcore_files[j], "r", 0)) == AF_NULL_FILEHANDLE)
            {
                fprintf(stderr, "    Cannot open speech file '%s'\n", bellcore_files[j]);
                exit (2);
            }
            /*endif*/
            if ((x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
            {
                fprintf(stderr, "    Unexpected frame size in speech file '%s'\n", bellcore_files[j]);
                exit (2);
            }
            /*endif*/
            if ((x = afGetRate(inhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
            {
                fprintf(stderr, "    Unexpected sample rate in speech file '%s'\n", bellcore_files[j]);
                exit(2);
            }
            /*endif*/
            if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
            {
                fprintf(stderr, "    Unexpected number of channels in speech file '%s'\n", bellcore_files[j]);
                exit(2);
            }
            /*endif*/

            when = 0;
            hits = 0;
            while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, amp, 8000)))
            {
                when++;
                modem_connect_tones_rx(&cng_rx, amp, frames);
                modem_connect_tones_rx(&ced_rx, amp, frames);
                modem_connect_tones_rx(&ans_pr_rx, amp, frames);
                if (modem_connect_tones_rx_get(&cng_rx) != MODEM_CONNECT_TONES_NONE)
                {
                    /* This is not a true measure of hits, as there might be more
                       than one in a block of data. However, since the only good
                       result is no hits, this approximation is OK. */
                    printf("Hit CNG at %ds\n", when);
                    hits++;
                    modem_connect_tones_rx_init(&cng_rx, MODEM_CONNECT_TONES_FAX_CNG, NULL, NULL);
                }
                /*endif*/
                if (modem_connect_tones_rx_get(&ced_rx) != MODEM_CONNECT_TONES_NONE)
                {
                    printf("Hit CED at %ds\n", when);
                    hits++;
                    modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED, NULL, NULL);
                }
                /*endif*/
                if (modem_connect_tones_rx_get(&ans_pr_rx) != MODEM_CONNECT_TONES_NONE)
                {
                    printf("Hit EC disable at %ds\n", when);
                    hits++;
                    modem_connect_tones_rx_init(&ans_pr_rx, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);
                }
                /*endif*/
            }
            /*endwhile*/
            if (afCloseFile(inhandle) != 0)
            {
                fprintf(stderr, "    Cannot close speech file '%s'\n", bellcore_files[j]);
                exit(2);
            }
            /*endif*/
            printf("    File %d gave %d false hits.\n", j + 1, hits);
        }
        /*endfor*/
        if (hits > 0)
        {
            printf("Test failed.\n");
            exit(2);
        }
        /*endif*/
        printf("Test passed.\n");
    }
    /*endif*/

    if (decode_test_file)
    {
        printf("Decode file '%s'\n", decode_test_file);
        modem_connect_tones_rx_init(&cng_rx, MODEM_CONNECT_TONES_FAX_CNG, cng_detected, NULL);
        modem_connect_tones_rx_init(&ced_rx, MODEM_CONNECT_TONES_FAX_CED, ced_detected, NULL);
        modem_connect_tones_rx_init(&ans_pr_rx, MODEM_CONNECT_TONES_ANS_PR, ec_dis_detected, NULL);
        hits = 0;
        if ((inhandle = afOpenFile(decode_test_file, "r", 0)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot open speech file '%s'\n", decode_test_file);
            exit (2);
        }
        /*endif*/
        if ((x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
        {
            fprintf(stderr, "    Unexpected frame size in speech file '%s'\n", decode_test_file);
            exit (2);
        }
        /*endif*/
        if ((x = afGetRate(inhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
        {
            fprintf(stderr, "    Unexpected sample rate in speech file '%s'\n", decode_test_file);
            exit(2);
        }
        /*endif*/
        if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
        {
            fprintf(stderr, "    Unexpected number of channels in speech file '%s'\n", decode_test_file);
            exit(2);
        }
        /*endif*/

        when = 0;
        hits = 0;
        while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, amp, 8000)))
        {
            when++;
            modem_connect_tones_rx(&cng_rx, amp, frames);
            modem_connect_tones_rx(&ced_rx, amp, frames);
            modem_connect_tones_rx(&ans_pr_rx, amp, frames);
        }
        /*endwhile*/
        if (afCloseFile(inhandle) != 0)
        {
            fprintf(stderr, "    Cannot close speech file '%s'\n", decode_test_file);
            exit(2);
        }
        /*endif*/
        printf("    File gave %d hits.\n", hits);
    }
    printf("Tests passed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
