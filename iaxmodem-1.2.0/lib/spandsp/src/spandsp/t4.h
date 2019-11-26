/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4.h - definitions for T.4 fax processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
 * $Id: t4.h,v 1.52 2008/07/22 13:48:15 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_T4_H_)
#define _SPANDSP_T4_H_

/*! \page t4_page T.4 image compression and decompression

\section t4_page_sec_1 What does it do?
The T.4 image compression and decompression routines implement the 1D and 2D
encoding methods defined in ITU specification T.4. They also implement the pure
2D encoding method defined in T.6. These are image compression algorithms used
for FAX transmission.

\section t4_page_sec_1 How does it work?
*/

typedef int (*t4_row_read_handler_t)(void *user_data, uint8_t buf[], size_t len);
typedef int (*t4_row_write_handler_t)(void *user_data, const uint8_t buf[], size_t len);

/*! Supported compression modes. */
typedef enum
{
    T4_COMPRESSION_ITU_T4_1D = 1,
    T4_COMPRESSION_ITU_T4_2D = 2,
    T4_COMPRESSION_ITU_T6 = 3
} t4_image_compression_t;

/*! Supported X resolutions, in pixels per metre. */
typedef enum
{
    T4_X_RESOLUTION_R4 = 4016,
    T4_X_RESOLUTION_R8 = 8031,
    T4_X_RESOLUTION_300 = 11811,
    T4_X_RESOLUTION_R16 = 16063,
    T4_X_RESOLUTION_600 = 23622,
    T4_X_RESOLUTION_800 = 31496,
    T4_X_RESOLUTION_1200 = 47244
} t4_image_x_resolution_t;

/*! Supported Y resolutions, in pixels per metre. */
typedef enum
{
    T4_Y_RESOLUTION_STANDARD = 3850,
    T4_Y_RESOLUTION_FINE = 7700,
    T4_Y_RESOLUTION_300 = 11811,
    T4_Y_RESOLUTION_SUPERFINE = 15400,  /* 400 is 15748 */
    T4_Y_RESOLUTION_600 = 23622,
    T4_Y_RESOLUTION_800 = 31496,
    T4_Y_RESOLUTION_1200 = 47244
} t4_image_y_resolution_t;

/*!
    Exact widths in PELs for the difference resolutions, and page widths.
    Note:
        The A4 widths also apply to North American letter and legal.
        The R4 resolution widths are not supported in recent versions of T.30
        Only images of exactly these widths are acceptable for FAX transmisson.

    R4    864 pels/215mm for ISO A4, North American Letter and Legal
    R4   1024 pels/255mm for ISO B4
    R4   1216 pels/303mm for ISO A3
    R8   1728 pels/215mm for ISO A4, North American Letter and Legal
    R8   2048 pels/255mm for ISO B4
    R8   2432 pels/303mm for ISO A3
    R16  3456 pels/215mm for ISO A4, North American Letter and Legal
    R16  4096 pels/255mm for ISO B4
    R16  4864 pels/303mm for ISO A3
*/
typedef enum
{
    T4_WIDTH_R4_A4 = 864,
    T4_WIDTH_R4_B4 = 1024,
    T4_WIDTH_R4_A3 = 1216,
    T4_WIDTH_R8_A4 = 1728,
    T4_WIDTH_R8_B4 = 2048,
    T4_WIDTH_R8_A3 = 2432,
    T4_WIDTH_300_A4 = 2592,
    T4_WIDTH_300_B4 = 3072,
    T4_WIDTH_300_A3 = 3648,
    T4_WIDTH_R16_A4 = 3456,
    T4_WIDTH_R16_B4 = 4096,
    T4_WIDTH_R16_A3 = 4864,
    T4_WIDTH_600_A4 = 5184,
    T4_WIDTH_600_B4 = 6144,
    T4_WIDTH_600_A3 = 7296,
    T4_WIDTH_1200_A4 = 10368,
    T4_WIDTH_1200_B4 = 12288,
    T4_WIDTH_1200_A3 = 14592
} t4_image_width_t;

