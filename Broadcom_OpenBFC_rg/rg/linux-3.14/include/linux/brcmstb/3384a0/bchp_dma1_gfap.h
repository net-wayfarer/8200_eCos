/***************************************************************************
 *     Copyright (c) 1999-2014, Broadcom Corporation
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
 * Date:           Generated on              Thu Sep  4 04:51:46 2014
 *                 Full Compile MD5 Checksum 8b9bd5137765e5dce9d70a0afe4b52c1
 *                   (minus title and desc)
 *                 MD5 Checksum              806e214b76041b03796d46a801f2d917
 *
 * Compiled with:  RDB Utility               combo_header.pl
 *                 RDB Parser                3.0
 *                 unknown                   unknown
 *                 Perl Interpreter          5.008008
 *                 Operating System          linux
 *
 * Revision History:
 *
 * $brcm_Log: $
 *
 ***************************************************************************/

#ifndef BCHP_DMA1_GFAP_H__
#define BCHP_DMA1_GFAP_H__

/***************************************************************************
 *DMA1_GFAP
 ***************************************************************************/
#define BCHP_DMA1_GFAP_DMA_SRC                   0x13c01320 /* DMA SRC Register */
#define BCHP_DMA1_GFAP_DMA_DEST                  0x13c01324 /* DMA DEST Register */
#define BCHP_DMA1_GFAP_DMA_CMD_LIST              0x13c01328 /* DMA CMD LIST Register */
#define BCHP_DMA1_GFAP_DMA_LEN_CTL               0x13c0132c /* DMA LEN CTL Register */
#define BCHP_DMA1_GFAP_DMA_RSLT_SRC              0x13c01330 /* DMA RSLT SRC Register */
#define BCHP_DMA1_GFAP_DMA_RSLT_DEST             0x13c01334 /* DMA RSLT DEST Register */
#define BCHP_DMA1_GFAP_DMA_RSLT_HCS              0x13c01338 /* DMA RSLT HCS Register */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT         0x13c0133c /* DMA RSLT LEN STAT Register */

/***************************************************************************
 *DMA_SRC - DMA SRC Register
 ***************************************************************************/
/* DMA1_GFAP :: DMA_SRC :: SOURCE [31:00] */
#define BCHP_DMA1_GFAP_DMA_SRC_SOURCE_MASK                         0xffffffff
#define BCHP_DMA1_GFAP_DMA_SRC_SOURCE_SHIFT                        0
#define BCHP_DMA1_GFAP_DMA_SRC_SOURCE_DEFAULT                      0x00000000

/***************************************************************************
 *DMA_DEST - DMA DEST Register
 ***************************************************************************/
/* DMA1_GFAP :: DMA_DEST :: DEST [31:00] */
#define BCHP_DMA1_GFAP_DMA_DEST_DEST_MASK                          0xffffffff
#define BCHP_DMA1_GFAP_DMA_DEST_DEST_SHIFT                         0
#define BCHP_DMA1_GFAP_DMA_DEST_DEST_DEFAULT                       0x00000000

/***************************************************************************
 *DMA_CMD_LIST - DMA CMD LIST Register
 ***************************************************************************/
/* DMA1_GFAP :: DMA_CMD_LIST :: CMD_LIST [31:00] */
#define BCHP_DMA1_GFAP_DMA_CMD_LIST_CMD_LIST_MASK                  0xffffffff
#define BCHP_DMA1_GFAP_DMA_CMD_LIST_CMD_LIST_SHIFT                 0
#define BCHP_DMA1_GFAP_DMA_CMD_LIST_CMD_LIST_DEFAULT               0x00000000

/***************************************************************************
 *DMA_LEN_CTL - DMA LEN CTL Register
 ***************************************************************************/
/* DMA1_GFAP :: DMA_LEN_CTL :: LENGTH_N_VALUE [31:18] */
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_LENGTH_N_VALUE_MASK             0xfffc0000
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_LENGTH_N_VALUE_SHIFT            18
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_LENGTH_N_VALUE_DEFAULT          0x00000000

/* DMA1_GFAP :: DMA_LEN_CTL :: WAIT [17:17] */
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_WAIT_MASK                       0x00020000
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_WAIT_SHIFT                      17
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_WAIT_DEFAULT                    0x00000000

/* DMA1_GFAP :: DMA_LEN_CTL :: EXEC_CMD_LIST [16:16] */
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_EXEC_CMD_LIST_MASK              0x00010000
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_EXEC_CMD_LIST_SHIFT             16
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_EXEC_CMD_LIST_DEFAULT           0x00000000

/* DMA1_GFAP :: DMA_LEN_CTL :: DEST_ADDR [15:14] */
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_DEST_ADDR_MASK                  0x0000c000
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_DEST_ADDR_SHIFT                 14
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_DEST_ADDR_DEFAULT               0x00000000

/* DMA1_GFAP :: DMA_LEN_CTL :: SRC_IS_TOKEN [13:13] */
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_SRC_IS_TOKEN_MASK               0x00002000
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_SRC_IS_TOKEN_SHIFT              13
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_SRC_IS_TOKEN_DEFAULT            0x00000000

/* DMA1_GFAP :: DMA_LEN_CTL :: CONTINUE [12:12] */
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_CONTINUE_MASK                   0x00001000
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_CONTINUE_SHIFT                  12
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_CONTINUE_DEFAULT                0x00000000

