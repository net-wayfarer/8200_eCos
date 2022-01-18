/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2014 Peter Sulc <petersu@broadcom.com>
 */


void prom_putchar(char c)
{
    unsigned tx_level;

    do	{
        tx_level = *(volatile unsigned *)0xb4e00528;
        tx_level = tx_level >> 24;
        tx_level &= 0x1f;
	}	while (tx_level >= 14);
    *(unsigned *)0xb4e00534 = (unsigned)c;
}
