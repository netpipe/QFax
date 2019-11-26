/*
 * SpanDSP - a series of DSP components for telephony
 *
 * faxtester_tests.c
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
 * $Id: tsb85_tests.c,v 1.8 2008/07/25 13:56:54 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
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
#include <unistd.h>
#include <audiofile.h>

#if defined(HAVE_LIBXML_XMLMEMORY_H)
#include <libxml/xmlmemory.h>
#endif
#if defined(HAVE_LIBXML_PARSER_H)
#include <libxml/parser.h>
#endif
#if defined(HAVE_LIBXML_XINCLUDE_H)
#include <libxml/xinclude.h>
#endif

#include "spandsp.h"
#include "fax_tester.h"

#define INPUT_TIFF_FILE_NAME    "../test-data/itu/fax/itutests.tif"
#define OUTPUT_TIFF_FILE_NAME   "tsb85.tif"

#define OUTPUT_FILE_NAME_WAVE   "tsb85.wav"

#define SAMPLES_PER_CHUNK       160

AFfilehandle out_handle;
AFfilesetup filesetup;

int use_receiver_not_ready = FALSE;
int test_local_interrupt = FALSE;

faxtester_state_t state;

uint8_t image[1000000];

static int next_step(faxtester_state_t *s);

static int phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    const char *u;
    
    i = (intptr_t) user_data;
    if ((u = t30_get_rx_ident(s)))
        printf("%d: Phase B: remote ident '%s'\n", i, u);
    if ((u = t30_get_rx_sub_address(s)))
        printf("%d: Phase B: remote sub-address '%s'\n", i, u);
    if ((u = t30_get_rx_polled_sub_address(s)))
        printf("%d: Phase B: remote polled sub-address '%s'\n", i, u);
    if ((u = t30_get_rx_selective_polling_address(s)))
        printf("%d: Phase B: remote selective polling address '%s'\n", i, u);
    if ((u = t30_get_rx_sender_ident(s)))
        printf("%d: Phase B: remote sender ident '%s'\n", i, u);
    if ((u = t30_get_rx_password(s)))
        printf("%d: Phase B: remote password '%s'\n", i, u);
    printf("%d: Phase B handler on channel %d - (0x%X) %s\n", i, i, result, t30_frametype(result));
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static int phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    const char *u;

    i = (intptr_t) user_data;
    t30_get_transfer_statistics(s, &t);

    printf("%d: Phase D handler on channel %d - (0x%X) %s\n", i, i, result, t30_frametype(result));
    printf("%d: Phase D: bit rate %d\n", i, t.bit_rate);
    printf("%d: Phase D: ECM %s\n", i, (t.error_correcting_mode)  ?  "on"  :  "off");
    printf("%d: Phase D: pages transferred %d\n", i, t.pages_transferred);
    printf("%d: Phase D: pages in the file %d\n", i, t.pages_in_file);
    printf("%d: Phase D: image size %d x %d\n", i, t.width, t.length);
    printf("%d: Phase D: image resolution %d x %d\n", i, t.x_resolution, t.y_resolution);
    printf("%d: Phase D: bad rows %d\n", i, t.bad_rows);
    printf("%d: Phase D: longest bad row run %d\n", i, t.longest_bad_row_run);
    printf("%d: Phase D: compression type %d\n", i, t.encoding);
    printf("%d: Phase D: image size %d bytes\n", i, t.image_size);
    if ((u = t30_get_tx_ident(s)))
        printf("%d: Phase D: local ident '%s'\n", i, u);
    if ((u = t30_get_rx_ident(s)))
        printf("%d: Phase D: remote ident '%s'\n", i, u);
    printf("%d: Phase D: bits per row - min %d, max %d\n", i, s->t4.min_row_bits, s->t4.max_row_bits);

    if (use_receiver_not_ready)
        t30_set_receiver_not_ready(s, 3);

    if (test_local_interrupt)
    {
        if (i == 0)
        {
            printf("%d: Initiating interrupt request\n", i);
            t30_local_interrupt_request(s, TRUE);
        }
        else
        {
            switch (result)
            {
            case T30_PIP:
            case T30_PRI_MPS:
            case T30_PRI_EOM:
            case T30_PRI_EOP:
                printf("%d: Accepting interrupt request\n", i);
                t30_local_interrupt_request(s, TRUE);
                break;
            case T30_PIN:
                break;
            }
        }
    }
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int i;
    t30_stats_t t;
    const char *u;
    
    i = (intptr_t) user_data;
    printf("%d: Phase E handler on channel %d - (%d) %s\n", i, i, result, t30_completion_code_to_str(result));    
    t30_get_transfer_statistics(s, &t);
    printf("%d: Phase E: bit rate %d\n", i, t.bit_rate);
    printf("%d: Phase E: ECM %s\n", i, (t.error_correcting_mode)  ?  "on"  :  "off");
    printf("%d: Phase E: pages transferred %d\n", i, t.pages_transferred);
    printf("%d: Phase E: pages in the file %d\n", i, t.pages_in_file);
    printf("%d: Phase E: image size %d x %d\n", i, t.width, t.length);
    printf("%d: Phase E: image resolution %d x %d\n", i, t.x_resolution, t.y_resolution);
    printf("%d: Phase E: bad rows %d\n", i, t.bad_rows);
    printf("%d: Phase E: longest bad row run %d\n", i, t.longest_bad_row_run);
    printf("%d: Phase E: coding method %s\n", i, t4_encoding_to_str(t.encoding));
    printf("%d: Phase E: image size %d bytes\n", i, t.image_size);
    //printf("%d: Phase E: local ident '%s'\n", i, info->ident);
    if ((u = t30_get_rx_ident(s)))
        printf("%d: Phase E: remote ident '%s'\n", i, u);
    if ((u = t30_get_rx_country(s)))
        printf("%d: Phase E: Remote was made in '%s'\n", i, u);
    if ((u = t30_get_rx_vendor(s)))
        printf("%d: Phase E: Remote was made by '%s'\n", i, u);
    if ((u = t30_get_rx_model(s)))
        printf("%d: Phase E: Remote is model '%s'\n", i, u);
}
/*- End of function --------------------------------------------------------*/

