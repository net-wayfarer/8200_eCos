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
 * Date:           Generated on               Wed Jun 10 03:09:40 2015
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

#ifndef BCHP_GIO_AON_H__
#define BCHP_GIO_AON_H__

/***************************************************************************
 *GIO_AON - GPIO
 ***************************************************************************/
#define BCHP_GIO_AON_ODEN_LO                     0x20417000 /* [RW] GENERAL PURPOSE I/O OPEN DRAIN ENABLE FOR AON_GPIO[19:0] */
#define BCHP_GIO_AON_DATA_LO                     0x20417004 /* [RW] GENERAL PURPOSE I/O DATA FOR AON_GPIO[19:0] */
#define BCHP_GIO_AON_IODIR_LO                    0x20417008 /* [RW] GENERAL PURPOSE I/O DIRECTION FOR AON_GPIO[19:0] */
#define BCHP_GIO_AON_EC_LO                       0x2041700c /* [RW] GENERAL PURPOSE I/O EDGE CONFIGURATION FOR AON_GPIO[19:0] */
#define BCHP_GIO_AON_EI_LO                       0x20417010 /* [RW] GENERAL PURPOSE I/O EDGE INSENSITIVE FOR AON_GPIO[19:0] */
#define BCHP_GIO_AON_MASK_LO                     0x20417014 /* [RW] GENERAL PURPOSE I/O INTERRUPT MASK FOR AON_GPIO[19:0] */
#define BCHP_GIO_AON_LEVEL_LO                    0x20417018 /* [RW] GENERAL PURPOSE I/O INTERRUPT TYPE FOR AON_GPIO[19:0] */
#define BCHP_GIO_AON_STAT_LO                     0x2041701c /* [RW] GENERAL PURPOSE I/O INTERRUPT STATUS FOR AON_GPIO[19:0] */
#define BCHP_GIO_AON_ODEN_EXT                    0x20417020 /* [RW] GENERAL PURPOSE I/O OPEN DRAIN ENABLE FOR AON_SGPIO[3:2] */
#define BCHP_GIO_AON_DATA_EXT                    0x20417024 /* [RW] GENERAL PURPOSE I/O DATA FOR AON_SGPIO[3:2] */
#define BCHP_GIO_AON_IODIR_EXT                   0x20417028 /* [RW] GENERAL PURPOSE I/O DIRECTION FOR AON_SGPIO[3:2] */
#define BCHP_GIO_AON_EC_EXT                      0x2041702c /* [RW] GENERAL PURPOSE I/O EDGE CONFIGURATION FOR AON_SGPIO[3:2] */
#define BCHP_GIO_AON_EI_EXT                      0x20417030 /* [RW] GENERAL PURPOSE I/O EDGE INSENSITIVE FOR AON_SGPIO[3:2] */
#define BCHP_GIO_AON_MASK_EXT                    0x20417034 /* [RW] GENERAL PURPOSE I/O INTERRUPT MASK FOR AON_SGPIO[3:2] */
#define BCHP_GIO_AON_LEVEL_EXT                   0x20417038 /* [RW] GENERAL PURPOSE I/O INTERRUPT TYPE FOR AON_SGPIO[3:2] */
#define BCHP_GIO_AON_STAT_EXT                    0x2041703c /* [RW] GENERAL PURPOSE I/O INTERRUPT STATUS FOR AON_SGPIO[3:2] */

/***************************************************************************
 *ODEN_LO - GENERAL PURPOSE I/O OPEN DRAIN ENABLE FOR AON_GPIO[19:0]
 ***************************************************************************/
/* GIO_AON :: ODEN_LO :: reserved0 [31:20] */
#define BCHP_GIO_AON_ODEN_LO_reserved0_MASK                        0xfff00000
#define BCHP_GIO_AON_ODEN_LO_reserved0_SHIFT                       20

/* GIO_AON :: ODEN_LO :: oden [19:00] */
#define BCHP_GIO_AON_ODEN_LO_oden_MASK                             0x000fffff
#define BCHP_GIO_AON_ODEN_LO_oden_SHIFT                            0
#define BCHP_GIO_AON_ODEN_LO_oden_DEFAULT                          0x00000000

