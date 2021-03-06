//===========================================================================
//
//      vsscanf.cxx
//
//      ANSI Stdio vsscanf() function
//
//===========================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 or (at your option) any later version.
//
// eCos is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with eCos; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
//
// As a special exception, if other files instantiate templates or use macros
// or inline functions from this file, or you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on
// this file might be covered by the GNU General Public License.
//
// Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
// at http://sources.redhat.com/ecos/ecos-license/
// -------------------------------------------
//####ECOSGPLCOPYRIGHTEND####
// ----------------------------------------------------------------------
// Portions of this software Copyright (c) 2012 Broadcom Corporation
// ----------------------------------------------------------------------
//===========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    Mike Sieweke (Broadcom), copied directly from vfscanf.cxx
// Contributors: 
// Date:         3-October-2012
// Purpose:      data input from string
// Description:  The original vsscanf() used a Cyg_StdioStream and called
//               vfscanf().  It could lock a mutex in several places, so the
//               function wasn't safe to be called from ISR, DSR, or timer
//               callback.  Also, it flushed stdout at the end of every string,
//               so it could cause unexpected context switches when the serial
//               output buffer was full.
//               This is a straight copy of vfscanf() with all the file-ops
//               converted to string ops.
// Usage:       
//
//####DESCRIPTIONEND####
//
//===========================================================================
//
// This code is based on original code with the following copyright:
//
/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


// CONFIGURATION

#include <pkgconf/libc_stdio.h>   // Configuration header
#include <pkgconf/libc_i18n.h>    // Configuration header for mb support


// INCLUDES

#include <cyg/infra/cyg_type.h>   // Common type definitions and support
#include <stdarg.h>               // Variable argument definitions
#include <stdio.h>                // Standard header for all stdio files
#include <ctype.h>                // isspace() and isupper()
#include <stdlib.h>               // standard utility functions e.g. strtol
#include <cyg/libc/stdio/stream.hxx>  // C library streams

#ifdef CYGSEM_LIBC_STDIO_SCANF_FLOATING_POINT

#include <float.h>

# define MAXFRACT   DBL_DIG
# define MAXEXP     DBL_MAX_10_EXP

# define BUF      (MAXEXP+MAXFRACT+3)  /* 3 = sign + decimal point + NUL */

#else // ifdef CYGSEM_LIBC_STDIO_PRINTF_FLOATING_POINT

# define BUF            40

#endif // ifdef CYGSEM_LIBC_STDIO_PRINTF_FLOATING_POINT

/*
 * Flags used during conversion.
 */

#define LONG            0x01    /* l: long or double */
#define LONGDBL         0x02    /* L: long double; unimplemented */
#define SHORT           0x04    /* h: short */
#define SUPPRESS        0x08    /* suppress assignment */
#define POINTER         0x10    /* weird %p pointer (`fake hex') */
#define NOSKIP          0x20    /* do not skip blanks */

/*
 * The following are used in numeric conversions only:
 * SIGNOK, NDIGITS, DPTOK, and EXPOK are for floating point;
 * SIGNOK, NDIGITS, PFXOK, and NZDIGITS are for integral.
 */

#define SIGNOK          0x40    /* +/- is (still) legal */
#define NDIGITS         0x80    /* no digits detected */

#define DPTOK           0x100   /* (float) decimal point is still legal */
#define EXPOK           0x200   /* (float) exponent (e+3, etc) still
                                   legal */

#define PFXOK           0x100   /* 0x prefix is (still) legal */
#define NZDIGITS        0x200   /* no zero digits detected */

/*
 * Conversion types.
 */

#define CT_CHAR         0       /* %c conversion */
#define CT_CCL          1       /* %[...] conversion */
#define CT_STRING       2       /* %s conversion */
#define CT_INT          3       /* integer, i.e., strtol or strtoul */
#define CT_FLOAT        4       /* floating, i.e., strtod */