static void t30_real_time_frame_handler(t30_state_t *s,
                                        void *user_data,
                                        int direction,
                                        const uint8_t *msg,
                                        int len)
{
    printf("T.30: Real time frame handler - %s, %s, length = %d\n",
           (direction)  ?  "line->T.30"  : "T.30->line",
           t30_frametype(msg[2]),
           len);
}
/*- End of function --------------------------------------------------------*/

static int document_handler(t30_state_t *s, void *user_data, int event)
{
    int i;
    
    i = (intptr_t) user_data;
    printf("%d: Document handler on channel %d - event %d\n", i, i, event);
    return FALSE;
}
/*- End of function --------------------------------------------------------*/

static void faxtester_real_time_frame_handler(faxtester_state_t *s,
                                              void *user_data,
                                              int direction,
                                              const uint8_t *msg,
                                              int len)
{
    printf("Tester: Real time frame handler - %s, %s, length = %d\n",
           (direction)  ?  "line->tester"  : "tester->line",
           t30_frametype(msg[2]),
           len);
    if (msg[1] == 0x13)
        next_step(s);
}
/*- End of function --------------------------------------------------------*/

static void faxtester_front_end_step_complete_handler(faxtester_state_t *s, void *user_data)
{
    next_step(s);
}
/*- End of function --------------------------------------------------------*/

static int string_to_msg(uint8_t msg[], uint8_t mask[], const char buf[])
{
    int i;
    int x;
    const char *t;

    msg[0] = 0;
    mask[0] = 0xFF;
    i = 0;
    t = (char *) buf;
    while (*t)
    {
        /* Skip white space */
        while (isspace(*t))
            t++;
        /* If we find ... we allow arbitrary addition info beyond this point in the message */
        if (t[0] == '.'  &&  t[1] == '.'  &&  t[2] == '.')
        {
            return -i;
        }
        else if (isxdigit(*t))
        {
            for (  ;  isxdigit(*t);  t++)
            {
                x = *t;
                if (x >= 'a')
                    x -= 0x20;
                if (x >= 'A')
                    x -= ('A' - 10);
                else
                    x -= '0';
                msg[i] = (msg[i] << 4) | x;
            }
            mask[i] = 0xFF;
            if (*t == '/')
            {
                /* There is a mask following the byte */
                mask[i] = 0;
                for (t++;  isxdigit(*t);  t++)
                {
                    x = *t;
                    if (x >= 'a')
                        x -= 0x20;
                    if (x >= 'A')
                        x -= ('A' - 10);
                    else
                        x -= '0';
                    mask[i] = (mask[i] << 4) | x;
                }
            }
            if (*t  &&  !isspace(*t))
            {
                /* Bad string */
                return 0;
            }
            i++;
        }
    }
    return i;
}
/*- End of function --------------------------------------------------------*/