/***************************************************************************
 *DATA_LO - GENERAL PURPOSE I/O DATA FOR AON_GPIO[19:0]
 ***************************************************************************/
/* GIO_AON :: DATA_LO :: reserved0 [31:20] */
#define BCHP_GIO_AON_DATA_LO_reserved0_MASK                        0xfff00000
#define BCHP_GIO_AON_DATA_LO_reserved0_SHIFT                       20

/* GIO_AON :: DATA_LO :: data [19:00] */
#define BCHP_GIO_AON_DATA_LO_data_MASK                             0x000fffff
#define BCHP_GIO_AON_DATA_LO_data_SHIFT                            0
#define BCHP_GIO_AON_DATA_LO_data_DEFAULT                          0x00000000

/***************************************************************************
 *IODIR_LO - GENERAL PURPOSE I/O DIRECTION FOR AON_GPIO[19:0]
 ***************************************************************************/
/* GIO_AON :: IODIR_LO :: reserved0 [31:20] */
#define BCHP_GIO_AON_IODIR_LO_reserved0_MASK                       0xfff00000
#define BCHP_GIO_AON_IODIR_LO_reserved0_SHIFT                      20

/* GIO_AON :: IODIR_LO :: iodir [19:00] */
#define BCHP_GIO_AON_IODIR_LO_iodir_MASK                           0x000fffff
#define BCHP_GIO_AON_IODIR_LO_iodir_SHIFT                          0
#define BCHP_GIO_AON_IODIR_LO_iodir_DEFAULT                        0x000fffff

/***************************************************************************
 *EC_LO - GENERAL PURPOSE I/O EDGE CONFIGURATION FOR AON_GPIO[19:0]
 ***************************************************************************/
/* GIO_AON :: EC_LO :: reserved0 [31:20] */
#define BCHP_GIO_AON_EC_LO_reserved0_MASK                          0xfff00000
#define BCHP_GIO_AON_EC_LO_reserved0_SHIFT                         20

/* GIO_AON :: EC_LO :: edge_config [19:00] */
#define BCHP_GIO_AON_EC_LO_edge_config_MASK                        0x000fffff
#define BCHP_GIO_AON_EC_LO_edge_config_SHIFT                       0
#define BCHP_GIO_AON_EC_LO_edge_config_DEFAULT                     0x00000000

/***************************************************************************
 *EI_LO - GENERAL PURPOSE I/O EDGE INSENSITIVE FOR AON_GPIO[19:0]
 ***************************************************************************/
/* GIO_AON :: EI_LO :: reserved0 [31:20] */
#define BCHP_GIO_AON_EI_LO_reserved0_MASK                          0xfff00000
#define BCHP_GIO_AON_EI_LO_reserved0_SHIFT                         20

/* GIO_AON :: EI_LO :: edge_insensitive [19:00] */
#define BCHP_GIO_AON_EI_LO_edge_insensitive_MASK                   0x000fffff
#define BCHP_GIO_AON_EI_LO_edge_insensitive_SHIFT                  0
#define BCHP_GIO_AON_EI_LO_edge_insensitive_DEFAULT                0x00000000

/***************************************************************************
 *MASK_LO - GENERAL PURPOSE I/O INTERRUPT MASK FOR AON_GPIO[19:0]
 ***************************************************************************/
/* GIO_AON :: MASK_LO :: reserved0 [31:20] */
#define BCHP_GIO_AON_MASK_LO_reserved0_MASK                        0xfff00000
#define BCHP_GIO_AON_MASK_LO_reserved0_SHIFT                       20

/* GIO_AON :: MASK_LO :: irq_mask [19:00] */
#define BCHP_GIO_AON_MASK_LO_irq_mask_MASK                         0x000fffff
#define BCHP_GIO_AON_MASK_LO_irq_mask_SHIFT                        0
#define BCHP_GIO_AON_MASK_LO_irq_mask_DEFAULT                      0x00000000

