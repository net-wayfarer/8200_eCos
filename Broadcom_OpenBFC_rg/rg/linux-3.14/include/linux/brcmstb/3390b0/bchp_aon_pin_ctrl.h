/****************************************************************************
 *     Copyright (c) 1999-2014, Broadcom Corporation
 *     All Rights Reserved
 *     Confidential Property of Broadcom Corporation
 *
 *
 * THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED SOFTWARE LICENSE
 * AGREEMENT  BETWEEN THE USER AND BROADCOM.  YOU HAVE NO RIGHT TO USE OR
 * EXPLOIT THIS MATERIAL EXCEPT SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
 *
 * Module Description:
 *                     DO NOT EDIT THIS FILE DIRECTLY
 *
 * This module was generated magically with RDB from a source description
 * file. You must edit the source file for changes to be made to this file.
 *
 *
 * Date:           Generated on               Wed Jun 10 03:09:37 2015
 *                 Full Compile MD5 Checksum  e8bde4108283f67d401938287dbb6bde
 *                     (minus title and desc)
 *                 MD5 Checksum               56f0f944fbd407545cd964e7b4e7f7b0
 *
 * Compiled with:  RDB Utility                combo_header.pl
 *                 RDB.pm                     15517
 *                 unknown                    unknown
 *                 Perl Interpreter           5.008008
 *                 Operating System           linux
 *
 *
 ***************************************************************************/

#ifndef BCHP_AON_PIN_CTRL_H__
#define BCHP_AON_PIN_CTRL_H__

/***************************************************************************
 *AON_PIN_CTRL - AON Pinmux Control Registers
 ***************************************************************************/
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0         0x20410700 /* [RW] Pinmux control register 0 */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1         0x20410704 /* [RW] Pinmux control register 1 */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2         0x20410708 /* [RW] Pinmux control register 2 */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0     0x2041070c /* [RW] Pad pull-up/pull-down control register 0 */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1     0x20410710 /* [RW] Pad pull-up/pull-down control register 1 */
#define BCHP_AON_PIN_CTRL_BYP_CLK_UNSELECT_0     0x20410714 /* [RW] Bypass clock unselect register 0 */

/***************************************************************************
 *PIN_MUX_CTRL_0 - Pinmux control register 0
 ***************************************************************************/
/* AON_PIN_CTRL :: PIN_MUX_CTRL_0 :: aon_gpio_07 [31:28] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_07_MASK          0xf0000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_07_SHIFT         28
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_07_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_07_AON_GPIO_07   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_07_LED_LS_3      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_07_LED_LD_15     2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_07_UART_RXD_3    3
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_07_MTSIF_TX0_DATA2 4

/* AON_PIN_CTRL :: PIN_MUX_CTRL_0 :: aon_gpio_06 [27:24] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_06_MASK          0x0f000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_06_SHIFT         24
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_06_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_06_AON_GPIO_06   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_06_LED_LS_2      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_06_LED_LD_14     2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_06_UART_TXD_3    3
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_06_MTSIF_TX0_DATA1 4

/* AON_PIN_CTRL :: PIN_MUX_CTRL_0 :: aon_gpio_05 [23:20] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_05_MASK          0x00f00000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_05_SHIFT         20
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_05_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_05_AON_GPIO_05   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_05_LED_LS_1      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_05_LED_LD_13     2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_05_SPI_M_SS2B    3
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_05_MTSIF_TX0_DATA0 4

/* AON_PIN_CTRL :: PIN_MUX_CTRL_0 :: aon_gpio_04 [19:16] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_04_MASK          0x000f0000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_04_SHIFT         16
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_04_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_04_AON_GPIO_04   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_04_LED_LS_0      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_04_LED_LD_12     2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_04_SPI_M_SS1B    3
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_04_UART_TXD_1    4

/* AON_PIN_CTRL :: PIN_MUX_CTRL_0 :: aon_gpio_03 [15:12] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_03_MASK          0x0000f000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_03_SHIFT         12
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_03_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_03_AON_GPIO_03   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_03_LED_KD_3      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_03_LED_LD_11     2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_03_SPI_M_SS0B    3
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_03_UART_RTS_1    4
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_03_ZIG_SECO      5

/* AON_PIN_CTRL :: PIN_MUX_CTRL_0 :: aon_gpio_02 [11:08] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_02_MASK          0x00000f00
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_02_SHIFT         8
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_02_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_02_AON_GPIO_02   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_02_LED_KD_2      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_02_LED_LD_10     2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_02_SPI_M_MISO    3
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_02_UART_CTS_1    4
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_02_ZIG_SECI_2    5

/* AON_PIN_CTRL :: PIN_MUX_CTRL_0 :: aon_gpio_01 [07:04] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_01_MASK          0x000000f0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_01_SHIFT         4
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_01_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_01_AON_GPIO_01   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_01_LED_KD_1      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_01_LED_LD_9      2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_01_SPI_M_MOSI    3
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_01_UART_RXD_1    4
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_01_ZIG_SECI_1    5

/* AON_PIN_CTRL :: PIN_MUX_CTRL_0 :: aon_gpio_00 [03:00] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_00_MASK          0x0000000f
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_00_SHIFT         0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_00_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_00_AON_GPIO_00   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_00_LED_KD_0      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_00_LED_LD_8      2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_00_SPI_M_SCK     3
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_0_aon_gpio_00_ZIG_SECI_0    4

/***************************************************************************
 *PIN_MUX_CTRL_1 - Pinmux control register 1
 ***************************************************************************/
