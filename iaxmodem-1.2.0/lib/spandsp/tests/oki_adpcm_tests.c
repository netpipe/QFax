/*
 * SpanDSP - a series of DSP components for telephony
 *
 * oki_adpcm_tests.c - Test the Oki (Dialogic) ADPCM encode and decode
 *                     software at 24kbps and 32kbps.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: oki_adpcm_tests.c,v 1.31 2008/05/13 13:17:26 steveu Exp $
 */

/*! \file */

/*! \page oki_adpcm_tests_page OKI (Dialogic) ADPCM tests
\section oki_adpcm_tests_page_sec_1 What does it do?
To perform a general audio quality test, oki_adpcm_tests should be run. The test file
../test-data/local/short_nb_voice.wav will be compressed to the specified bit rate,
decompressed, and the resulting audio stored in post_oki_adpcm.wav. A simple SNR test
is automatically performed. Listening tests may be used for a more detailed evaluation
of the degradation in quality caused by the compression. Both 32k bps and 24k bps
compression may be tested.

\section oki_adpcm_tests_page_sec_2 How is it used?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <audiofile.h>

#include "spandsp.h"

#define IN_FILE_NAME    "../test-data/local/short_nb_voice.wav"
#define OUT_FILE_NAME   "post_oki_adpcm.wav"

#define HIST_LEN        1000

int main(int argc, char *argv[])
{
    int i;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int frames;
    int dec_frames;
    int outframes;
    int oki_bytes;
    int bit_rate;
    float x;
    double pre_energy;
    double post_energy;
    double diff_energy;
    int16_t pre_amp[HIST_LEN];
    int16_t post_amp[HIST_LEN];
    uint8_t oki_data[HIST_LEN];
    int16_t history[HIST_LEN];
    int hist_in;
    int hist_out;
    oki_adpcm_state_t *oki_enc_state;
    oki_adpcm_state_t *oki_dec_state;
    int xx;
    int total_pre_samples;
    int total_compressed_bytes;
    int total_post_samples;
    const char *in_file_name;
    int log_encoded_data;
    int opt;

    bit_rate = 32000;
    in_file_name = IN_FILE_NAME;
    log_encoded_data = FALSE;
    while ((opt = getopt(argc, argv, "2i:l")) != -1)
    {
        switch (opt)
        {
        case '2':
            bit_rate = 24000;
            break;
        case 'i':
            in_file_name = optarg;
            break;
        case 'l':
            log_encoded_data = TRUE;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    if ((inhandle = afOpenFile(in_file_name, "r", 0)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", in_file_name);
        exit(2);
    }
    if ((x = afGetFrameSize(inhandle, AF_DEFAULT_TRACK, 1)) != 2.0)
    {
        fprintf(stderr, "    Unexpected frame size in wave file '%s'\n", in_file_name);
        exit(2);
    }
    if ((x = afGetRate(inhandle, AF_DEFAULT_TRACK)) != (float) SAMPLE_RATE)
    {
        fprintf(stderr, "    Unexpected sample rate in wave file '%s'\n", in_file_name);
        exit(2);
    }
    if ((x = afGetChannels(inhandle, AF_DEFAULT_TRACK)) != 1.0)
    {
        fprintf(stderr, "    Unexpected number of channels in wave file '%s'\n", in_file_name);
        exit(2);
    }
    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    outhandle = afOpenFile(OUT_FILE_NAME, "w", filesetup);
    if (outhandle == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    if ((oki_enc_state = oki_adpcm_init(NULL, bit_rate)) == NULL)
    {
        fprintf(stderr, "    Cannot create encoder\n");
        exit(2);
    }
        
    if ((oki_dec_state = oki_adpcm_init(NULL, bit_rate)) == NULL)
    {
        fprintf(stderr, "    Cannot create decoder\n");
        exit(2);
    }

    hist_in = 0;
    if (bit_rate == 32000)
        hist_out = 0;
    else
        hist_out = HIST_LEN - 27;
    memset(history, 0, sizeof(history));
    pre_energy = 0.0;
    post_energy = 0.0;
    diff_energy = 0.0;
    total_pre_samples = 0;
    total_compressed_bytes = 0;
    total_post_samples = 0;
    while ((frames = afReadFrames(inhandle, AF_DEFAULT_TRACK, pre_amp, 159)))
    {
        total_pre_samples += frames;
        oki_bytes = oki_adpcm_encode(oki_enc_state, oki_data, pre_amp, frames);
        if (log_encoded_data)
            write(1, oki_data, oki_bytes);
        total_compressed_bytes += oki_bytes;
        dec_frames = oki_adpcm_decode(oki_dec_state, post_amp, oki_data, oki_bytes);
        total_post_samples += dec_frames;
        for (i = 0;  i < frames;  i++)
        {
            history[hist_in++] = pre_amp[i];
            if (hist_in >= HIST_LEN)
                hist_in = 0;
            pre_energy += (double) pre_amp[i] * (double) pre_amp[i];
        }
        for (i = 0;  i < dec_frames;  i++)
        {
            post_energy += (double) post_amp[i] * (double) post_amp[i];
            xx = post_amp[i] - history[hist_out++];
            if (hist_out >= HIST_LEN)
                hist_out = 0;
            diff_energy += (double) xx * (double) xx;
            //post_amp[i] = xx;
        }
        outframes = afWriteFrames(outhandle, AF_DEFAULT_TRACK, post_amp, dec_frames);
    }
    if (afCloseFile(inhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", in_file_name);
        exit(2);
    }
    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);
    oki_adpcm_release(oki_enc_state);
    oki_adpcm_release(oki_dec_state);

    printf("Pre samples: %d\n", total_pre_samples);
    printf("Compressed bytes: %d\n", total_compressed_bytes);
    printf("Post samples: %d\n", total_post_samples);

    printf("Output energy is %f%% of input energy.\n", 100.0*post_energy/pre_energy);
    printf("Residual energy is %f%% of the total.\n", 100.0*diff_energy/post_energy);
    if (bit_rate == 32000)
    {
        if (fabs(1.0 - post_energy/pre_energy) > 0.05
            ||
            fabs(diff_energy/post_energy) > 0.03)
        {
            printf("Tests failed.\n");
            exit(2);
        }
    }
    else
    {
        if (fabs(1.0 - post_energy/pre_energy) > 0.20
            ||
            fabs(diff_energy/post_energy) > 0.10)
        {
            printf("Tests failed.\n");
            exit(2);
        }
    }

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