#if 0
static void string_test2(const uint8_t msg[], int len)
{
    int i;
    
    if (len > 0)
    {
        for (i = 0;  i < len - 1;  i++)
            printf("%02X ", msg[i]);
        printf("%02X", msg[i]);
    }
}
/*- End of function --------------------------------------------------------*/

static void string_test3(const char buf[])
{
    uint8_t msg[1000];
    uint8_t mask[1000];
    int len;
    int i;
    
    len = string_to_msg(msg, mask, buf);
    printf("Len = %d: ", len);
    string_test2(msg, abs(len));
    printf("/");
    string_test2(mask, abs(len));
    printf("\n");
}
/*- End of function --------------------------------------------------------*/

static int string_test(void)
{
    string_test3("FF C8 12 34 56 78");
    string_test3("FF C8 12/55 34 56/aA 78 ");
    string_test3("FF C8 12/55 34 56/aA 78 ...");
    string_test3("FF C8 12/55 34 56/aA 78...");
    string_test3("FF C8 12/55 34 56/aA 78 ... 99 88 77");
    exit(0);
}
/*- End of function --------------------------------------------------------*/
#endif

static void corrupt_image(uint8_t image[], int len, const char *bad_rows)
{
    int i;
    int j;
    int k;
    uint32_t bits;
    uint32_t bitsx;
    int list[1000];
    int x;
    int row;
    const char *s;

    /* Form the list of rows to be hit */
    x = 0;
    s = bad_rows;
    while (*s)
    {
        while (isspace(*s))
            s++;
        if (sscanf(s, "%d", &list[x]) < 1)
            break;
        x++;
        while (isdigit(*s))
            s++;
        if (*s == ',')
            s++;
    }

    /* Go through the image, and corrupt the first bit of every listed row */
    bits = 0x7FF;
    bitsx = 0x7FF;
    row = 0;
    for (i = 0;  i < len;  i++)
    {
        bits ^= (image[i] << 11);
        bitsx ^= (image[i] << 11);
        for (j = 0;  j < 8;  j++)
        {
            if ((bits & 0xFFF) == 0x800)
            {
                /* We are at an EOL. Is this row in the list of rows to be corrupted? */
                row++;
                for (k = 0;  k < x;  k++)
                {
                    if (list[k] == row)
                    {
                        /* Corrupt this row. TSB85 says to hit the first bit after the EOL */
                        bitsx ^= 0x1000;
                    }
                }
            }
            bits >>= 1;
            bitsx >>= 1;
        }
        image[i] = (bitsx >> 3) & 0xFF;
    }
    printf("%d rows found. %d corrupted\n", row, x);
}
/*- End of function --------------------------------------------------------*/

