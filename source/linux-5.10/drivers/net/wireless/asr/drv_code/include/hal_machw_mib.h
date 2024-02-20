/**
 ****************************************************************************************
 *
 * @file hal_machw_mib.h
 *
 * @brief MACHW MIB structure
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _HAL_MIB_H_
#define _HAL_MIB_H_

/**
 *****************************************************************************************
 * @defgroup MACHW_MIB MACHW_MIB
 * @ingroup HAL
 * @brief MACHW MIB structure
 * @{
 *****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "co_wf_int.h"

/*
 * DEFINES
 ****************************************************************************************
 */
/// Simulated MIB structure
struct machw_mib_tag
{
    /// MIB element to count the number of unencrypted frames that have been discarded
    uint32_t dot11_wep_excluded_count;//0
    /// MIB element to count the receive FCS errors
    uint32_t dot11_fcs_error_count;
    /**
      * MIB element to count the number of PHY Errors reported during a receive
      * transaction.
      */
    uint32_t nx_rx_phy_error_count;
    /// MIB element to count the number of times the receive FIFO has overflowed
    uint32_t nx_rd_fifo_overflow_count;
    /**
      * MIB element to count the number of times underrun has occured on the
      * transmit side
      */
    uint32_t nx_tx_underun_count;
    /**
      * MIB element to count the number of times overflow has occured on the
      * mpif size
      */
    uint32_t nx_rx_mpif_overflow_count;
    /// Reserved
    uint32_t reserved_1[6];//11
    /// MIB element to count unicast transmitted MPDU
    uint32_t nx_qos_utransmitted_mpdu_count[8];
    /// MIB element to count group addressed transmitted MPDU
    uint32_t nx_qos_gtransmitted_mpdu_count[8];
    /**
      * MIB element to count the number of MSDUs or MMPDUs discarded because of
      * retry-limit reached
      */
    uint32_t dot11_qos_failed_count[8];
    /**
      * MIB element to count number of unfragmented MSDU's or MMPDU's transmitted
      * successfully after 1 or more transmission
      */
    uint32_t dot11_qos_retry_count[8];
    /// MIB element to count number of successful RTS Frame transmission
    uint32_t dot11_qos_rts_success_count[8];
    /// MIB element to count number of unsuccessful RTS Frame transmission
    uint32_t dot11_qos_rts_failure_count[8];
    /// MIB element to count number of MPDU's not received ACK
    uint32_t nx_qos_ack_failure_count[8];
    /// MIB element to count number of unicast MPDU's received successfully
    uint32_t nx_qos_ureceived_mpdu_count[8];
    /// MIB element to count number of group addressed MPDU's received successfully
    uint32_t nx_qos_greceived_mpdu_count[8];
    /**
      * MIB element to count the number of unicast MPDUs not destined to this device
      * received successfully.
      */
    uint32_t nx_qos_ureceived_other_mpdu[8];
    /// MIB element to count the number of MPDUs received with retry bit set
    uint32_t dot11_qos_retries_received_count[8]; //99
    /**
      * MIB element to count the number of unicast A-MSDUs that were transmitted
      * successfully
      */
    uint32_t nx_utransmitted_amsdu_count[8];
    /**
      * MIB element to count the number of group-addressed A-MSDUs that were
      * transmitted successfully
      */
    uint32_t nx_gtransmitted_amsdu_count[8];
    /// MIB element to count number of AMSDU's discarded because of retry limit reached
    uint32_t dot11_failed_amsdu_count[8];
    /// MIB element to count number of A-MSDU's transmitted successfully with retry
    uint32_t dot11_retry_amsdu_count[8];
    /**
      * MIB element to count number of bytes of an A-MSDU that was
      * transmitted successfully
      */
    uint32_t dot11_transmitted_octets_in_amsdu[8];
    /**
      * MIB element to counts the number of A-MSDUs that did not receive an ACK frame
      * successfully in response
      */
    uint32_t dot11_amsdu_ack_failure_count[8];
    /// MIB element to count number of unicast A-MSDUs received successfully
    uint32_t nx_ureceived_amsdu_count[8];
    /// MIB element to count number of group addressed A-MSDUs received successfully
    uint32_t nx_greceived_amsdu_count[8];
    /**
      * MIB element to count number of unicast A-MSDUs not destined to this device
      * received successfully
      */
    uint32_t nx_ureceived_other_amsdu[8];
    /// MIB element to count number of bytes in an A-MSDU is received
    uint32_t dot11_received_octets_in_amsdu_count[8]; //179
    /// Reserved
    uint32_t reserved_2[24]; // trigger based mib set //203
    /// MIB element to count number of A-MPDUs transmitted successfully
    uint32_t dot11_transmitted_ampdu_count;
    /// MIB element to count number of MPDUs transmitted in an A-MPDU
    uint32_t dot11_transmitted_mpdus_in_ampdu_count ;
    /// MIB element to count the number of bytes in a transmitted A-MPDU
    uint32_t dot11_transmitted_octets_in_ampdu_count ;
    /// MIB element to count number of unicast A-MPDU's received
    uint32_t wnlu_ampdu_received_count;
    /// MIB element to count number of group addressed A-MPDU's received
    uint32_t nx_gampdu_received_count;
    /**
      * MIB element to count number of unicast A-MPDUs received not destined
      * to this device
      */
    uint32_t nx_other_ampdu_received_count ;
    /// MIB element to count number of MPDUs received in an A-MPDU
    uint32_t dot11_mpdu_in_received_ampdu_count;
    /// MIB element to count number of bytes received in an A-MPDU
    uint32_t dot11_received_octets_in_ampdu_count;
    /// MIB element to count number of CRC errors in MPDU delimeter of and A-MPDU
    uint32_t dot11_ampdu_delimiter_crc_error_count;
    /**
      * MIB element to count number of implicit BAR frames that did not received
      * BA frame successfully in response
      */
    uint32_t dot11_implicit_bar_failure_count;
    /**
      * MIB element to count number of explicit BAR frames that did not received
      * BA frame successfully in response
      */
    uint32_t dot11_explicit_bar_failure_count; //214
    /// Reserved
    uint32_t reserved_3[5];
    /// MIB element to count the number of frames transmitted at 20 MHz BW
    uint32_t dot11_20mhz_frame_transmitted_count; //220
    /// MIB element to count the number of frames transmitted at 40 MHz BW
    uint32_t dot11_40mhz_frame_transmitted_count;
    /// MIB element to count the number of frames transmitted at 80 MHz BW
    uint32_t dot11_80mhz_frame_transmitted_count;
    /// MIB element to count the number of frames transmitted at 160 MHz BW
    uint32_t dot11_160mhz_frame_transmitted_count;
    /// MIB element to count the number of frames received at 20 MHz BW
    uint32_t dot11_20mhz_frame_received_count;
    /// MIB element to count the number of frames received at 40 MHz BW
    uint32_t dot11_40mhz_frame_received_count;
    /// MIB element to count the number of frames received at 80 MHz BW
    uint32_t dot11_80mhz_frame_received_count;
    /// MIB element to count the number of frames received at 160 MHz BW
    uint32_t dot11_160mhz_frame_received_count;
    /// MIB element to count the number of attempts made to acquire a 20 MHz TXOP
    uint32_t nx_failed_20mhz_txop;
    /// MIB element to count the number of successful 20M TXOPs
    uint32_t nx_successful_20mhz_txops;
    /// MIB element to count the number of attempts made to acquire a 40 MHz TXOP
    uint32_t nx_failed_40mhz_txop;
    /// MIB element to count the number of successful 40M TXOPs
    uint32_t nx_successful_40mhz_txops;
    /// MIB element to count the number of attempts made to acquire a 80 MHz TXOP
    uint32_t nx_failed_80mhz_txop;
    /// MIB element to count the number of successful 80M TXOPs
    uint32_t nx_successful_80mhz_txops;
    /// MIB element to count the number of attempts made to acquire a 160 MHz TXOP
    uint32_t nx_failed_160mhz_txop;
    /// MIB element to count the number of successful 160M TXOPs
    uint32_t nx_successful_160mhz_txops;//235
    /// MIB element to count the number of dyn bw drop
    uint32_t nx_dyn_bwdrop_count;
    /// MIB element to count the number of sta bw failed
    uint32_t nx_sta_bwfailed_count;
    /// Reserved
    uint32_t reserved_4[2];
    /// MIB element to count the number of times the dual CTS fails
    //uint32_t dot11_dualcts_success_count;
    /**
      * MIB element to count the number of times the AP does not detect a collision
      * PIFS after transmitting a STBC CTS frame
      */
    uint32_t dot11_stbc_cts_success_count;
    /**
      * MIB element to count the number of times the AP detects a collision PIFS after
      * transmitting a STBC CTS frame
      */
    uint32_t dot11_stbc_cts_failure_count;
    /**
      * MIB element to count the number of times the AP does not detect a collision PIFS
      * after transmitting a non-STBC CTS frame
      */
    uint32_t dot11_non_stbc_cts_success_count;
    /**
      * MIB element to count the number of times the AP detects a collision PIFS after
      * transmitting a non-STBC CTS frame
      */
    uint32_t dot11_non_stbc_cts_failure_count;
    /**
      * reserved
      */
    uint32_t reserved_5[4]; //247

    //
    uint32_t dot11_beamforming_frame_count;
    //
    uint32_t nx_beamforming_received_frame_count;
    //
    uint32_t nx_su_bfr_trasmitted_count;
    //
    uint32_t nx_mu_bfr_transmitted_count;
    //
    uint32_t nx_bfr_received_count;
    //
    uint32_t reserved_6[1];//253

    //
    uint32_t nx_mu_received_frame_count;//254
    //
    //uint32_t reserved_7[1];//255

};

/// @}
#endif //_HAL_MIB_H_