#if 0
#define u_char unsigned char
#endif
#define u_char char
#define u_long unsigned long

typedef unsigned long (*strtoul_t)(const char *, char **endptr, int base);

static u_char *
__sccl (char *tab, u_char *fmt);


/*
 * vsscanf
 */

#define _CAST_VOID

#define CURR_POS (str)

#define INC_CURR_POS (str++)
#define UNGETC       (str--)

#define MOVE_CURR_POS(x) (str += x)

#define SPACE_LEFT (*str != 0)

#define BufferEmpty ( !SPACE_LEFT )

#ifdef CYGINT_LIBC_I18N_MB_REQUIRED
typedef int (*mbtowc_fn_type)(wchar_t *, const char *, size_t, int *);
externC mbtowc_fn_type __get_current_locale_mbtowc_fn();
#endif

int
vsscanf (const char *str, const char *fmt0, va_list ap)
{
    u_char *fmt = (u_char *) fmt0;
    int     c;          /* character from format, or conversion */
    wchar_t wc;         /* wide character from format */
    size_t  width;      /* field width, or 0 */
    char   *p;          /* points into all kinds of strings */
    int     n;          /* handy integer */
    int     flags;      /* flags as defined above */
    char   *p0;         /* saves original value of p when necessary */
    char   *lptr;       /* literal pointer */
    int     nassigned;  /* number of fields assigned */
    int     nread;      /* number of characters consumed from fp */
    int     base = 0;   /* base argument to strtol/strtoul */
    int     nbytes = 1; /* number of bytes processed */

#ifdef CYGINT_LIBC_I18N_MB_REQUIRED
    mbtowc_fn_type mbtowc_fn;
    int     state = 0;      /* used for mbtowc_fn */
#endif
    
    strtoul_t ccfn = NULL;      /* conversion function (strtol/strtoul) */
    char      ccltab[256];      /* character class table for %[...] */
    char      buf[BUF];         /* buffer for numeric conversions */

    short       *sp;
    int         *ip;
    float       *flp;
    long double *ldp;
    double      *dp;
    long        *lp;

    /* `basefix' is used to avoid `if' tests in the integer scanner */
    static const short basefix[17] =
    {10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

#ifdef CYGINT_LIBC_I18N_MB_REQUIRED
    mbtowc_fn = __get_current_locale_mbtowc_fn();
#endif

    nassigned = 0;
    nread = 0;
    for (;;)
    {
#ifndef CYGINT_LIBC_I18N_MB_REQUIRED
        wc = *fmt;
#else
        nbytes = mbtowc_fn (&wc, fmt, MB_CUR_MAX, &state);
#endif
        fmt += nbytes;

        if (wc == 0)
           return nassigned;
        if (nbytes == 1 && isspace (wc))
        {
            for (;;)
            {
                if (BufferEmpty)
                    return nassigned;
                if (!isspace (*CURR_POS))
                    break;
                nread++, INC_CURR_POS;
            }
            continue;
        }
        if (wc != '%')
            goto literal;
        width = 0;
        flags = 0;

        /*
         * switch on the format.  continue if done; break once format
         * type is derived.
         */
        
again:
        c = *fmt++;
        
        switch (c)
        {
        case '%':
literal:
            lptr = fmt - nbytes;
            for (n = 0; n < nbytes; ++n)
              {
                if (BufferEmpty)
                  goto input_failure;
                if (*CURR_POS != *lptr)
                  goto match_failure;
                INC_CURR_POS;
                nread++;
                ++lptr;
              }
        continue;

        case '*':
            flags |= SUPPRESS;
            goto again;
        case 'l':
            flags |= LONG;
            goto again;
        case 'L':
            flags |= LONGDBL;
            goto again;
        case 'h':
            flags |= SHORT;
            goto again;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            width = width * 10 + c - '0';
            goto again;

            /*
             * Conversions. Those marked `compat' are for
             * 4.[123]BSD compatibility.
             *
             * (According to ANSI, E and X formats are supposed to
             * the same as e and x.  Sorry about that.)
             */

        case 'D':               /* compat */
            flags |= LONG;
            /* FALLTHROUGH */
        case 'd':
            c = CT_INT;
            ccfn = (strtoul_t)strtol;
            base = 10;
            break;

        case 'i':
            c = CT_INT;
            ccfn = (strtoul_t)strtol;
            base = 0;
            break;

        case 'O':               /* compat */
            flags |= LONG;
            /* FALLTHROUGH */
        case 'o':
            c = CT_INT;
            ccfn = strtoul;
            base = 8;
            break;

        case 'u':
            c = CT_INT;
            ccfn = strtoul;
            base = 10;
            break;

        case 'X':               /* compat   XXX */
        case 'x':
            flags |= PFXOK;     /* enable 0x prefixing */
            c = CT_INT;
            ccfn = strtoul;
            base = 16;
            break;

        case 'E':               /* compat   XXX */
        case 'G':               /* compat   XXX */
/* ANSI says that E,G and X behave the same way as e,g,x */
            /* FALLTHROUGH */
        case 'e':
        case 'f':
        case 'g':
            c = CT_FLOAT;
            break;

        case 's':
            c = CT_STRING;
            break;

        case '[':
            fmt = __sccl (ccltab, fmt);
            flags |= NOSKIP;
            c = CT_CCL;
            break;

        case 'c':
            flags |= NOSKIP;
            c = CT_CHAR;
            break;

        case 'p':               /* pointer format is like hex */
            flags |= POINTER | PFXOK;
            c = CT_INT;
            ccfn = strtoul;
            base = 16;
            break;

        case 'n':
            if (flags & SUPPRESS)       /* ??? */
                continue;
            if (flags & SHORT)
            {
                sp = va_arg (ap, short *);
                *sp = nread;
            }
            else if (flags & LONG)
            {
                lp = va_arg (ap, long *);
                *lp = nread;
            }
            else
            {
                ip = va_arg (ap, int *);
                *ip = nread;
            }
            continue;

            /*
             * Disgusting backwards compatibility hacks.        XXX
             */
        case '\0':              /* compat */
            return EOF;

        default:                /* compat */
            if (isupper (c))
                flags |= LONG;
            c = CT_INT;
            ccfn = (strtoul_t)strtol;
            base = 10;
            break;
        }

        /*
         * We have a conversion that requires input.
         */
        if (BufferEmpty)
            goto input_failure;

        /*
         * Consume leading white space, except for formats that
         * suppress this.
         */
        if ((flags & NOSKIP) == 0)
        {
            while (isspace (*CURR_POS))
            {
                nread++;
                INC_CURR_POS;
                if (SPACE_LEFT == 0)
                    goto input_failure;
            }
            /*
             * Note that there is at least one character in the
             * buffer, so conversions that do not set NOSKIP ca
             * no longer result in an input failure.
             */
        }

        /*
         * Do the conversion.
         */
        switch (c)
        {

        case CT_CHAR:
            /* scan arbitrary characters (sets NOSKIP) */
            if (width == 0)
                width = 1;
            if (flags & SUPPRESS)
            {
                size_t sum = 0;

                for (;;)
                {
                    if ((n = SPACE_LEFT) < (signed)width)
                    {
                        sum += n;
                        width -= n;
                        MOVE_CURR_POS(n-1);
                        INC_CURR_POS;
                        if (sum == 0)
                            goto input_failure;
                        break;
                    }
                    else
                    {
                        sum += width;
                        MOVE_CURR_POS(width - 1);
                        INC_CURR_POS;
                        break;
                    }
                }
                nread += sum;
            }
            else
            {
                /* Kludge city for the moment */
                char *dest = va_arg (ap, char *);
                int n = width;
                if (SPACE_LEFT == 0)
                    goto input_failure;

                while (n && !BufferEmpty)
                {
                    *dest++ = *CURR_POS;
                    INC_CURR_POS;
                    n--;
                    nread++;
                }
                nassigned++;
            }
            break;

        case CT_CCL:
            /* scan a (nonempty) character class (sets NOSKIP) */
            if (width == 0)
                width = ~0;             /* `infinity' */
            /* take only those things in the class */
            if (flags & SUPPRESS)
            {
                n = 0;
                while (ccltab[*CURR_POS])
                {
                    n++, INC_CURR_POS;
                    if (--width == 0)
                        break;
                    if (BufferEmpty)
                    {
                        if (n == 0)
                            goto input_failure;
                        break;
                    }
                }
                if (n == 0)
                    goto match_failure;
            }
            else
            {
                p0 = p = va_arg (ap, char *);
                while (ccltab[*CURR_POS])
                {
                    *p++ = *CURR_POS;
                    INC_CURR_POS;
                    if (--width == 0)
                        break;
                    if (BufferEmpty)
                    {
                        if (p == p0)
                            goto input_failure;
                        break;
                    }
                }
                n = p - p0;
                if (n == 0)
                    goto match_failure;
                *p = 0;
                nassigned++;
            }
            nread += n;
            break;

        case CT_STRING:
            /* like CCL, but zero-length string OK, & no NOSKIP */
            if (width == 0)
                width = ~0;
            if (flags & SUPPRESS)
            {
                n = 0;
                while (!isspace (*CURR_POS))
                {
                    n++, INC_CURR_POS;
                    if (--width == 0)
                        break;
                    if (BufferEmpty)
                        break;
                }
                nread += n;
            }
            else
            {
                p0 = p = va_arg (ap, char *);
                while (!isspace (*CURR_POS))
                {
                    *p++ = *CURR_POS;
                    INC_CURR_POS;
                    if (--width == 0)
                        break;
                    if (BufferEmpty)
                        break;
                }
                *p = 0;
                nread += p - p0;
                nassigned++;
            }
            continue;

        case CT_INT:
            /* scan an integer as if by strtol/strtoul */
#ifdef hardway
            if (width == 0 || width > sizeof (buf) - 1)
                width = sizeof (buf) - 1;
#else
            /* size_t is unsigned, hence this optimisation */
            if (--width > sizeof (buf) - 2)
                width = sizeof (buf) - 2;
            width++;
#endif
            flags |= SIGNOK | NDIGITS | NZDIGITS;
            for (p = buf; width; width--)
            {
                c = *CURR_POS;
                /*
                 * Switch on the character; `goto ok' if we
                 * accept it as a part of number.
                 */
                switch (c)
                {
                    /*
                     * The digit 0 is always legal, but is special.
                     * For %i conversions, if no digits (zero or nonzero)
                     * have been scanned (only signs), we will have base==0
                     * In that case, we should set it to 8 and enable 0x
                     * prefixing. Also, if we have not scanned zero digits
                     * before this, do not turn off prefixing (someone else
                     * will turn it off if we have scanned any nonzero
                     * digits).
                     */
                case '0':
                    if (base == 0)
                    {
                        base = 8;
                        flags |= PFXOK;
                    }
                    if (flags & NZDIGITS)
                        flags &= ~(SIGNOK | NZDIGITS | NDIGITS);
                    else
                        flags &= ~(SIGNOK | PFXOK | NDIGITS);
                    goto ok;

                    /* 1 through 7 always legal */
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                    base = basefix[base];
                    flags &= ~(SIGNOK | PFXOK | NDIGITS);
                    goto ok;

                    /* digits 8 and 9 ok iff decimal or hex */
                case '8':
                case '9':
                    base = basefix[base];
                    if (base <= 8)
                        break;  /* not legal here */
                    flags &= ~(SIGNOK | PFXOK | NDIGITS);
                    goto ok;

                    /* letters ok iff hex */
                case 'A':
                case 'B':
                case 'C':
                case 'D':
                case 'E':
                case 'F':
                case 'a':
                case 'b':
                case 'c':
                case 'd':
                case 'e':
                case 'f':
                    /* no need to fix base here */
                    if (base <= 10)
                        break;  /* not legal here */
                    flags &= ~(SIGNOK | PFXOK | NDIGITS);
                    goto ok;

                    /* sign ok only as first character */
                case '+':
                case '-':
                    if (flags & SIGNOK)
                    {
                        flags &= ~SIGNOK;
                        goto ok;
                    }
                    break;

                    /* x ok iff flag still set & 2nd char */
                case 'x':
                case 'X':
                    if (flags & PFXOK && p == buf + 1)
                    {
                        base = 16;/* if %i */
                        flags &= ~PFXOK;
                        goto ok;
                    }
                    break;
                }

                /*
                 * If we got here, c is not a legal character
                 * for a number.  Stop accumulating digits.
                 */
                break;
            ok:
                /*
                 * c is legal: store it and look at the next.
                 */
                *p++ = c;
                INC_CURR_POS;
                if (SPACE_LEFT == 0)
                    break;          /* EOF */
            }
            /*
             * If we had only a sign, it is no good; push back the sign.
             * If the number ends in `x', it was [sign] '0' 'x', so push
             * back the x and treat it as [sign] '0'.
             */
            if (flags & NDIGITS)
            {
                if (p > buf)
                    UNGETC;
                goto match_failure;
            }
            c = ((u_char *) p)[-1];
            if (c == 'x' || c == 'X')
            {
                --p;
                UNGETC;
            }
            if ((flags & SUPPRESS) == 0)
            {
                u_long res;

                *p = 0;
                res = (*ccfn) (buf, (char **) NULL, base);
                if (flags & POINTER)
                    *(va_arg (ap, char **)) = (char *) (CYG_ADDRESS) res;
                else if (flags & SHORT)
                {
                    sp = va_arg (ap, short *);
                    *sp = res;
                }
                else if (flags & LONG)
                {
                    lp = va_arg (ap, long *);
                    *lp = res;
                }
                else
                {
                    ip = va_arg (ap, int *);
                    *ip = res;
                }
                nassigned++;
            }
            nread += p - buf;
            break;

        case CT_FLOAT:
            /* scan a floating point number as if by strtod */

#ifdef hardway
            if (width == 0 || width > sizeof (buf) - 1)
                width = sizeof (buf) - 1;
#else
            /* size_t is unsigned, hence this optimisation */
            if (--width > sizeof (buf) - 2)
                width = sizeof (buf) - 2;
            width++;
#endif // ifdef hardway

            flags |= SIGNOK | NDIGITS | DPTOK | EXPOK;
            for (p = buf; width; width--)
            {
                c = *CURR_POS;
                /*
                 * This code mimicks the integer conversion
                 * code, but is much simpler.
                 */
                switch (c)
                {

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    flags &= ~(SIGNOK | NDIGITS);
                    goto fok;

                case '+':
                case '-':
                    if (flags & SIGNOK)
                    {
                        flags &= ~SIGNOK;
                        goto fok;
                    }
                    break;
                case '.':
                    if (flags & DPTOK)
                    {
                        flags &= ~(SIGNOK | DPTOK);
                        goto fok;
                    }
                    break;
                case 'e':
                case 'E':
                    /* no exponent without some digits */
                    if ((flags & (NDIGITS | EXPOK)) == EXPOK)
                    {
                        flags =
                            (flags & ~(EXPOK | DPTOK)) |
                            SIGNOK | NDIGITS;
                        goto fok;
                    }
                    break;
                }
                break;
            fok:
                *p++ = c;
                INC_CURR_POS;
                if (SPACE_LEFT == 0)
                    break;          /* EOF */
            }
            /*
             * If no digits, might be missing exponent digits
             * (just give back the exponent) or might be missing
             * regular digits, but had sign and/or decimal point.
             */
            if (flags & NDIGITS)
            {
                if (flags & EXPOK)
                {
                    /* no digits at all */
                    while (p > buf)
                        UNGETC;
                    goto match_failure;
                }
                /* just a bad exponent (e and maybe sign) */
                c = *(u_char *)-- p;
                if (c != 'e' && c != 'E')
                {
                    UNGETC; /* sign */
                    c = *(u_char *)-- p;
                }
                UNGETC;
            }
            if ((flags & SUPPRESS) == 0)
            {
                double res;

                *p = 0;
#ifdef CYGSEM_LIBC_STDIO_SCANF_FLOATING_POINT
                res = atof (buf);
#else
                res = 0.0;
#endif
                if (flags & LONG)
                {
                    dp = va_arg (ap, double *);
                    *dp = res;
                }
                else if (flags & LONGDBL)
                {
                    ldp = va_arg (ap, long double *);
                    *ldp = res;
                }
                else
                {
                    flp = va_arg (ap, float *);
                    *flp = res;
                }
                nassigned++;
            }
            nread += p - buf;
            break;
        }
    }

input_failure:
    return nassigned ? nassigned : -1;
match_failure:
    return nassigned;
} // vsscanf()

/*
 * Fill in the given table from the scanset at the given format
 * (just after `[').  Return a pointer to the character past the
 * closing `]'.  The table has a 1 wherever characters should be
 * considered part of the scanset.
 */

/*static*/
u_char *
__sccl (char *tab, u_char *fmt)
{
    int c, n, v;

    /* first `clear' the whole table */
    c = *fmt++;                 /* first char hat => negated scanset */
    if (c == '^')
    {
        v = 1;                  /* default => accept */
        c = *fmt++;             /* get new first char */
    }
    else
        v = 0;                  /* default => reject */
    /* should probably use memset here */
    for (n = 0; n < 256; n++)
        tab[n] = v;
    if (c == 0)
        return fmt - 1;         /* format ended before closing ] */

    /*
     * Now set the entries corresponding to the actual scanset to the
     * opposite of the above.
     *
     * The first character may be ']' (or '-') without being special; the
     * last character may be '-'.
     */

    v = 1 - v;
    for (;;)
    {
        tab[c] = v;             /* take character c */
    doswitch:
        n = *fmt++;             /* and examine the next */
        switch (n)
        {

        case 0:         /* format ended too soon */
            return fmt - 1;

        case '-':
            /*
             * A scanset of the form [01+-] is defined as `the digit 0, the
             * digit 1, the character +, the character -', but the effect
             * of a scanset such as [a-zA-Z0-9] is implementation defined.
             * The V7 Unix scanf treats `a-z' as `the letters a through z',
             * but treats `a-a' as `the letter a, the character -, and the
             * letter a'.
             *
             * For compatibility, the `-' is not considerd to define a
             * range if the character following it is either a close
             * bracket (required by ANSI) or is not numerically greater
             * than the character we just stored in the table (c).
             */
            n = *fmt;
            if (n == ']' || n < c)
            {
                c = '-';
                break;          /* resume the for(;;) */
            }
            fmt++;
            do
            {                   /* fill in the range */
                tab[++c] = v;
            }
            while (c < n);
#if 1                   /* XXX another disgusting compatibility hack */
            /*
             * Alas, the V7 Unix scanf also treats formats such
             * as [a-c-e] as `the letters a through e'. This too
             * is permitted by the standard....
             */
            goto doswitch;
#else
            c = *fmt++;
            if (c == 0)
                return fmt - 1;
            if (c == ']')
                return fmt;
#endif

            break;


        case ']':               /* end of scanset */
            return fmt;

        default:                /* just another character */
            c = n;
            break;
        }
    }
    /* NOTREACHED */
} // __sccl()


// EOF vsscanf.cxx