static int next_step(faxtester_state_t *s)
{
    int delay;
    int flags;
    xmlChar *dir;
    xmlChar *type;
    xmlChar *modem;
    xmlChar *value;
    xmlChar *tag;
    xmlChar *bad_rows;
    uint8_t buf[1000];
    uint8_t mask[1000];
    int i;
    int hdlc;
    int short_train;
    int min_row_bits;
    int compression;
    int compression_step;
    int len;
    t4_state_t t4_state;

    if (s->cur == NULL)
    {
        if (!s->final_delayed)
        {
            /* Add a bit of waiting at the end, to ensure everything gets flushed through,
               any timers can expire, etc. */
            faxtester_set_rx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            faxtester_set_tx_type(s, T30_MODEM_PAUSE, 120000, FALSE);
            s->final_delayed = TRUE;
            return 1;
        }
        /* Finished */
        exit(0);
    }
    while (s->cur  &&  xmlStrcmp(s->cur->name, (const xmlChar *) "step") != 0)
        s->cur = s->cur->next;
    if (s->cur == NULL)
    {
        /* Finished */
        exit(0);
    }

    dir = xmlGetProp(s->cur, (const xmlChar *) "dir");
    type = xmlGetProp(s->cur, (const xmlChar *) "type");
    modem = xmlGetProp(s->cur, (const xmlChar *) "modem");
    value = xmlGetProp(s->cur, (const xmlChar *) "value");
    tag = xmlGetProp(s->cur, (const xmlChar *) "tag");
    bad_rows = xmlGetProp(s->cur, (const xmlChar *) "bad_rows");

    s->cur = s->cur->next;

    printf("Dir - %s, type - %s, modem - %s, value - %s, tag - %s\n",
           (dir)  ?  (const char *) dir  :  "",
           (type)  ?  (const char *) type  :  "",
           (modem)  ?  (const char *) modem  :  "",
           (value)  ?  (const char *) value  :  "",
           (tag)  ?  (const char *) tag  :  "");
    if (type == NULL)
        return 1;
    /*endif*/
    if (dir  &&  strcasecmp((const char *) dir, "R") == 0)
    {
        if (strcasecmp((const char *) type, "HDLC") == 0)
        {
            faxtester_set_rx_type(s, T30_MODEM_V21, FALSE, TRUE);
            faxtester_set_tx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            i = string_to_msg(buf, mask, (const char *) value);
            bit_reverse(buf, buf, abs(i));
        }
        else
        {
            return 0;
        }
    }
    else
    {
        if (modem)
        {
            hdlc = (strcasecmp((const char *) type, "PREAMBLE") == 0);
            short_train = (strcasecmp((const char *) type, "TCF") == 0);
            faxtester_set_rx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            if (strcasecmp((const char *) modem, "V.21") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V21, FALSE, TRUE);
            }
            else if (strcasecmp((const char *) modem, "V.17/14400") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17_14400, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.17/12000") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17_12000, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.17/9600") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17_9600, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.17/7200") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17_7200, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.29/9600") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V29_9600, FALSE, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.29/7200") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V29_7200, FALSE, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.27ter/4800") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V27TER_4800, FALSE, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.27ter/2400") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V27TER_2400, FALSE, hdlc);
            }
            else
            {
                printf("Unrecognised modem\n");
            }
        }

        if (strcasecmp((const char *) type, "CALL") == 0)
        {
            return 0;
        }
        else if (strcasecmp((const char *) type, "ANSWER") == 0)
        {
            return 0;
        }
        else if (strcasecmp((const char *) type, "CNG") == 0)
        {
            faxtester_set_rx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            faxtester_set_tx_type(s, T30_MODEM_CNG, FALSE, FALSE);
        }
        else if (strcasecmp((const char *) type, "CED") == 0)
        {
            faxtester_set_rx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            faxtester_set_tx_type(s, T30_MODEM_CED, FALSE, FALSE);
        }
        else if (strcasecmp((const char *) type, "WAIT") == 0)
        {
            delay = (value)  ?  atoi((const char *) value)  :  1;
            faxtester_set_rx_type(s, T30_MODEM_NONE, FALSE, FALSE);
            faxtester_set_tx_type(s, T30_MODEM_PAUSE, delay, FALSE);
        }
        else if (strcasecmp((const char *) type, "PREAMBLE") == 0)
        {
            flags = (value)  ?  atoi((const char *) value)  :  37;
            faxtester_send_hdlc_flags(s, flags);
        }
        else if (strcasecmp((const char *) type, "POSTAMBLE") == 0)
        {
            flags = (value)  ?  atoi((const char *) value)  :  5;
            faxtester_send_hdlc_flags(s, flags);
        }
        else if (strcasecmp((const char *) type, "HDLC") == 0)
        {
            i = string_to_msg(buf, mask, (const char *) value);
            bit_reverse(buf, buf, abs(i));
            faxtester_send_hdlc_msg(s, buf, abs(i));
        }
        else if (strcasecmp((const char *) type, "TCF") == 0)
        {
            if (value)
                i = atoi((const char *) value);
            else
                i = 450;
            memset(image, 0, i);
            faxtester_set_image_buffer(s, image, i);
        }
        else if (strcasecmp((const char *) type, "MSG") == 0)
        {
            min_row_bits = 0;
            compression_step = 0;
            if (t4_tx_init(&t4_state, (const char *) value, -1, -1) == NULL)
            {
                printf("Failed to init T.4 send\n");
                exit(2);
            }
            t4_tx_set_min_row_bits(&t4_state, min_row_bits);
            t4_tx_set_header_info(&t4_state, NULL);
            switch (compression_step)
            {
            case 0:
                compression = T4_COMPRESSION_ITU_T4_1D;
                break;
            case 1:
                compression = T4_COMPRESSION_ITU_T4_2D;
                break;
            case 2:
                compression = T4_COMPRESSION_ITU_T6;
                break;
            }
            t4_tx_set_tx_encoding(&t4_state, compression);
            if (t4_tx_start_page(&t4_state))
            {
                printf("Failed to start T.4 send\n");
                exit(2);
            }
            len = t4_tx_get_chunk(&t4_state, image, sizeof(image));
            if (bad_rows)
            {
                printf("We need to corrupt the image\n");
                corrupt_image(image, len, (const char *) bad_rows);
            }
            t4_tx_end(&t4_state);
            faxtester_set_image_buffer(s, image, len);
        }
        else
        {
            return 0;
        }
        /*endif*/
    }
    /*endif*/
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void exchange(faxtester_state_t *s)
{
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int len;
    int i;
    fax_state_t fax;
    int total_audio_time;
    const char *input_tiff_file_name;
    const char *output_tiff_file_name;
    int log_audio;
    t30_state_t *t30;

    log_audio = TRUE;
    input_tiff_file_name = INPUT_TIFF_FILE_NAME;
    output_tiff_file_name = OUTPUT_TIFF_FILE_NAME;

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

        if ((out_handle = afOpenFile(OUTPUT_FILE_NAME_WAVE, "w", filesetup)) == AF_NULL_FILEHANDLE)
        {
            fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME_WAVE);
            exit(2);
        }
    }

    total_audio_time = 0;

    faxtester_set_transmit_on_idle(&state, TRUE);
    faxtester_set_real_time_frame_handler(&state, faxtester_real_time_frame_handler, NULL);
    faxtester_set_front_end_step_complete_handler(&state, faxtester_front_end_step_complete_handler, NULL);

    fax_init(&fax, FALSE);
    t30 = fax_get_t30_state(&fax);
    fax_set_transmit_on_idle(&fax, TRUE);
    fax_set_tep_mode(&fax, TRUE);
    t30_set_tx_ident(t30, "1234567890");
    t30_set_tx_sub_address(t30, "Sub-address");
    t30_set_tx_sender_ident(t30, "Sender ID");
    t30_set_tx_password(t30, "Password");
    t30_set_tx_polled_sub_address(t30, "Polled sub-address");
    t30_set_tx_selective_polling_address(t30, "Sel polling address");
    t30_set_tx_nsf(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp\x00", 12);
    t30_set_ecm_capability(t30, TRUE);
    t30_set_supported_t30_features(t30,
                                   T30_SUPPORT_IDENTIFICATION
                                 | T30_SUPPORT_SELECTIVE_POLLING
                                 | T30_SUPPORT_SUB_ADDRESSING);
    t30_set_supported_image_sizes(t30,
                                  T30_SUPPORT_US_LETTER_LENGTH
                                | T30_SUPPORT_US_LEGAL_LENGTH
                                | T30_SUPPORT_UNLIMITED_LENGTH
                                | T30_SUPPORT_215MM_WIDTH
                                | T30_SUPPORT_255MM_WIDTH
                                | T30_SUPPORT_303MM_WIDTH);
    t30_set_supported_resolutions(t30,
                                  T30_SUPPORT_STANDARD_RESOLUTION
                                | T30_SUPPORT_FINE_RESOLUTION
                                | T30_SUPPORT_SUPERFINE_RESOLUTION
                                | T30_SUPPORT_R8_RESOLUTION
                                | T30_SUPPORT_R16_RESOLUTION
                                | T30_SUPPORT_300_300_RESOLUTION
                                | T30_SUPPORT_400_400_RESOLUTION
                                | T30_SUPPORT_600_600_RESOLUTION
                                | T30_SUPPORT_1200_1200_RESOLUTION
                                | T30_SUPPORT_300_600_RESOLUTION
                                | T30_SUPPORT_400_800_RESOLUTION
                                | T30_SUPPORT_600_1200_RESOLUTION);
    t30_set_supported_modems(t30, T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17);
    t30_set_supported_compressions(t30, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);
    t30_set_phase_b_handler(t30, phase_b_handler, (void *) (intptr_t) 1);
    t30_set_phase_d_handler(t30, phase_d_handler, (void *) (intptr_t) 1);
    t30_set_phase_e_handler(t30, phase_e_handler, (void *) (intptr_t) 1);
    t30_set_real_time_frame_handler(t30, t30_real_time_frame_handler, (void *) (intptr_t) 1);
    t30_set_document_handler(t30, document_handler, (void *) (intptr_t) 1);
    t30_set_rx_file(t30, output_tiff_file_name, -1);
    //t30_set_tx_file(t30, input_tiff_file_name, -1, -1);

    span_log_set_level(&t30->logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&t30->logging, "A");
    span_log_set_level(&fax.fe.modems.v29_rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.fe.modems.v29_rx.logging, "A");
    span_log_set_level(&fax.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.logging, "A");

    while (next_step(s) == 0)
        ;
    for (;;)
    {
        len = fax_tx(&fax, amp, SAMPLES_PER_CHUNK);
        faxtester_rx(s, amp, len);
        if (log_audio)
        {
            for (i = 0;  i < len;  i++)
                out_amp[2*i + 0] = amp[i];
        }

        total_audio_time += SAMPLES_PER_CHUNK;
        span_log_bump_samples(&fax.t30.logging, len);
        span_log_bump_samples(&fax.fe.modems.v29_rx.logging, len);
        span_log_bump_samples(&fax.logging, len);
                
        len = faxtester_tx(s, amp, 160);
        if (fax_rx(&fax, amp, len))
            break;
        if (log_audio)
        {
            for (i = 0;  i < len;  i++)
                out_amp[2*i + 1] = amp[i];
            if (afWriteFrames(out_handle, AF_DEFAULT_TRACK, out_amp, SAMPLES_PER_CHUNK) != SAMPLES_PER_CHUNK)
                break;
        }
    }
    if (log_audio)
    {
        if (afCloseFile(out_handle))
        {
            fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME_WAVE);
            exit(2);
        }
        afFreeFileSetup(filesetup);
    }
}
/*- End of function --------------------------------------------------------*/