/* AON_PIN_CTRL :: PIN_MUX_CTRL_1 :: aon_gpio_15 [31:28] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_15_MASK          0xf0000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_15_SHIFT         28
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_15_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_15_AON_GPIO_15   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_15_LED_LD_6      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_15_UART_RTS_2    2

/* AON_PIN_CTRL :: PIN_MUX_CTRL_1 :: aon_gpio_14 [27:24] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_14_MASK          0x0f000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_14_SHIFT         24
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_14_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_14_AON_GPIO_14   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_14_LED_LD_5      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_14_UART_RXD_2    2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_14_MTSIF_TX0_CLK 3

/* AON_PIN_CTRL :: PIN_MUX_CTRL_1 :: aon_gpio_13 [23:20] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_13_MASK          0x00f00000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_13_SHIFT         20
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_13_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_13_AON_GPIO_13   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_13_LED_LD_4      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_13_UART_TXD_2    2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_13_MTSIF_TX0_SYNC 3

/* AON_PIN_CTRL :: PIN_MUX_CTRL_1 :: aon_gpio_12 [19:16] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_12_MASK          0x000f0000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_12_SHIFT         16
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_12_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_12_AON_GPIO_12   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_12_LED_LD_3      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_12_UART_CTS_2    2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_12_MTSIF_TX0_DATA7 3

/* AON_PIN_CTRL :: PIN_MUX_CTRL_1 :: aon_gpio_11 [15:12] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_11_MASK          0x0000f000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_11_SHIFT         12
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_11_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_11_AON_GPIO_11   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_11_LED_LD_2      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_11_UART_CTS_0    2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_11_MTSIF_TX0_DATA6 3

/* AON_PIN_CTRL :: PIN_MUX_CTRL_1 :: aon_gpio_10 [11:08] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_10_MASK          0x00000f00
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_10_SHIFT         8
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_10_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_10_AON_GPIO_10   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_10_LED_LD_1      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_10_UART_RTS_0    2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_10_MTSIF_TX0_DATA5 3

/* AON_PIN_CTRL :: PIN_MUX_CTRL_1 :: aon_gpio_09 [07:04] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_09_MASK          0x000000f0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_09_SHIFT         4
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_09_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_09_AON_GPIO_09   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_09_LED_LD_0      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_09_UART_RXD_0    2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_09_MTSIF_TX0_DATA4 3

/* AON_PIN_CTRL :: PIN_MUX_CTRL_1 :: aon_gpio_08 [03:00] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_08_MASK          0x0000000f
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_08_SHIFT         0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_08_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_08_AON_GPIO_08   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_08_LED_LS_4      1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_08_UART_TXD_0    2
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_1_aon_gpio_08_MTSIF_TX0_DATA3 3

/***************************************************************************
 *PIN_MUX_CTRL_2 - Pinmux control register 2
 ***************************************************************************/