/*!
    Length of the various supported paper sizes, in pixels at the various Y resolutions.
    Paper sizes are
        A4 (215mm x 297mm)
        B4 (255mm x 364mm)
        A3 (303mm x 418.56mm)
        North American Letter (215.9mm x 279.4mm)
        North American Legal (215.9mm x 355.6mm)
        Unlimited

    T.4 does not accurately define the maximum number of scan lines in a page. A wide
    variety of maximum row counts are used in the real world. It is important not to
    set our sending limit too high, or a receiving machine might split pages. It is
    important not to set it too low, or we might clip pages.

    Values seen for standard resolution A4 pages include 1037, 1045, 1109, 1126 and 1143.
    1109 seems the most-popular.  At fine res 2150, 2196, 2200, 2237, 2252-2262, 2264,
    2286, and 2394 are used. 2255 seems the most popular. We try to use balanced choices
    here.
*/
typedef enum
{
    /* A4 is 297mm long */
    T4_LENGTH_STANDARD_A4 = 1143,
    T4_LENGTH_FINE_A4 = 2286,
    T4_LENGTH_300_A4 = 4665,
    T4_LENGTH_SUPERFINE_A4 = 4573,
    T4_LENGTH_600_A4 = 6998,
    T4_LENGTH_800_A4 = 9330,
    T4_LENGTH_1200_A4 = 13996,
    /* B4 is 364mm long */
    T4_LENGTH_STANDARD_B4 = 1401,
    T4_LENGTH_FINE_B4 = 2802,
    T4_LENGTH_300_B4 = 0,
    T4_LENGTH_SUPERFINE_B4 = 5605,
    T4_LENGTH_600_B4 = 0,
    T4_LENGTH_800_B4 = 0,
    T4_LENGTH_1200_B4 = 0,
    /* North American letter is 279.4mm long */
    T4_LENGTH_STANDARD_US_LETTER = 1075,
    T4_LENGTH_FINE_US_LETTER = 2151,
    T4_LENGTH_300_US_LETTER = 0,
    T4_LENGTH_SUPERFINE_US_LETTER = 4302,
    T4_LENGTH_600_US_LETTER = 0,
    T4_LENGTH_800_US_LETTER = 0,
    T4_LENGTH_1200_US_LETTER = 0,
    /* North American legal is 355.6mm long */
    T4_LENGTH_STANDARD_US_LEGAL = 1369,
    T4_LENGTH_FINE_US_LEGAL = 2738,
    T4_LENGTH_300_US_LEGAL = 0,
    T4_LENGTH_SUPERFINE_US_LEGAL = 5476,
    T4_LENGTH_600_US_LEGAL = 0,
    T4_LENGTH_800_US_LEGAL = 0,
    T4_LENGTH_1200_US_LEGAL = 0
} t4_image_length_t;