/***************************************************************************
 *LEVEL_LO - GENERAL PURPOSE I/O INTERRUPT TYPE FOR AON_GPIO[19:0]
 ***************************************************************************/
/* GIO_AON :: LEVEL_LO :: reserved0 [31:20] */
#define BCHP_GIO_AON_LEVEL_LO_reserved0_MASK                       0xfff00000
#define BCHP_GIO_AON_LEVEL_LO_reserved0_SHIFT                      20

/* GIO_AON :: LEVEL_LO :: level [19:00] */
#define BCHP_GIO_AON_LEVEL_LO_level_MASK                           0x000fffff
#define BCHP_GIO_AON_LEVEL_LO_level_SHIFT                          0
#define BCHP_GIO_AON_LEVEL_LO_level_DEFAULT                        0x00000000

/***************************************************************************
 *STAT_LO - GENERAL PURPOSE I/O INTERRUPT STATUS FOR AON_GPIO[19:0]
 ***************************************************************************/
/* GIO_AON :: STAT_LO :: reserved0 [31:20] */
#define BCHP_GIO_AON_STAT_LO_reserved0_MASK                        0xfff00000
#define BCHP_GIO_AON_STAT_LO_reserved0_SHIFT                       20

/* GIO_AON :: STAT_LO :: irq_status [19:00] */
#define BCHP_GIO_AON_STAT_LO_irq_status_MASK                       0x000fffff
#define BCHP_GIO_AON_STAT_LO_irq_status_SHIFT                      0
#define BCHP_GIO_AON_STAT_LO_irq_status_DEFAULT                    0x00000000

/***************************************************************************
 *ODEN_EXT - GENERAL PURPOSE I/O OPEN DRAIN ENABLE FOR AON_SGPIO[3:2]
 ***************************************************************************/
/* GIO_AON :: ODEN_EXT :: reserved0 [31:02] */
#define BCHP_GIO_AON_ODEN_EXT_reserved0_MASK                       0xfffffffc
#define BCHP_GIO_AON_ODEN_EXT_reserved0_SHIFT                      2

/* GIO_AON :: ODEN_EXT :: oden [01:00] */
#define BCHP_GIO_AON_ODEN_EXT_oden_MASK                            0x00000003
#define BCHP_GIO_AON_ODEN_EXT_oden_SHIFT                           0
#define BCHP_GIO_AON_ODEN_EXT_oden_DEFAULT                         0x00000000

/***************************************************************************
 *DATA_EXT - GENERAL PURPOSE I/O DATA FOR AON_SGPIO[3:2]
 ***************************************************************************/
/* GIO_AON :: DATA_EXT :: reserved0 [31:02] */
#define BCHP_GIO_AON_DATA_EXT_reserved0_MASK                       0xfffffffc
#define BCHP_GIO_AON_DATA_EXT_reserved0_SHIFT                      2

/* GIO_AON :: DATA_EXT :: data [01:00] */
#define BCHP_GIO_AON_DATA_EXT_data_MASK                            0x00000003
#define BCHP_GIO_AON_DATA_EXT_data_SHIFT                           0
#define BCHP_GIO_AON_DATA_EXT_data_DEFAULT                         0x00000000

/***************************************************************************
 *IODIR_EXT - GENERAL PURPOSE I/O DIRECTION FOR AON_SGPIO[3:2]
 ***************************************************************************/
/* GIO_AON :: IODIR_EXT :: reserved0 [31:02] */
#define BCHP_GIO_AON_IODIR_EXT_reserved0_MASK                      0xfffffffc
#define BCHP_GIO_AON_IODIR_EXT_reserved0_SHIFT                     2

/* GIO_AON :: IODIR_EXT :: iodir [01:00] */
#define BCHP_GIO_AON_IODIR_EXT_iodir_MASK                          0x00000003
#define BCHP_GIO_AON_IODIR_EXT_iodir_SHIFT                         0
#define BCHP_GIO_AON_IODIR_EXT_iodir_DEFAULT                       0x00000003

/***************************************************************************
 *EC_EXT - GENERAL PURPOSE I/O EDGE CONFIGURATION FOR AON_SGPIO[3:2]
 ***************************************************************************/