/* DMA1_GFAP :: DMA_LEN_CTL :: DMA_LEN [11:00] */
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_DMA_LEN_MASK                    0x00000fff
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_DMA_LEN_SHIFT                   0
#define BCHP_DMA1_GFAP_DMA_LEN_CTL_DMA_LEN_DEFAULT                 0x00000000

/***************************************************************************
 *DMA_RSLT_SRC - DMA RSLT SRC Register
 ***************************************************************************/
/* DMA1_GFAP :: DMA_RSLT_SRC :: RSLT_SOURCE [31:00] */
#define BCHP_DMA1_GFAP_DMA_RSLT_SRC_RSLT_SOURCE_MASK               0xffffffff
#define BCHP_DMA1_GFAP_DMA_RSLT_SRC_RSLT_SOURCE_SHIFT              0
#define BCHP_DMA1_GFAP_DMA_RSLT_SRC_RSLT_SOURCE_DEFAULT            0x00000000

/***************************************************************************
 *DMA_RSLT_DEST - DMA RSLT DEST Register
 ***************************************************************************/
/* DMA1_GFAP :: DMA_RSLT_DEST :: RSLT_DEST [31:00] */
#define BCHP_DMA1_GFAP_DMA_RSLT_DEST_RSLT_DEST_MASK                0xffffffff
#define BCHP_DMA1_GFAP_DMA_RSLT_DEST_RSLT_DEST_SHIFT               0
#define BCHP_DMA1_GFAP_DMA_RSLT_DEST_RSLT_DEST_DEFAULT             0x00000000

/***************************************************************************
 *DMA_RSLT_HCS - DMA RSLT HCS Register
 ***************************************************************************/
/* DMA1_GFAP :: DMA_RSLT_HCS :: HCS1_VALUE [31:16] */
#define BCHP_DMA1_GFAP_DMA_RSLT_HCS_HCS1_VALUE_MASK                0xffff0000
#define BCHP_DMA1_GFAP_DMA_RSLT_HCS_HCS1_VALUE_SHIFT               16
#define BCHP_DMA1_GFAP_DMA_RSLT_HCS_HCS1_VALUE_DEFAULT             0x00000000

/* DMA1_GFAP :: DMA_RSLT_HCS :: HCS0_VALUE [15:00] */
#define BCHP_DMA1_GFAP_DMA_RSLT_HCS_HCS0_VALUE_MASK                0x0000ffff
#define BCHP_DMA1_GFAP_DMA_RSLT_HCS_HCS0_VALUE_SHIFT               0
#define BCHP_DMA1_GFAP_DMA_RSLT_HCS_HCS0_VALUE_DEFAULT             0x00000000

/***************************************************************************
 *DMA_RSLT_LEN_STAT - DMA RSLT LEN STAT Register
 ***************************************************************************/
/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: reserved0 [31:23] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_reserved0_MASK            0xff800000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_reserved0_SHIFT           23

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: ZERO_BYTE_UBUS_WRITE [22:22] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ZERO_BYTE_UBUS_WRITE_MASK 0x00400000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ZERO_BYTE_UBUS_WRITE_SHIFT 22
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ZERO_BYTE_UBUS_WRITE_DEFAULT 0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: NOT_END_CMNDS [21:21] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_NOT_END_CMNDS_MASK        0x00200000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_NOT_END_CMNDS_SHIFT       21
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_NOT_END_CMNDS_DEFAULT     0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: FLUSHED [20:20] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_FLUSHED_MASK              0x00100000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_FLUSHED_SHIFT             20
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_FLUSHED_DEFAULT           0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: ABORTED [19:19] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ABORTED_MASK              0x00080000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ABORTED_SHIFT             19
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ABORTED_DEFAULT           0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: ERR_CMD_FMT [18:18] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_CMD_FMT_MASK          0x00040000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_CMD_FMT_SHIFT         18
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_CMD_FMT_DEFAULT       0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: ERR_DEST [17:17] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_DEST_MASK             0x00020000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_DEST_SHIFT            17
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_DEST_DEFAULT          0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: ERR_SRC [16:16] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_SRC_MASK              0x00010000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_SRC_SHIFT             16
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_SRC_DEFAULT           0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: ERR_CMD_LIST [15:15] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_CMD_LIST_MASK         0x00008000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_CMD_LIST_SHIFT        15
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_CMD_LIST_DEFAULT      0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: ERR_DEST_LEN [14:14] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_DEST_LEN_MASK         0x00004000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_DEST_LEN_SHIFT        14
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_DEST_LEN_DEFAULT      0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: ERR_SRC_LEN [13:13] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_SRC_LEN_MASK          0x00002000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_SRC_LEN_SHIFT         13
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_ERR_SRC_LEN_DEFAULT       0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: CONTINUE [12:12] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_CONTINUE_MASK             0x00001000
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_CONTINUE_SHIFT            12
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_CONTINUE_DEFAULT          0x00000000

/* DMA1_GFAP :: DMA_RSLT_LEN_STAT :: DMA_LEN [11:00] */
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_DMA_LEN_MASK              0x00000fff
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_DMA_LEN_SHIFT             0
#define BCHP_DMA1_GFAP_DMA_RSLT_LEN_STAT_DMA_LEN_DEFAULT           0x00000000

#endif /* #ifndef BCHP_DMA1_GFAP_H__ */

/* End of File */