/*!
    T.4 FAX compression/decompression descriptor. This defines the working state
    for a single instance of a T.4 FAX compression or decompression channel.
*/
typedef struct
{
    /*! \brief The same structure is used for T.4 transmit and receive. This variable
               records which mode is in progress. */
    int rx;
    /* "Background" information about the FAX, which can be stored in a TIFF file. */
    /*! \brief The vendor of the machine which produced the TIFF file. */ 
    const char *vendor;
    /*! \brief The model of machine which produced the TIFF file. */ 
    const char *model;
    /*! \brief The local ident string. */ 
    const char *local_ident;
    /*! \brief The remote end's ident string. */ 
    const char *far_ident;
    /*! \brief The FAX sub-address. */ 
    const char *sub_address;
    /*! \brief The FAX DCS information, as an ASCII string. */ 
    const char *dcs;
    /*! \brief The text which will be used in FAX page header. No text results
               in no header line. */
    const char *header_info;

    /*! \brief The type of compression used between the FAX machines. */
    int line_encoding;
    /*! \brief The minimum number of encoded bits per row. This is a timing thing
               for hardware FAX machines. */
    int min_bits_per_row;
    
    /*! \brief The compression type for output to the TIFF file. */
    int output_compression;
    /*! \brief The TIFF G3 FAX options. */
    int output_t4_options;

    /*! \brief Callback function to read a row of pixels from the image source. */
    t4_row_read_handler_t row_read_handler;
    /*! \brief Opaque pointer passed to row_read_handler. */
    void *row_read_user_data;
    /*! \brief Callback function to write a row of pixels to the image destination. */
    t4_row_write_handler_t row_write_handler;
    /*! \brief Opaque pointer passed to row_write_handler. */
    void *row_write_user_data;

    /*! \brief The time at which handling of the current page began. */
    time_t page_start_time;

    /*! \brief The current number of bytes per row of uncompressed image data. */
    int bytes_per_row;
    /*! \brief The size of the image in the image buffer, in bytes. */
    int image_size;
    /*! \brief The size of the compressed image on the line side, in bits. */
    int line_image_size;
    /*! \brief The current size of the image buffer. */
    int image_buffer_size;
    /*! \brief A point to the image buffer. */
    uint8_t *image_buffer;

    /*! \brief The libtiff context for the current TIFF file */
    TIFF *tiff_file;
    /*! \brief The current file name. */
    const char *file;
    /*! \brief The first page to transfer. -1 to start at the beginning of the file. */
    int start_page;
    /*! \brief The last page to transfer. -1 to continue to the end of the file. */
    int stop_page;

    /*! \brief The number of pages transferred to date. */
    int pages_transferred;
    /*! \brief The number of pages in the current TIFF file. */
    int pages_in_file;
    /*! \brief Column-to-column (X) resolution in pixels per metre. */
    int x_resolution;
    /*! \brief Row-to-row (Y) resolution in pixels per metre. */
    int y_resolution;
    /*! \brief Width of the current page, in pixels. */
    int image_width;
    /*! \brief Length of the current page, in pixels. */
    int image_length;
    /*! \brief Current pixel row number. */
    int row;
    /*! \brief The current number of consecutive bad rows. */
    int curr_bad_row_run;
    /*! \brief The longest run of consecutive bad rows seen in the current page. */
    int longest_bad_row_run;
    /*! \brief The total number of bad rows in the current page. */
    int bad_rows;

    /*! \brief Incoming bit buffer for decompression. */
    uint32_t rx_bitstream;
    /*! \brief The number of bits currently in rx_bitstream. */
    int rx_bits;
    /*! \brief The number of bits to be skipped before trying to match the next code word. */
    int rx_skip_bits;

    /*! \brief This variable is set if we are treating the current row as a 2D encoded
               one. */
    int row_is_2d;
    /*! \brief TRUE if the current run is black */
    int its_black;
    /*! \brief The current length of the current row. */
    int row_len;
    /*! \brief This variable is used to count the consecutive EOLS we have seen. If it
               reaches six, this is the end of the image. It is initially set to -1 for
               1D and 2D decoding, as an indicator that we must wait for the first EOL,
               before decodin any image data. */
    int consecutive_eols;

    /*! \brief Black and white run-lengths for the current row. */
    uint32_t *cur_runs;
    /*! \brief Black and white run-lengths for the reference row. */
    uint32_t *ref_runs;
    /*! \brief The number of runs currently in the reference row. */
    int ref_steps;
    /*! \brief The current step into the reference row run-lengths buffer. */
    int b_cursor;
    /*! \brief The current step into the current row run-lengths buffer. */
    int a_cursor;

    /*! \brief The reference or starting changing element on the coding line. At the
               start of the coding line, a0 is set on an imaginary white changing element
               situated just before the first element on the line. During the coding of
               the coding line, the position of a0 is defined by the previous coding mode.
               (See 4.2.1.3.2.). */
    int a0;
    /*! \brief The first changing element on the reference line to the right of a0 and of
               opposite colour to a0. */
    int b1;
    /*! \brief The length of the in-progress run of black or white. */
    int run_length;
    /*! \brief 2D horizontal mode control. */
    int black_white;

    /*! \brief Encoded data bits buffer. */
    uint32_t tx_bitstream;
    /*! \brief The number of bits currently in tx_bitstream. */
    int tx_bits;

    /*! \brief A pointer into the image buffer indicating where the last row begins */
    int last_row_starts_at;
    /*! \brief A pointer into the image buffer indicating where the current row begins */
    int row_starts_at;
    
    /*! \brief Pointer to the buffer for the current pixel row. */
    uint8_t *row_buf;
    
    /*! \brief Pointer to the byte containing the next image bit to transmit. */
    int bit_pos;
    /*! \brief Pointer to the bit within the byte containing the next image bit to transmit. */
    int bit_ptr;

    /*! \brief The current maximum contiguous rows that may be 2D encoded. */
    int max_rows_to_next_1d_row;
    /*! \brief Number of rows left that can be 2D encoded, before a 1D encoded row
               must be used. */
    int rows_to_next_1d_row;
    /*! \brief The current number of bits in the current encoded row. */
    int row_bits;
    /*! \brief The minimum bits in any row of the current page. For monitoring only. */
    int min_row_bits;
    /*! \brief The maximum bits in any row of the current page. For monitoring only. */
    int max_row_bits;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
} t4_state_t;