/* AON_PIN_CTRL :: PIN_MUX_CTRL_2 :: aon_sgpio_03 [31:28] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_03_MASK         0xf0000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_03_SHIFT        28
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_03_DEFAULT      0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_03_AON_SGPIO_03 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_03_BSC_M1_SDA   1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_03_BSC_S1_SDA   2

/* AON_PIN_CTRL :: PIN_MUX_CTRL_2 :: aon_sgpio_02 [27:24] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_02_MASK         0x0f000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_02_SHIFT        24
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_02_DEFAULT      0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_02_AON_SGPIO_02 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_02_BSC_M1_SCL   1
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_02_BSC_S1_SCL   2

/* AON_PIN_CTRL :: PIN_MUX_CTRL_2 :: aon_sgpio_01 [23:20] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_01_MASK         0x00f00000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_01_SHIFT        20
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_01_DEFAULT      0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_01_AON_SGPIO_01 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_01_BSC_M0_SDA   1

/* AON_PIN_CTRL :: PIN_MUX_CTRL_2 :: aon_sgpio_00 [19:16] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_00_MASK         0x000f0000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_00_SHIFT        16
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_00_DEFAULT      0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_00_AON_SGPIO_00 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_sgpio_00_BSC_M0_SCL   1

/* AON_PIN_CTRL :: PIN_MUX_CTRL_2 :: aon_gpio_19 [15:12] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_19_MASK          0x0000f000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_19_SHIFT         12
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_19_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_19_AON_GPIO_19   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_19_ZIG_LNA_CTL   1

/* AON_PIN_CTRL :: PIN_MUX_CTRL_2 :: aon_gpio_18 [11:08] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_18_MASK          0x00000f00
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_18_SHIFT         8
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_18_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_18_AON_GPIO_18   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_18_ZIG_ANT_SEL   1

/* AON_PIN_CTRL :: PIN_MUX_CTRL_2 :: aon_gpio_17 [07:04] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_17_MASK          0x000000f0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_17_SHIFT         4
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_17_DEFAULT       0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_17_AON_GPIO_17   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_17_LED_OUT       1

/* AON_PIN_CTRL :: PIN_MUX_CTRL_2 :: aon_gpio_16 [03:00] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_16_MASK          0x0000000f
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_16_SHIFT         0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_16_DEFAULT       0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_16_AON_GPIO_16   0
#define BCHP_AON_PIN_CTRL_PIN_MUX_CTRL_2_aon_gpio_16_LED_LD_7      1

/***************************************************************************
 *PIN_MUX_PAD_CTRL_0 - Pad pull-up/pull-down control register 0
 ***************************************************************************/
/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: spare_pad_ctrl_0 [31:30] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_spare_pad_ctrl_0_MASK 0xc0000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_spare_pad_ctrl_0_SHIFT 30
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_spare_pad_ctrl_0_DEFAULT 0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_spare_pad_ctrl_0_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_spare_pad_ctrl_0_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_spare_pad_ctrl_0_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: aon_gpio_10_pad_ctrl [29:28] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_10_pad_ctrl_MASK 0x30000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_10_pad_ctrl_SHIFT 28
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_10_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_10_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_10_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_10_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: aon_gpio_09_pad_ctrl [27:26] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_09_pad_ctrl_MASK 0x0c000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_09_pad_ctrl_SHIFT 26
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_09_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_09_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_09_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_09_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: aon_gpio_08_pad_ctrl [25:24] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_08_pad_ctrl_MASK 0x03000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_08_pad_ctrl_SHIFT 24
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_08_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_08_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_08_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_08_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: aon_gpio_07_pad_ctrl [23:22] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_07_pad_ctrl_MASK 0x00c00000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_07_pad_ctrl_SHIFT 22
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_07_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_07_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_07_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_07_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: aon_gpio_06_pad_ctrl [21:20] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_06_pad_ctrl_MASK 0x00300000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_06_pad_ctrl_SHIFT 20
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_06_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_06_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_06_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_06_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: aon_gpio_05_pad_ctrl [19:18] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_05_pad_ctrl_MASK 0x000c0000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_05_pad_ctrl_SHIFT 18
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_05_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_05_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_05_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_05_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: reserved0 [17:14] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_reserved0_MASK        0x0003c000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_reserved0_SHIFT       14

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: aon_gpio_02_pad_ctrl [13:12] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_02_pad_ctrl_MASK 0x00003000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_02_pad_ctrl_SHIFT 12
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_02_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_02_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_02_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_02_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: aon_gpio_01_pad_ctrl [11:10] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_01_pad_ctrl_MASK 0x00000c00
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_01_pad_ctrl_SHIFT 10
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_01_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_01_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_01_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_01_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: aon_gpio_00_pad_ctrl [09:08] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_00_pad_ctrl_MASK 0x00000300
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_00_pad_ctrl_SHIFT 8
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_00_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_00_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_00_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_aon_gpio_00_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_0 :: reserved1 [07:00] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_reserved1_MASK        0x000000ff
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_0_reserved1_SHIFT       0