static int parse_test_group(faxtester_state_t *s, xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur, const char *test)
{
    xmlChar *x;

    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "test") == 0)
        {
            if ((x = xmlGetProp(cur, (const xmlChar *) "name")))
            {
                if (xmlStrcmp(x, (const xmlChar *) test) == 0)
                {
                    printf("Found '%s'\n", (char *) x);
                    s->cur = cur->xmlChildrenNode;
                    return 0;
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int get_test_set(faxtester_state_t *s, const char *test_file, const char *test)
{
    xmlDocPtr doc;
    xmlNsPtr ns;
    xmlNodePtr cur;
#if 0
    xmlValidCtxt valid;
#endif

    ns = NULL;    
    xmlKeepBlanksDefault(0);
    xmlCleanupParser();
    doc = xmlParseFile(test_file);
    if (doc == NULL)
    {
        fprintf(stderr, "No document\n");
        exit(2);
    }
    /*endif*/
    xmlXIncludeProcess(doc);
#if 0
    if (!xmlValidateDocument(&valid, doc))
    {
        fprintf(stderr, "Invalid document\n");
        exit(2);
    }
    /*endif*/
#endif
    /* Check the document is of the right kind */
    if ((cur = xmlDocGetRootElement(doc)) == NULL)
    {
        fprintf(stderr, "Empty document\n");
        xmlFreeDoc(doc);
        exit(2);
    }
    /*endif*/
    if (xmlStrcmp(cur->name, (const xmlChar *) "fax-tests"))
    {
        fprintf(stderr, "Document of the wrong type, root node != fax-tests");
        xmlFreeDoc(doc);
        exit(2);
    }
    /*endif*/
    cur = cur->xmlChildrenNode;
    while (cur  &&  xmlIsBlankNode(cur))
        cur = cur->next;
    /*endwhile*/
    if (cur == NULL)
        exit(2);
    /*endif*/
    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "test-group") == 0)
        {
            if (parse_test_group(s, doc, ns, cur->xmlChildrenNode, test) == 0)
            {
                /* We found the test we want, so run it. */
                exchange(s);
                break;
            }
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    xmlFreeDoc(doc);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    const char *test_name;

    //string_test();

    test_name = "MRGN01";
    if (argc > 1)
        test_name = argv[1];

    faxtester_init(&state, TRUE);
    get_test_set(&state, "../spandsp/tsb85.xml", test_name);
    printf("Done\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
