/*
 * Copyright 1995-2003 by Frederic Lepied, France. <Lepied@XFree86.org>
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifndef __WCMSERIAL_H
#define __WCMSERIAL_H

#define WC_RESET "\r#"           /* reset wacom IV or wacom V command set */
#define WC_RESET_BAUD "\r$"      /* reset baud rate to default (wacom V) ors
                                  * switch to wacom IIs (wacom IV) */
#define WC_CONFIG "~R\r"         /* request a configuration string */
#define WC_COORD "~C\r"          /* request max coordinates */
#define WC_MODEL "~#\r"          /* request model and ROM version */

#define WC_MULTI "MU1\r"         /* multi mode input */
#define WC_UPPER_ORIGIN "OC1\r"  /* origin in upper left */
#define WC_SUPPRESS "SU"         /* suppress mode */
#define WC_ALL_MACRO "~M0\r"     /* enable all macro buttons */
#define WC_NO_MACRO1 "~M1\r"     /* disable macro buttons of group 1 */
#define WC_RATE  "IT0\r"         /* max transmit rate (unit of 5 ms) */
#define WC_TILT_MODE "FM1\r"     /* enable extra protocol for tilt management */
#define WC_NO_INCREMENT "IN0\r"  /* do not enable increment mode */
#define WC_STREAM_MODE "SR\r"    /* enable continuous mode */
#define WC_PRESSURE_MODE "PH1\r" /* enable pressure mode */
#define WC_ZFILTER "ZF1\r"       /* stop sending coordinates */
#define WC_STOP  "\nSP\r"        /* stop sending coordinates */
#define WC_START "ST\r"          /* start sending coordinates */
#define WC_NEW_RESOLUTION "NR"   /* change the resolution */

#define WC_V_SINGLE "MT0\r"
#define WC_V_MULTI "MT1\r"
#define WC_V_ID  "ID1\r"
#define WC_V_19200 "BA19\r"
#define WC_V_38400 "BA38\r"
/*  #define WC_V_9600 "BA96\r" */
#define WC_V_9600 "$\r"

#define WC_RESET_19200 "\r$"     /* reset to 9600 baud */
#define WC_RESET_19200_IV "\r#"

char* xf86WcmSendRequest(int fd, const char* request, char* answer, int maxlen);

#endif /* __WCMSERIAL_H */

