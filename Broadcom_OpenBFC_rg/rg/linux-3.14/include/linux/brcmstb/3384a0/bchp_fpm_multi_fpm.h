/***************************************************************************
 *     Copyright (c) 1999-2012, Broadcom Corporation
 *     All Rights Reserved
 *     Confidential Property of Broadcom Corporation
 *
 *
 * THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED SOFTWARE LICENSE
 * AGREEMENT  BETWEEN THE USER AND BROADCOM.  YOU HAVE NO RIGHT TO USE OR
 * EXPLOIT THIS MATERIAL EXCEPT SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
 *
 * $brcm_Workfile: $
 * $brcm_Revision: $
 * $brcm_Date: $
 *
 * Module Description:
 *                     DO NOT EDIT THIS FILE DIRECTLY
 *
 * This module was generated magically with RDB from a source description
 * file. You must edit the source file for changes to be made to this file.
 *
 *
 * Date:           Generated on         Wed Nov 14 03:20:37 2012
 *                 MD5 Checksum         d41d8cd98f00b204e9800998ecf8427e
 *
 * Compiled with:  RDB Utility          combo_header.pl
 *                 RDB Parser           3.0
 *                 unknown              unknown
 *                 Perl Interpreter     5.008008
 *                 Operating System     linux
 *
 * Revision History:
 *
 * $brcm_Log: $
 *
 ***************************************************************************/

#ifndef BCHP_FPM_MULTI_FPM_H__
#define BCHP_FPM_MULTI_FPM_H__

/***************************************************************************
 *FPM_MULTI_FPM - FPM Multicast Memory Access FPM
 ***************************************************************************/

/***************************************************************************
 *multicast_data%i - Multicast memory space.
 ***************************************************************************/
#define BCHP_FPM_MULTI_FPM_multicast_datai_ARRAY_BASE              0x0000000012210000
#define BCHP_FPM_MULTI_FPM_multicast_datai_ARRAY_START             0
#define BCHP_FPM_MULTI_FPM_multicast_datai_ARRAY_END               2047
#define BCHP_FPM_MULTI_FPM_multicast_datai_ARRAY_ELEMENT_SIZE      64

/***************************************************************************
 *multicast_data%i - Multicast memory space.
 ***************************************************************************/
/* FPM_MULTI_FPM :: multicast_datai :: reserved0 [63:56] */
#define BCHP_FPM_MULTI_FPM_multicast_datai_reserved0_MASK          0xff00000000000000
#define BCHP_FPM_MULTI_FPM_multicast_datai_reserved0_SHIFT         56

/* FPM_MULTI_FPM :: multicast_datai :: Multicast7 [55:49] */
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast7_MASK         0x00fe000000000000
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast7_SHIFT        49

/* FPM_MULTI_FPM :: multicast_datai :: Multicast6 [48:42] */
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast6_MASK         0x0001fc0000000000
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast6_SHIFT        42

/* FPM_MULTI_FPM :: multicast_datai :: Multicast5 [41:35] */
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast5_MASK         0x000003f800000000
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast5_SHIFT        35

/* FPM_MULTI_FPM :: multicast_datai :: Multicast4 [34:28] */
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast4_MASK         0x00000007f0000000
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast4_SHIFT        28

/* FPM_MULTI_FPM :: multicast_datai :: Multicast3 [27:21] */
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast3_MASK         0x000000000fe00000
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast3_SHIFT        21

/* FPM_MULTI_FPM :: multicast_datai :: Multicast2 [20:14] */
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast2_MASK         0x00000000001fc000
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast2_SHIFT        14

/* FPM_MULTI_FPM :: multicast_datai :: Multicast1 [13:07] */
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast1_MASK         0x0000000000003f80
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast1_SHIFT        7

/* FPM_MULTI_FPM :: multicast_datai :: Multicast0 [06:00] */
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast0_MASK         0x000000000000007f
#define BCHP_FPM_MULTI_FPM_multicast_datai_Multicast0_SHIFT        0


#endif /* #ifndef BCHP_FPM_MULTI_FPM_H__ */

/* End of File */