/***************************************************************************
 *PIN_MUX_PAD_CTRL_1 - Pad pull-up/pull-down control register 1
 ***************************************************************************/
/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_1 :: reserved0 [31:28] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_reserved0_MASK        0xf0000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_reserved0_SHIFT       28

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_1 :: spare_pad_ctrl_1 [27:26] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_spare_pad_ctrl_1_MASK 0x0c000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_spare_pad_ctrl_1_SHIFT 26
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_spare_pad_ctrl_1_DEFAULT 0x00000000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_spare_pad_ctrl_1_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_spare_pad_ctrl_1_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_spare_pad_ctrl_1_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_1 :: reserved1 [25:14] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_reserved1_MASK        0x03ffc000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_reserved1_SHIFT       14

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_1 :: aon_gpio_17_pad_ctrl [13:12] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_17_pad_ctrl_MASK 0x00003000
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_17_pad_ctrl_SHIFT 12
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_17_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_17_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_17_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_17_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_1 :: reserved2 [11:08] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_reserved2_MASK        0x00000f00
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_reserved2_SHIFT       8

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_1 :: aon_gpio_14_pad_ctrl [07:06] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_14_pad_ctrl_MASK 0x000000c0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_14_pad_ctrl_SHIFT 6
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_14_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_14_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_14_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_14_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_1 :: aon_gpio_13_pad_ctrl [05:04] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_13_pad_ctrl_MASK 0x00000030
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_13_pad_ctrl_SHIFT 4
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_13_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_13_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_13_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_13_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_1 :: aon_gpio_12_pad_ctrl [03:02] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_12_pad_ctrl_MASK 0x0000000c
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_12_pad_ctrl_SHIFT 2
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_12_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_12_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_12_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_12_pad_ctrl_PULL_UP 2

/* AON_PIN_CTRL :: PIN_MUX_PAD_CTRL_1 :: aon_gpio_11_pad_ctrl [01:00] */
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_11_pad_ctrl_MASK 0x00000003
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_11_pad_ctrl_SHIFT 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_11_pad_ctrl_DEFAULT 0x00000001
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_11_pad_ctrl_PULL_NONE 0
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_11_pad_ctrl_PULL_DOWN 1
#define BCHP_AON_PIN_CTRL_PIN_MUX_PAD_CTRL_1_aon_gpio_11_pad_ctrl_PULL_UP 2

/***************************************************************************
 *BYP_CLK_UNSELECT_0 - Bypass clock unselect register 0
 ***************************************************************************/
/* AON_PIN_CTRL :: BYP_CLK_UNSELECT_0 :: reserved0 [31:02] */
#define BCHP_AON_PIN_CTRL_BYP_CLK_UNSELECT_0_reserved0_MASK        0xfffffffc
#define BCHP_AON_PIN_CTRL_BYP_CLK_UNSELECT_0_reserved0_SHIFT       2

/* AON_PIN_CTRL :: BYP_CLK_UNSELECT_0 :: unsel_byp_clk_on_aon_gpio_11 [01:01] */
#define BCHP_AON_PIN_CTRL_BYP_CLK_UNSELECT_0_unsel_byp_clk_on_aon_gpio_11_MASK 0x00000002
#define BCHP_AON_PIN_CTRL_BYP_CLK_UNSELECT_0_unsel_byp_clk_on_aon_gpio_11_SHIFT 1
#define BCHP_AON_PIN_CTRL_BYP_CLK_UNSELECT_0_unsel_byp_clk_on_aon_gpio_11_DEFAULT 0x00000000

/* AON_PIN_CTRL :: BYP_CLK_UNSELECT_0 :: unsel_byp_clk_on_aon_gpio_09 [00:00] */
#define BCHP_AON_PIN_CTRL_BYP_CLK_UNSELECT_0_unsel_byp_clk_on_aon_gpio_09_MASK 0x00000001
#define BCHP_AON_PIN_CTRL_BYP_CLK_UNSELECT_0_unsel_byp_clk_on_aon_gpio_09_SHIFT 0
#define BCHP_AON_PIN_CTRL_BYP_CLK_UNSELECT_0_unsel_byp_clk_on_aon_gpio_09_DEFAULT 0x00000000

#endif /* #ifndef BCHP_AON_PIN_CTRL_H__ */

/* End of File */
