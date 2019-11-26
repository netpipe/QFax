/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v8_tests.c
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
 * $Id: v8_tests.c,v 1.25 2008/05/13 13:17:27 steveu Exp $
 */

/*! \page v8_tests_page V.8 tests
\section v8_tests_page_sec_1 What does it do?
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

#define FALSE 0
#define TRUE (!FALSE)

#define SAMPLES_PER_CHUNK   160

#define OUTPUT_FILE_NAME    "v8.wav"

int negotiations_ok = 0;

static void handler(void *user_data, v8_result_t *result)
{
    const char *s;
    
    s = (const char *) user_data;
    
    if (result == NULL)
    {
        printf("%s V.8 negotiation failed\n", s);
        return;
    }
    printf("%s V.8 negotiation result:\n", s);
    printf("  Call function '%s'\n", v8_call_function_to_str(result->call_function));
    printf("  Negotiated modulation '%s'\n", v8_modulation_to_str(result->negotiated_modulation));
    printf("  Protocol '%s'\n", v8_protocol_to_str(result->protocol));
    printf("  PSTN access '%s'\n", v8_pstn_access_to_str(result->pstn_access));
    printf("  PCM modem availability '%s'\n", v8_pcm_modem_availability_to_str(result->pcm_modem_availability));
    if (result->call_function == V8_CALL_V_SERIES
        &&
        result->negotiated_modulation == V8_MOD_V90
        &&
        result->protocol == V8_PROTOCOL_LAPM_V42)
    {
        negotiations_ok++;
    }
}

int main(int argc, char *argv[])
{
    int i;
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    v8_state_t v8_caller;
    v8_state_t v8_answerer;
    int outframes;
    int samples;
    int remnant;
    int caller_available_modulations;
    int answerer_available_modulations;
    AFfilehandle inhandle;
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    int opt;
    char *decode_test_file;
    float x;
    
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

    caller_available_modulations = V8_MOD_V17
                                 | V8_MOD_V21
                                 | V8_MOD_V22
                                 | V8_MOD_V23HALF
                                 | V8_MOD_V23
                                 | V8_MOD_V26BIS
                                 | V8_MOD_V26TER
                                 | V8_MOD_V27TER
                                 | V8_MOD_V29
                                 | V8_MOD_V32
                                 | V8_MOD_V34HALF
                                 | V8_MOD_V34
                                 | V8_MOD_V90
                                 | V8_MOD_V92;
    answerer_available_modulations = V8_MOD_V17
                                   | V8_MOD_V21
                                   | V8_MOD_V22
                                   | V8_MOD_V23HALF
                                   | V8_MOD_V23
                                   | V8_MOD_V26BIS
                                   | V8_MOD_V26TER
                                   | V8_MOD_V27TER
                                   | V8_MOD_V29
                                   | V8_MOD_V32
                                   | V8_MOD_V34HALF
                                   | V8_MOD_V34
                                   | V8_MOD_V90
                                   | V8_MOD_V92;
    
    if (decode_test_file == NULL)
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
    
        v8_init(&v8_caller, TRUE, caller_available_modulations, handler, (void *) "caller");
        v8_init(&v8_answerer, FALSE, answerer_available_modulations, handler, (void *) "answerer");
        span_log_set_level(&v8_caller.logging, SPAN_LOG_FLOW | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(&v8_caller.logging, "caller");
        span_log_set_level(&v8_answerer.logging, SPAN_LOG_FLOW | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(&v8_answerer.logging, "answerer");
        for (i = 0;  i < 1000;  i++)
        {
            samples = v8_tx(&v8_caller, amp, SAMPLES_PER_CHUNK);
            if (samples < SAMPLES_PER_CHUNK)
            {
                memset(amp + samples, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - samples));
                samples = SAMPLES_PER_CHUNK;
            }
            remnant = v8_rx(&v8_answerer, amp, samples);
            for (i = 0;  i < samples;  i++)
                out_amp[2*i] = amp[i];
            
            samples = v8_tx(&v8_answerer, amp, SAMPLES_PER_CHUNK);
            if (samples < SAMPLES_PER_CHUNK)
            {
                memset(amp + samples, 0, sizeof(int16_t)*(SAMPLES_PER_CHUNK - samples));
                samples = SAMPLES_PER_CHUNK;
            }
            if (v8_rx(&v8_caller, amp, samples)  &&  remnant)
                break;
            for (i = 0;  i < samples;  i++)
                out_amp[2*i + 1] = amp[i];
    
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      out_amp,
                                      samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing wave file\n");
                exit(2);
            }
        }
        if (afCloseFile(outhandle))
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME);
            exit(2);
        }
        afFreeFileSetup(filesetup);
        
        v8_release(&v8_caller);
        v8_release(&v8_answerer);
        
        if (negotiations_ok != 2)
        {
            printf("Tests failed.\n");
            exit(2);
        }
        printf("Tests passed.\n");
    }
    else
    {
        printf("Decode file '%s'\n", decode_test_file);
        v8_init(&v8_answerer, FALSE, answerer_available_modulations, handler, (void *) "answerer");
        span_log_set_level(&v8_answerer.logging, SPAN_LOG_FLOW | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(&v8_answerer.logging, "decoder");
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
        while ((samples = afReadFrames(inhandle, AF_DEFAULT_TRACK, amp, SAMPLES_PER_CHUNK)))
        {
            remnant = v8_rx(&v8_answerer, amp, samples);
        }
        /*endwhile*/
        v8_release(&v8_answerer);
        if (afCloseFile(inhandle) != 0)
        {
            fprintf(stderr, "    Cannot close speech file '%s'\n", decode_test_file);
            exit(2);
        }
        /*endif*/
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