/* GIO_AON :: EC_EXT :: reserved0 [31:02] */
#define BCHP_GIO_AON_EC_EXT_reserved0_MASK                         0xfffffffc
#define BCHP_GIO_AON_EC_EXT_reserved0_SHIFT                        2

/* GIO_AON :: EC_EXT :: edge_config [01:00] */
#define BCHP_GIO_AON_EC_EXT_edge_config_MASK                       0x00000003
#define BCHP_GIO_AON_EC_EXT_edge_config_SHIFT                      0
#define BCHP_GIO_AON_EC_EXT_edge_config_DEFAULT                    0x00000000

/***************************************************************************
 *EI_EXT - GENERAL PURPOSE I/O EDGE INSENSITIVE FOR AON_SGPIO[3:2]
 ***************************************************************************/
/* GIO_AON :: EI_EXT :: reserved0 [31:02] */
#define BCHP_GIO_AON_EI_EXT_reserved0_MASK                         0xfffffffc
#define BCHP_GIO_AON_EI_EXT_reserved0_SHIFT                        2

/* GIO_AON :: EI_EXT :: edge_insensitive [01:00] */
#define BCHP_GIO_AON_EI_EXT_edge_insensitive_MASK                  0x00000003
#define BCHP_GIO_AON_EI_EXT_edge_insensitive_SHIFT                 0
#define BCHP_GIO_AON_EI_EXT_edge_insensitive_DEFAULT               0x00000000

/***************************************************************************
 *MASK_EXT - GENERAL PURPOSE I/O INTERRUPT MASK FOR AON_SGPIO[3:2]
 ***************************************************************************/
/* GIO_AON :: MASK_EXT :: reserved0 [31:02] */
#define BCHP_GIO_AON_MASK_EXT_reserved0_MASK                       0xfffffffc
#define BCHP_GIO_AON_MASK_EXT_reserved0_SHIFT                      2

/* GIO_AON :: MASK_EXT :: irq_mask [01:00] */
#define BCHP_GIO_AON_MASK_EXT_irq_mask_MASK                        0x00000003
#define BCHP_GIO_AON_MASK_EXT_irq_mask_SHIFT                       0
#define BCHP_GIO_AON_MASK_EXT_irq_mask_DEFAULT                     0x00000000

/***************************************************************************
 *LEVEL_EXT - GENERAL PURPOSE I/O INTERRUPT TYPE FOR AON_SGPIO[3:2]
 ***************************************************************************/
/* GIO_AON :: LEVEL_EXT :: reserved0 [31:02] */
#define BCHP_GIO_AON_LEVEL_EXT_reserved0_MASK                      0xfffffffc
#define BCHP_GIO_AON_LEVEL_EXT_reserved0_SHIFT                     2

/* GIO_AON :: LEVEL_EXT :: level [01:00] */
#define BCHP_GIO_AON_LEVEL_EXT_level_MASK                          0x00000003
#define BCHP_GIO_AON_LEVEL_EXT_level_SHIFT                         0
#define BCHP_GIO_AON_LEVEL_EXT_level_DEFAULT                       0x00000000

/***************************************************************************
 *STAT_EXT - GENERAL PURPOSE I/O INTERRUPT STATUS FOR AON_SGPIO[3:2]
 ***************************************************************************/
/* GIO_AON :: STAT_EXT :: reserved0 [31:02] */
#define BCHP_GIO_AON_STAT_EXT_reserved0_MASK                       0xfffffffc
#define BCHP_GIO_AON_STAT_EXT_reserved0_SHIFT                      2

/* GIO_AON :: STAT_EXT :: irq_status [01:00] */
#define BCHP_GIO_AON_STAT_EXT_irq_status_MASK                      0x00000003
#define BCHP_GIO_AON_STAT_EXT_irq_status_SHIFT                     0
#define BCHP_GIO_AON_STAT_EXT_irq_status_DEFAULT                   0x00000000

#endif /* #ifndef BCHP_GIO_AON_H__ */

/* End of File */