/*!
    T.4 FAX compression/decompression statistics.
*/
typedef struct
{
    /*! \brief The number of pages transferred so far. */
    int pages_transferred;
    /*! \brief The number of pages in the file (<0 if unknown). */
    int pages_in_file;
    /*! \brief The number of horizontal pixels in the most recent page. */
    int width;
    /*! \brief The number of vertical pixels in the most recent page. */
    int length;
    /*! \brief The number of bad pixel rows in the most recent page. */
    int bad_rows;
    /*! \brief The largest number of bad pixel rows in a block in the most recent page. */
    int longest_bad_row_run;
    /*! \brief The horizontal resolution of the page in pixels per metre */
    int x_resolution;
    /*! \brief The vertical resolution of the page in pixels per metre */
    int y_resolution;
    /*! \brief The type of compression used between the FAX machines */
    int encoding;
    /*! \brief The size of the image on the line, in bytes */
    int line_image_size;
} t4_stats_t;
    
#if defined(__cplusplus)
extern "C" {
#endif

/*! \brief Prepare for reception of a document.
    \param s The T.4 context.
    \param file The name of the file to be received.
    \param output_encoding The output encoding.
    \return A pointer to the context, or NULL if there was a problem. */
t4_state_t *t4_rx_init(t4_state_t *s, const char *file, int output_encoding);

/*! \brief Prepare to receive the next page of the current document.
    \param s The T.4 context.
    \return zero for success, -1 for failure. */
int t4_rx_start_page(t4_state_t *s);

/*! \brief Put a bit of the current document page.
    \param s The T.4 context.
    \param bit The data bit.
    \return TRUE when the bit ends the document page, otherwise FALSE. */
int t4_rx_put_bit(t4_state_t *s, int bit);

/*! \brief Put a byte of the current document page.
    \param s The T.4 context.
    \param byte The data byte.
    \return TRUE when the byte ends the document page, otherwise FALSE. */
int t4_rx_put_byte(t4_state_t *s, uint8_t byte);

/*! \brief Put a byte of the current document page.
    \param s The T.4 context.
    \param buf The buffer containing the chunk.
    \param len The length of the chunk.
    \return TRUE when the byte ends the document page, otherwise FALSE. */
int t4_rx_put_chunk(t4_state_t *s, const uint8_t buf[], int len);

/*! \brief Complete the reception of a page.
    \param s The T.4 receive context.
    \return 0 for success, otherwise -1. */
int t4_rx_end_page(t4_state_t *s);

/*! \brief End reception of a document. Tidy up, close the file and
           free the context. This should be used to end T.4 reception
           started with t4_rx_init.
    \param s The T.4 receive context.
    \return 0 for success, otherwise -1. */
int t4_rx_delete(t4_state_t *s);

/*! \brief End reception of a document. Tidy up and close the file.
           This should be used to end T.4 reception started with
           t4_rx_init.
    \param s The T.4 context.
    \return 0 for success, otherwise -1. */
int t4_rx_end(t4_state_t *s);

int t4_rx_set_row_write_handler(t4_state_t *s, t4_row_write_handler_t handler, void *user_data);

/*! \brief Set the encoding for the received data.
    \param s The T.4 context.
    \param encoding The encoding. */
void t4_rx_set_rx_encoding(t4_state_t *s, int encoding);

/*! \brief Set the expected width of the received image, in pixel columns.
    \param s The T.4 context.
    \param width The number of pixels across the image. */
void t4_rx_set_image_width(t4_state_t *s, int width);

/*! \brief Set the row-to-row (y) resolution to expect for a received image.
    \param s The T.4 context.
    \param resolution The resolution, in pixels per metre. */
void t4_rx_set_y_resolution(t4_state_t *s, int resolution);

/*! \brief Set the column-to-column (x) resolution to expect for a received image.
    \param s The T.4 context.
    \param resolution The resolution, in pixels per metre. */
void t4_rx_set_x_resolution(t4_state_t *s, int resolution);

/*! \brief Set the DCS information of the fax, for inclusion in the file.
    \param s The T.4 context.
    \param dcs The DCS information, formatted as an ASCII string. */
void t4_rx_set_dcs(t4_state_t *s, const char *dcs);

/*! \brief Set the sub-address of the fax, for inclusion in the file.
    \param s The T.4 context.
    \param sub_address The sub-address string. */
void t4_rx_set_sub_address(t4_state_t *s, const char *sub_address);

/*! \brief Set the identity of the remote machine, for inclusion in the file.
    \param s The T.4 context.
    \param ident The identity string. */
void t4_rx_set_far_ident(t4_state_t *s, const char *ident);

/*! \brief Set the vendor of the remote machine, for inclusion in the file.
    \param s The T.4 context.
    \param vendor The vendor string, or NULL. */
void t4_rx_set_vendor(t4_state_t *s, const char *vendor);

/*! \brief Set the model of the remote machine, for inclusion in the file.
    \param s The T.4 context.
    \param model The model string, or NULL. */
void t4_rx_set_model(t4_state_t *s, const char *model);

/*! \brief Prepare for transmission of a document.
    \param s The T.4 context.
    \param file The name of the file to be sent.
    \param start_page The first page to send. -1 for no restriction.
    \param stop_page The last page to send. -1 for no restriction.
    \return A pointer to the context, or NULL if there was a problem. */
t4_state_t *t4_tx_init(t4_state_t *s, const char *file, int start_page, int stop_page);

/*! \brief Prepare to send the next page of the current document.
    \param s The T.4 context.
    \return zero for success, -1 for failure. */
int t4_tx_start_page(t4_state_t *s);

/*! \brief Prepare the current page for a resend.
    \param s The T.4 context.
    \return zero for success, -1 for failure. */
int t4_tx_restart_page(t4_state_t *s);

/*! \brief Check for the existance of the next page. This information can
    be needed before it is determined that the current page is finished with.
    \param s The T.4 context.
    \return zero for next page found, -1 for failure. */
int t4_tx_more_pages(t4_state_t *s);

/*! \brief Complete the sending of a page.
    \param s The T.4 context.
    \return zero for success, -1 for failure. */
int t4_tx_end_page(t4_state_t *s);

/*! \brief Get the next bit of the current document page. The document will
           be padded for the current minimum scan line time. If the
           file does not contain an RTC (return to control) code at
           the end of the page, one will be added where appropriate.
    \param s The T.4 context.
    \return The next bit (i.e. 0 or 1). For the last bit of data, bit 1 is
            set (i.e. the returned value is 2 or 3). */
int t4_tx_get_bit(t4_state_t *s);

/*! \brief Get the next byte of the current document page. The document will
           be padded for the current minimum scan line time. If the
           file does not contain an RTC (return to control) code at
           the end of the page, one will be added where appropriate.
    \param s The T.4 context.
    \return The next byte. For the last byte of data, bit 8 is
            set. In this case, one or more bits of the byte may be padded with
            zeros, to complete the byte. */
int t4_tx_get_byte(t4_state_t *s);

/*! \brief Get the next chunk of the current document page. The document will
           be padded for the current minimum scan line time. If the
           file does not contain an RTC (return to control) code at
           the end of the page, one will be added where appropriate.
    \param s The T.4 context.
    \param buf The buffer into which the chunk is to written.
    \param max_len The maximum length of the chunk.
    \return The actual length of the chunk. If this is less than max_len it 
            indicates that the end of the document has been reached. */
int t4_tx_get_chunk(t4_state_t *s, uint8_t buf[], int max_len);

/*! \brief Return the next bit of the current document page, without actually
           moving forward in the buffer. The document will be padded for the
           current minimum scan line time. If the file does not contain an
           RTC (return to control) code at the end of the page, one will be
           added.
    \param s The T.4 context.
    \return The next bit (i.e. 0 or 1). For the last bit of data, bit 1 is
            set (i.e. the returned value is 2 or 3). */
int t4_tx_check_bit(t4_state_t *s);

/*! \brief End the transmission of a document. Tidy up, close the file and
           free the context. This should be used to end T.4 transmission
           started with t4_tx_init.
    \param s The T.4 context.
    \return 0 for success, otherwise -1. */
int t4_tx_delete(t4_state_t *s);

/*! \brief End the transmission of a document. Tidy up and close the file.
           This should be used to end T.4 transmission started with t4_tx_init.
    \param s The T.4 context.
    \return 0 for success, otherwise -1. */
int t4_tx_end(t4_state_t *s);

/*! \brief Set the encoding for the encoded data.
    \param s The T.4 context.
    \param encoding The encoding. */
void t4_tx_set_tx_encoding(t4_state_t *s, int encoding);

/*! \brief Set the minimum number of encoded bits per row. This allows the
           makes the encoding process to be set to comply with the minimum row
           time specified by a remote receiving machine.
    \param s The T.4 context.
    \param bits The minimum number of bits per row. */
void t4_tx_set_min_row_bits(t4_state_t *s, int bits);

/*! \brief Set the identity of the local machine, for inclusion in page headers.
    \param s The T.4 context.
    \param ident The identity string. */
void t4_tx_set_local_ident(t4_state_t *s, const char *ident);

/*! Set the info field, included in the header line included in each page of an encoded
    FAX. This is a string of up to 50 characters. Other information (date, local ident, etc.)
    are automatically included in the header. If the header info is set to NULL or a zero
    length string, no header lines will be added to the encoded FAX.
    \brief Set the header info.
    \param s The T.4 context.
    \param info A string, of up to 50 bytes, which will form the info field. */
void t4_tx_set_header_info(t4_state_t *s, const char *info);

int t4_tx_set_row_read_handler(t4_state_t *s, t4_row_read_handler_t handler, void *user_data);

/*! \brief Get the row-to-row (y) resolution of the current page.
    \param s The T.4 context.
    \return The resolution, in pixels per metre. */
int t4_tx_get_y_resolution(t4_state_t *s);

/*! \brief Get the column-to-column (x) resolution of the current page.
    \param s The T.4 context.
    \return The resolution, in pixels per metre. */
int t4_tx_get_x_resolution(t4_state_t *s);

/*! \brief Get the width of the current page, in pixel columns.
    \param s The T.4 context.
    \return The number of columns. */
int t4_tx_get_image_width(t4_state_t *s);

/*! \brief Get the number of pages in the file.
    \param s The T.4 context.
    \return The number of pages, or -1 if there is an error. */
int t4_tx_get_pages_in_file(t4_state_t *s);

/*! Get the current image transfer statistics. 
    \brief Get the current transfer statistics.
    \param s The T.4 context.
    \param t A pointer to a statistics structure. */
void t4_get_transfer_statistics(t4_state_t *s, t4_stats_t *t);

/*! Get the short text name of an encoding format. 
    \brief Get the short text name of an encoding format.
    \param encoding The encoding type.
    \return A pointer to the string. */
const char *t4_encoding_to_str(int encoding);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
