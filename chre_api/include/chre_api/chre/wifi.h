/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// IWYU pragma: private, include "chre_api/chre.h"
// IWYU pragma: friend chre/.*\.h

#ifndef _CHRE_WIFI_H_
#define _CHRE_WIFI_H_

/**
 * @file
 * WiFi (IEEE 802.11) API, currently covering scanning features useful for
 * determining location and offloading certain connectivity scans.
 *
 * In this file, specification references use the following shorthand:
 *
 *    Shorthand | Full specification name
 *   ---------- | ------------------------
 *     "802.11" | IEEE Std 802.11-2007
 *     "HT"     | IEEE Std 802.11n-2009
 *     "VHT"    | IEEE Std 802.11ac-2013
 *     "WiFi 6" | IEEE Std 802.11ax draft
 *     "NAN"    | Wi-Fi Neighbor Awareness Networking (NAN) Technical
 *                Specification (v3.2)
 *
 * In the current version of CHRE API, the 6GHz band introduced in WiFi 6 is
 * not supported. A scan request from CHRE should not result in scanning 6GHz
 * channels. In particular, if a 6GHz channel is specified in scanning or
 * ranging request parameter, CHRE should return an error code of
 * CHRE_ERROR_NOT_SUPPORTED. Additionally, CHRE implementations must not include
 * observations of access points on 6GHz channels in scan results, especially
 * those produced due to scan monitoring.
 */

#include "common.h"
#include <chre/common.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The set of flags returned by chreWifiGetCapabilities().
 * @defgroup CHRE_WIFI_CAPABILITIES
 * @{
 */

//! No WiFi APIs are supported
#define CHRE_WIFI_CAPABILITIES_NONE              UINT32_C(0)

//! Listening to scan results is supported, as enabled via
//! chreWifiConfigureScanMonitorAsync()
#define CHRE_WIFI_CAPABILITIES_SCAN_MONITORING   UINT32_C(1 << 0)

//! Requesting WiFi scans on-demand is supported via chreWifiRequestScanAsync()
#define CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN    UINT32_C(1 << 1)

//! Specifying the radio chain preference in on-demand scan requests, and
//! reporting it in scan events is supported
//! @since v1.2
#define CHRE_WIFI_CAPABILITIES_RADIO_CHAIN_PREF  UINT32_C(1 << 2)

//! Requesting RTT ranging is supported via chreWifiRequestRangingAsync()
//! @since v1.2
#define CHRE_WIFI_CAPABILITIES_RTT_RANGING       UINT32_C(1 << 3)

//! Specifies if WiFi NAN service subscription is supported. If a platform
//! supports subscriptions, then it must also support RTT ranging for NAN
//! services via chreWifiNanRequestRangingAsync()
//! @since v1.6
#define CHRE_WIFI_CAPABILITIES_NAN_SUB           UINT32_C(1 << 4)

/** @} */

/**
 * Produce an event ID in the block of IDs reserved for WiFi
 * @param offset  Index into WiFi event ID block; valid range [0,15]
 */
#define CHRE_WIFI_EVENT_ID(offset)  (CHRE_EVENT_WIFI_FIRST_EVENT + (offset))

/**
 * nanoappHandleEvent argument: struct chreAsyncResult
 *
 * Communicates the asynchronous result of a request to the WiFi API. The
 * requestType field in {@link #chreAsyncResult} is set to a value from enum
 * chreWifiRequestType.
 */
#define CHRE_EVENT_WIFI_ASYNC_RESULT  CHRE_WIFI_EVENT_ID(0)

/**
 * nanoappHandleEvent argument: struct chreWifiScanEvent
 *
 * Provides results of a WiFi scan.
 */
#define CHRE_EVENT_WIFI_SCAN_RESULT  CHRE_WIFI_EVENT_ID(1)

/**
 * nanoappHandleEvent argument: struct chreWifiRangingEvent
 *
 * Provides results of an RTT ranging request.
 */
#define CHRE_EVENT_WIFI_RANGING_RESULT  CHRE_WIFI_EVENT_ID(2)

/**
 * nanoappHandleEvent argument: struct chreWifiNanIdentifierEvent
 *
 * Lets the client know if the NAN engine was able to successfully assign
 * an identifier to the subscribe call. The 'cookie' field in the event
 * argument struct can be used to track which subscribe request this identifier
 * maps to.
 */
#define CHRE_EVENT_WIFI_NAN_IDENTIFIER_RESULT   CHRE_WIFI_EVENT_ID(3)

/**
 * nanoappHandleEvent argument: struct chreWifiNanDiscoveryEvent
 *
 * Event that is sent whenever a NAN service matches the criteria specified
 * in a subscription request.
 */
#define CHRE_EVENT_WIFI_NAN_DISCOVERY_RESULT  CHRE_WIFI_EVENT_ID(4)

/**
 * nanoappHandleEvent argument: struct chreWifiNanSessionLostEvent
 *
 * Informs the client that a discovered service is no longer available or
 * visible.
 * The ID of the service on the client that was communicating with the extinct
 * service is indicated by the event argument.
 */
#define CHRE_EVENT_WIFI_NAN_SESSION_LOST  CHRE_WIFI_EVENT_ID(5)

/**
 * nanoappHandleEvent argument: struct chreWifiNanSessionTerminatedEvent
 *
 * Signals the end of a NAN subscription session. The termination can be due to
 * the user turning the WiFi off, or other platform reasons like not being able
 * to support NAN concurrency with the host. The terminated event will have a
 * reason code appropriately populated to denote why the event was sent.
 */
#define CHRE_EVENT_WIFI_NAN_SESSION_TERMINATED  CHRE_WIFI_EVENT_ID(6)

// NOTE: Do not add new events with ID > 15; only values 0-15 are reserved
// (see chre/event.h)

/**
 * The maximum amount of time that is allowed to elapse between a call to
 * chreWifiRequestScanAsync() that returns true, and the associated
 * CHRE_EVENT_WIFI_ASYNC_RESULT used to indicate whether the scan completed
 * successfully or not.
 */
#define CHRE_WIFI_SCAN_RESULT_TIMEOUT_NS  (30 * CHRE_NSEC_PER_SEC)

/**
 * The maximum amount of time that is allowed to elapse between a call to
 * chreWifiRequestRangingAsync() that returns true, and the associated
 * CHRE_EVENT_WIFI_RANGING_RESULT used to indicate whether the ranging operation
 * completed successfully or not.
 */
#define CHRE_WIFI_RANGING_RESULT_TIMEOUT_NS  (30 * CHRE_NSEC_PER_SEC)

/**
 * The current compatibility version of the chreWifiScanEvent structure,
 * including nested structures.
 */
#define CHRE_WIFI_SCAN_EVENT_VERSION  UINT8_C(1)

/**
 * The current compatibility version of the chreWifiRangingEvent structure,
 * including nested structures.
 */
#define CHRE_WIFI_RANGING_EVENT_VERSION  UINT8_C(0)

/**
 * Maximum number of frequencies that can be explicitly specified when
 * requesting a scan
 * @see #chreWifiScanParams
 */
#define CHRE_WIFI_FREQUENCY_LIST_MAX_LEN  (20)

/**
 * Maximum number of SSIDs that can be explicitly specified when requesting a
 * scan
 * @see #chreWifiScanParams
 */
#define CHRE_WIFI_SSID_LIST_MAX_LEN  (20)

/**
 * The maximum number of devices that can be specified in a single RTT ranging
 * request.
 * @see #chreWifiRangingParams
 */
#define CHRE_WIFI_RANGING_LIST_MAX_LEN  (10)

/**
 * The maximum number of octets in an SSID (see 802.11 7.3.2.1)
 */
#define CHRE_WIFI_SSID_MAX_LEN  (32)

/**
 * The number of octets in a BSSID (see 802.11 7.1.3.3.3)
 */
#define CHRE_WIFI_BSSID_LEN  (6)

/**
 * Set of flags which can either indicate a frequency band. Specified as a bit
 * mask to allow for combinations in future API versions.
 * @defgroup CHRE_WIFI_BAND_MASK
 * @{
 */

#define CHRE_WIFI_BAND_MASK_2_4_GHZ  UINT8_C(1 << 0)  //!< 2.4 GHz
#define CHRE_WIFI_BAND_MASK_5_GHZ    UINT8_C(1 << 1)  //!< 5 GHz

/** @} */

/**
 * Characteristics of a scanned device given in struct chreWifiScanResult.flags
 * @defgroup CHRE_WIFI_SCAN_RESULT_FLAGS
 * @{
 */

#define CHRE_WIFI_SCAN_RESULT_FLAGS_NONE                         UINT8_C(0)

//! Element ID 61 (HT Operation) is present (see HT 7.3.2)
#define CHRE_WIFI_SCAN_RESULT_FLAGS_HT_OPS_PRESENT               UINT8_C(1 << 0)

//! Element ID 192 (VHT Operation) is present (see VHT 8.4.2)
#define CHRE_WIFI_SCAN_RESULT_FLAGS_VHT_OPS_PRESENT              UINT8_C(1 << 1)

//! Element ID 127 (Extended Capabilities) is present, and bit 70 (Fine Timing
//! Measurement Responder) is set to 1 (see IEEE Std 802.11-2016 9.4.2.27)
#define CHRE_WIFI_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER             UINT8_C(1 << 2)

//! Retained for backwards compatibility
//! @see CHRE_WIFI_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER
#define CHRE_WIFI_SCAN_RESULT_FLAGS_IS_80211MC_RTT_RESPONDER \
    CHRE_WIFI_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER

//! HT Operation element indicates that a secondary channel is present
//! (see HT 7.3.2.57)
#define CHRE_WIFI_SCAN_RESULT_FLAGS_HAS_SECONDARY_CHANNEL_OFFSET UINT8_C(1 << 3)

//! HT Operation element indicates that the secondary channel is below the
//! primary channel (see HT 7.3.2.57)
#define CHRE_WIFI_SCAN_RESULT_FLAGS_SECONDARY_CHANNEL_OFFSET_IS_BELOW  \
                                                                 UINT8_C(1 << 4)

/** @} */

/**
 * Identifies the authentication methods supported by an AP. Note that not every
 * combination of flags may be possible. Based on WIFI_PNO_AUTH_CODE_* from
 * hardware/libhardware_legacy/include/hardware_legacy/gscan.h in Android.
 * @defgroup CHRE_WIFI_SECURITY_MODE_FLAGS
 * @{
 */

#define CHRE_WIFI_SECURITY_MODE_UNKNOWN  UINT8_C(0)
//! @deprecated since v1.10. Use CHRE_WIFI_SECURITY_MODE_UNKNOWN instead.
#define CHRE_WIFI_SECURITY_MODE_UNKONWN CHRE_WIFI_SECURITY_MODE_UNKNOWN

#define CHRE_WIFI_SECURITY_MODE_OPEN  UINT8_C(1 << 0)  //!< No auth/security
#define CHRE_WIFI_SECURITY_MODE_WEP   UINT8_C(1 << 1)
#define CHRE_WIFI_SECURITY_MODE_PSK   UINT8_C(1 << 2)  //!< WPA-PSK or WPA2-PSK
#define CHRE_WIFI_SECURITY_MODE_EAP   UINT8_C(1 << 3)  //!< WPA-EAP or WPA2-EAP

//! @since v1.5
#define CHRE_WIFI_SECURITY_MODE_SAE   UINT8_C(1 << 4)

//! @since v1.5
#define CHRE_WIFI_SECURITY_MODE_EAP_SUITE_B  UINT8_C(1 << 5)

//! @since v1.5
#define CHRE_WIFI_SECURITY_MODE_OWE   UINT8_C(1 << 6)

/** @} */

/**
 * Identifies which radio chain was used to discover an AP. The underlying
 * hardware does not necessarily support more than one radio chain.
 * @defgroup CHRE_WIFI_RADIO_CHAIN_FLAGS
 * @{
 */

#define CHRE_WIFI_RADIO_CHAIN_UNKNOWN  UINT8_C(0)
#define CHRE_WIFI_RADIO_CHAIN_0        UINT8_C(1 << 0)
#define CHRE_WIFI_RADIO_CHAIN_1        UINT8_C(1 << 1)

/** @} */

//! Special value indicating that an LCI uncertainty fields is not provided
//! Ref: RFC 6225
#define CHRE_WIFI_LCI_UNCERTAINTY_UNKNOWN  UINT8_C(0)

/**
 * Defines the flags that may be returned in
 * {@link #chreWifiRangingResult.flags}. Undefined bits are reserved for future
 * use and must be ignored by nanoapps.
 * @defgroup CHRE_WIFI_RTT_RESULT_FLAGS
 * @{
 */

//! If set, the nested chreWifiLci structure is populated; otherwise it is
//! invalid and must be ignored
#define CHRE_WIFI_RTT_RESULT_HAS_LCI  UINT8_C(1 << 0)

/** @} */

/**
 * Identifies a WiFi frequency band
 */
enum chreWifiBand {
    CHRE_WIFI_BAND_2_4_GHZ = CHRE_WIFI_BAND_MASK_2_4_GHZ,
    CHRE_WIFI_BAND_5_GHZ   = CHRE_WIFI_BAND_MASK_5_GHZ,
};

/**
 * Indicates the BSS operating channel width determined from the VHT and/or HT
 * Operation elements. Refer to VHT 8.4.2.161 and HT 7.3.2.57.
 */
enum chreWifiChannelWidth {
    CHRE_WIFI_CHANNEL_WIDTH_20_MHZ         = 0,
    CHRE_WIFI_CHANNEL_WIDTH_40_MHZ         = 1,
    CHRE_WIFI_CHANNEL_WIDTH_80_MHZ         = 2,
    CHRE_WIFI_CHANNEL_WIDTH_160_MHZ        = 3,
    CHRE_WIFI_CHANNEL_WIDTH_80_PLUS_80_MHZ = 4,
};

/**
 * Indicates the type of scan requested or performed
 */
enum chreWifiScanType {
    //! Perform a purely active scan using probe requests. Do not scan channels
    //! restricted to use via Dynamic Frequency Selection (DFS) only.
    CHRE_WIFI_SCAN_TYPE_ACTIVE = 0,

    //! Perform an active scan on unrestricted channels, and also perform a
    //! passive scan on channels that are restricted to use via Dynamic
    //! Frequency Selection (DFS), e.g. the U-NII bands 5250-5350MHz and
    //! 5470-5725MHz in the USA as mandated by FCC regulation.
    CHRE_WIFI_SCAN_TYPE_ACTIVE_PLUS_PASSIVE_DFS = 1,

    //! Perform a passive scan, only listening for beacons.
    CHRE_WIFI_SCAN_TYPE_PASSIVE = 2,

    //! Client has no preference for a particular scan type.
    //! Only valid in a {@link #chreWifiScanParams}.
    //!
    //! On a v1.4 or earlier platform, this will fall back to
    //! CHRE_WIFI_SCAN_TYPE_ACTIVE if {@link #chreWifiScanParams.channelSet} is
    //! set to CHRE_WIFI_CHANNEL_SET_NON_DFS, and to
    //! CHRE_WIFI_SCAN_TYPE_ACTIVE_PLUS_PASSIVE_DFS otherwise.
    //!
    //! If CHRE_WIFI_CAPABILITIES_RADIO_CHAIN_PREF is supported, a v1.5 or
    //! later platform shall perform a type of scan optimized for {@link
    //! #chreWifiScanParams.radioChainPref}.
    //!
    //! Clients are strongly encouraged to set this value in {@link
    //! #chreWifiScanParams.scanType} and instead express their preferences
    //! through {@link #chreWifiRadioChainPref} and {@link #chreWifiChannelSet}
    //! so the platform can best optimize power and performance.
    //!
    //! @since v1.5
    CHRE_WIFI_SCAN_TYPE_NO_PREFERENCE = 3,
};

/**
 * Indicates whether RTT ranging with a specific device succeeded
 */
enum chreWifiRangingStatus {
    //! Ranging completed successfully
    CHRE_WIFI_RANGING_STATUS_SUCCESS = 0,

    //! Ranging failed due to an unspecified error
    CHRE_WIFI_RANGING_STATUS_ERROR   = 1,
};

/**
 * Possible values for {@link #chreWifiLci.altitudeType}. Ref: RFC 6225 2.4
 */
enum chreWifiLciAltitudeType {
    CHRE_WIFI_LCI_ALTITUDE_TYPE_UNKNOWN = 0,
    CHRE_WIFI_LCI_ALTITUDE_TYPE_METERS  = 1,
    CHRE_WIFI_LCI_ALTITUDE_TYPE_FLOORS  = 2,
};

/**
 * Indicates a type of request made in this API. Used to populate the resultType
 * field of struct chreAsyncResult sent with CHRE_EVENT_WIFI_ASYNC_RESULT.
 */
enum chreWifiRequestType {
    CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR = 1,
    CHRE_WIFI_REQUEST_TYPE_REQUEST_SCAN           = 2,
    CHRE_WIFI_REQUEST_TYPE_RANGING                = 3,
    CHRE_WIFI_REQUEST_TYPE_NAN_SUBSCRIBE          = 4,
};

/**
 * Allows a nanoapp to express its preference for how multiple available
 * radio chains should be used when performing an on-demand scan. This is only a
 * preference from the nanoapp and is not guaranteed to be honored by the WiFi
 * firmware.
 */
enum chreWifiRadioChainPref {
    //! No preference for radio chain usage
    CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT = 0,

    //! In a scan result, indicates that the radio chain preference used for the
    //! scan is not known
    CHRE_WIFI_RADIO_CHAIN_PREF_UNKNOWN = CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT,

    //! Prefer to use available radio chains in a way that minimizes time to
    //! complete the scan
    CHRE_WIFI_RADIO_CHAIN_PREF_LOW_LATENCY = 1,

    //! Prefer to use available radio chains in a way that minimizes total power
    //! consumed for the scan
    CHRE_WIFI_RADIO_CHAIN_PREF_LOW_POWER = 2,

    //! Prefer to use available radio chains in a way that maximizes accuracy of
    //! the scan result, e.g. RSSI measurements
    CHRE_WIFI_RADIO_CHAIN_PREF_HIGH_ACCURACY = 3,
};

/**
 * WiFi NAN subscription type.
 */
enum chreWifiNanSubscribeType {
    //! In the active mode, explicit transmission of a subscribe message is
    //! requested, and publish messages are processed.
    CHRE_WIFI_NAN_SUBSCRIBE_TYPE_ACTIVE = 0,

    //! In the passive mode, no transmission of a subscribe message is
    //! requested, but received publish messages are checked for matches.
    CHRE_WIFI_NAN_SUBSCRIBE_TYPE_PASSIVE = 1,
};

/**
 * Indicates the reason for a subscribe session termination.
 */
enum chreWifiNanTerminatedReason {
    CHRE_WIFI_NAN_TERMINATED_BY_USER_REQUEST = 0,
    CHRE_WIFI_NAN_TERMINATED_BY_TIMEOUT = 1,
    CHRE_WIFI_NAN_TERMINATED_BY_FAILURE = 2,
};

/**
 * SSID with an explicit length field, used when an array of SSIDs is supplied.
 */
struct chreWifiSsidListItem {
    //! Number of valid bytes in ssid. Valid range [0, CHRE_WIFI_SSID_MAX_LEN]
    uint8_t ssidLen;

    //! Service Set Identifier (SSID)
    uint8_t ssid[CHRE_WIFI_SSID_MAX_LEN];
};

/**
 * Indicates the set of channels to be scanned.
 *
 * @since v1.5
 */
enum chreWifiChannelSet {
    //! The set of channels that allows active scan using probe request.
    CHRE_WIFI_CHANNEL_SET_NON_DFS = 0,

    //! The set of all channels supported.
    CHRE_WIFI_CHANNEL_SET_ALL = 1,
};

/**
 * Data structure passed to chreWifiRequestScanAsync
 */
struct chreWifiScanParams {
    //! Set to a value from @ref enum chreWifiScanType
    uint8_t scanType;

    //! Indicates whether the client is willing to tolerate receiving cached
    //! results of a previous scan, and if so, the maximum age of the scan that
    //! the client will accept. "Age" in this case is defined as the elapsed
    //! time between when the most recent scan was completed and the request is
    //! received, in milliseconds. If set to 0, no cached results may be
    //! provided, and all scan results must come from a "fresh" WiFi scan, i.e.
    //! one that completes strictly after this request is received. If more than
    //! one scan is cached and meets this age threshold, only the newest scan is
    //! provided.
    uint32_t maxScanAgeMs;

    //! If set to 0, scan all frequencies. Otherwise, this indicates the number
    //! of frequencies to scan, as specified in the frequencyList array. Valid
    //! range [0, CHRE_WIFI_FREQUENCY_LIST_MAX_LEN].
    uint16_t frequencyListLen;

    //! Pointer to an array of frequencies to scan, given as channel center
    //! frequencies in MHz. This field may be NULL if frequencyListLen is 0.
    const uint32_t *frequencyList;

    //! If set to 0, do not restrict scan to any SSIDs. Otherwise, this
    //! indicates the number of SSIDs in the ssidList array to be used for
    //! directed probe requests. Not applicable and ignore when scanType is
    //! CHRE_WIFI_SCAN_TYPE_PASSIVE.
    uint8_t ssidListLen;

    //! Pointer to an array of SSIDs to use for directed probe requests. May be
    //! NULL if ssidListLen is 0.
    const struct chreWifiSsidListItem *ssidList;

    //! Set to a value from enum chreWifiRadioChainPref to specify the desired
    //! trade-off between power consumption, accuracy, etc. If
    //! chreWifiGetCapabilities() does not have the applicable bit set, this
    //! parameter is ignored.
    //! @since v1.2
    uint8_t radioChainPref;

    //! Set to a value from enum chreWifiChannelSet to specify the set of
    //! channels to be scanned. This field is considered by the platform only
    //! if scanType is CHRE_WIFI_SCAN_TYPE_NO_PREFERENCE and frequencyListLen
    //! is equal to zero.
    //!
    //! @since v1.5
    uint8_t channelSet;
};

/**
 * Provides information about a single access point (AP) detected in a scan.
 */
struct chreWifiScanResult {
    //! Number of milliseconds prior to referenceTime in the enclosing
    //! chreWifiScanEvent struct when the probe response or beacon frame that
    //! was used to populate this structure was received.
    uint32_t ageMs;

    //! Capability Information field sent by the AP (see 802.11 7.3.1.4). This
    //! field must reflect native byte order and bit ordering, such that
    //! (capabilityInfo & 1) gives the bit for the ESS subfield.
    uint16_t capabilityInfo;

    //! Number of valid bytes in ssid. Valid range [0, CHRE_WIFI_SSID_MAX_LEN]
    uint8_t ssidLen;

    //! Service Set Identifier (SSID), a series of 0 to 32 octets identifying
    //! the access point. Note that this is commonly a human-readable ASCII
    //! string, but this is not the required encoding per the standard.
    uint8_t ssid[CHRE_WIFI_SSID_MAX_LEN];

    //! Basic Service Set Identifier (BSSID), represented in big-endian byte
    //! order, such that the first octet of the OUI is accessed in byte index 0.
    uint8_t bssid[CHRE_WIFI_BSSID_LEN];

    //! A set of flags from CHRE_WIFI_SCAN_RESULT_FLAGS_*
    uint8_t flags;

    //! RSSI (Received Signal Strength Indicator), in dBm. Typically negative.
    //! If multiple radio chains were used to scan this AP, this is a "best
    //! available" measure that may be a composite of measurements taken across
    //! the radio chains.
    int8_t  rssi;

    //! Operating band, set to a value from enum chreWifiBand
    uint8_t band;

    /**
     * Indicates the center frequency of the primary 20MHz channel, given in
     * MHz. This value is derived from the channel number via the formula:
     *
     *     primaryChannel (MHz) = CSF + 5 * primaryChannelNumber
     *
     * Where CSF is the channel starting frequency (in MHz) given by the
     * operating class/band (i.e. 2407 or 5000), and primaryChannelNumber is the
     * channel number in the range [1, 200].
     *
     * Refer to VHT 22.3.14.
     */
    uint32_t primaryChannel;

    /**
     * If the channel width is 20 MHz, this field is not relevant and set to 0.
     * If the channel width is 40, 80, or 160 MHz, then this denotes the channel
     * center frequency (in MHz). If the channel is 80+80 MHz, then this denotes
     * the center frequency of segment 0, which contains the primary channel.
     * This value is derived from the frequency index using the same formula as
     * for primaryChannel.
     *
     * Refer to VHT 8.4.2.161, and VHT 22.3.14.
     *
     * @see #primaryChannel
     */
    uint32_t centerFreqPrimary;

    /**
     * If the channel width is 80+80MHz, then this denotes the center frequency
     * of segment 1, which does not contain the primary channel. Otherwise, this
     * field is not relevant and set to 0.
     *
     * @see #centerFreqPrimary
     */
    uint32_t centerFreqSecondary;

    //! @see #chreWifiChannelWidth
    uint8_t channelWidth;

    //! Flags from CHRE_WIFI_SECURITY_MODE_* indicating supported authentication
    //! and associated security modes
    //! @see CHRE_WIFI_SECURITY_MODE_FLAGS
    uint8_t securityMode;

    //! Identifies the radio chain(s) used to discover this AP
    //! @see CHRE_WIFI_RADIO_CHAIN_FLAGS
    //! @since v1.2
    uint8_t radioChain;

    //! If the CHRE_WIFI_RADIO_CHAIN_0 bit is set in radioChain, gives the RSSI
    //! measured on radio chain 0 in dBm; otherwise invalid and set to 0. This
    //! field, along with its relative rssiChain1, can be used to determine RSSI
    //! measurements from each radio chain when multiple chains were used to
    //! discover this AP.
    //! @see #radioChain
    //! @since v1.2
    int8_t rssiChain0;
    int8_t rssiChain1;  //!< @see #rssiChain0

    //! Reserved; set to 0
    uint8_t reserved[7];
};

/**
 * Data structure sent with events of type CHRE_EVENT_WIFI_SCAN_RESULT.
 */
struct chreWifiScanEvent {
    //! Indicates the version of the structure, for compatibility purposes.
    //! Clients do not normally need to worry about this field; the CHRE
    //! implementation guarantees that the client only receives the structure
    //! version it expects.
    uint8_t version;

    //! The number of entries in the results array in this event. The CHRE
    //! implementation may split scan results across multiple events for memory
    //! concerns, etc.
    uint8_t resultCount;

    //! The total number of results returned by the scan. Allows an event
    //! consumer to identify when it has received all events associated with a
    //! scan.
    uint8_t resultTotal;

    //! Sequence number for this event within the series of events comprising a
    //! complete scan result. Scan events are delivered strictly in order, i.e.
    //! this is monotonically increasing for the results of a single scan. Valid
    //! range [0, <number of events for scan> - 1]. The number of events for a
    //! scan is typically given by
    //! ceil(resultTotal / <max results per event supported by platform>).
    uint8_t eventIndex;

    //! A value from enum chreWifiScanType indicating the type of scan performed
    uint8_t scanType;

    //! If a directed scan was performed to a limited set of SSIDs, then this
    //! identifies the number of unique SSIDs included in the probe requests.
    //! Otherwise, this is set to 0, indicating that the scan was not limited by
    //! SSID. Note that if this is non-zero, the list of SSIDs used is not
    //! included in the scan event.
    uint8_t ssidSetSize;

    //! If 0, indicates that all frequencies applicable for the scanType were
    //! scanned. Otherwise, indicates the number of frequencies scanned, as
    //! specified in scannedFreqList.
    uint16_t scannedFreqListLen;

    //! Timestamp when the scan was completed, from the same time base as
    //! chreGetTime() (in nanoseconds)
    uint64_t referenceTime;

    //! Pointer to an array containing scannedFreqListLen values comprising the
    //! set of frequencies that were scanned. Frequencies are specified as
    //! channel center frequencies in MHz. May be NULL if scannedFreqListLen is
    //! 0.
    const uint32_t *scannedFreqList;

    //! Pointer to an array containing resultCount entries. May be NULL if
    //! resultCount is 0.
    const struct chreWifiScanResult *results;

    //! Set to a value from enum chreWifiRadioChainPref indicating the radio
    //! chain preference used for the scan. If the applicable bit is not set in
    //! chreWifiGetCapabilities(), this will always be set to
    //! CHRE_WIFI_RADIO_CHAIN_PREF_UNKNOWN.
    //! @since v1.2
    uint8_t radioChainPref;
};

/**
 * Identifies a device to perform RTT ranging against. These values are normally
 * populated based on the contents of a scan result.
 * @see #chreWifiScanResult
 * @see chreWifiRangingTargetFromScanResult()
 */
struct chreWifiRangingTarget {
    //! Device MAC address, specified in the same byte order as
    //! {@link #chreWifiScanResult.bssid}
    uint8_t macAddress[CHRE_WIFI_BSSID_LEN];

    //! Center frequency of the primary 20MHz channel, in MHz
    //! @see #chreWifiScanResult.primaryChannel
    uint32_t primaryChannel;

    //! Channel center frequency, in MHz, or 0 if not relevant
    //! @see #chreWifiScanResult.centerFreqPrimary
    uint32_t centerFreqPrimary;

    //! Channel center frequency of segment 1 if channel width is 80+80MHz,
    //! otherwise 0
    //! @see #chreWifiScanResult.centerFreqSecondary
    uint32_t centerFreqSecondary;

    //! @see #chreWifiChannelWidth
    uint8_t channelWidth;

    //! Reserved for future use and ignored by CHRE
    uint8_t reserved[3];
};

/**
 * Parameters for an RTT ("Fine Timing Measurement" in terms of 802.11-2016)
 * ranging request, supplied to chreWifiRequestRangingAsync().
 */
struct chreWifiRangingParams {
    //! Number of devices to perform ranging against and the length of
    //! targetList, in range [1, CHRE_WIFI_RANGING_LIST_MAX_LEN].
    uint8_t targetListLen;

    //! Array of macAddressListLen MAC addresses (e.g. BSSIDs) with which to
    //! attempt RTT ranging.
    const struct chreWifiRangingTarget *targetList;
};

/**
 * Provides the result of RTT ranging with a single device.
 */
struct chreWifiRangingResult {
    //! Time when the ranging operation on this device was performed, in the
    //! same time base as chreGetTime() (in nanoseconds)
    uint64_t timestamp;

    //! MAC address of the device for which ranging was requested
    uint8_t macAddress[CHRE_WIFI_BSSID_LEN];

    //! Gives the result of ranging to this device. If not set to
    //! CHRE_WIFI_RANGING_STATUS_SUCCESS, the ranging attempt to this device
    //! failed, and other fields in this structure may be invalid.
    //! @see #chreWifiRangingStatus
    uint8_t status;

    //! The mean RSSI measured during the RTT burst, in dBm. Typically negative.
    //! If status is not CHRE_WIFI_RANGING_STATUS_SUCCESS, will be set to 0.
    int8_t rssi;

    //! Estimated distance to the device with the given BSSID, in millimeters.
    //! Generally the mean of multiple measurements performed in a single burst.
    //! If status is not CHRE_WIFI_RANGING_STATUS_SUCCESS, will be set to 0.
    uint32_t distance;

    //! Standard deviation of estimated distance across multiple measurements
    //! performed in a single RTT burst, in millimeters. If status is not
    //! CHRE_WIFI_RANGING_STATUS_SUCCESS, will be set to 0.
    uint32_t distanceStdDev;

    //! Location Configuration Information (LCI) information optionally returned
    //! during the ranging procedure. Only valid if {@link #flags} has the
    //! CHRE_WIFI_RTT_RESULT_HAS_LCI bit set. Refer to IEEE 802.11-2016
    //! 9.4.2.22.10, 11.24.6.7, and RFC 6225 (July 2011) for more information.
    //! Coordinates are to be interpreted according to the WGS84 datum.
    struct chreWifiLci {
        //! Latitude in degrees as 2's complement fixed-point with 25 fractional
        //! bits, i.e. degrees * 2^25. Ref: RFC 6225 2.3
        int64_t latitude;

        //! Longitude, same format as {@link #latitude}
        int64_t longitude;

        //! Altitude represented as a 2's complement fixed-point value with 8
        //! fractional bits. Interpretation depends on {@link #altitudeType}. If
        //! UNKNOWN, this field must be ignored. If *METERS, distance relative
        //! to the zero point in the vertical datum. If *FLOORS, a floor value
        //! relative to the ground floor, potentially fractional, e.g. to
        //! indicate mezzanine levels. Ref: RFC 6225 2.4
        int32_t altitude;

        //! Maximum extent of latitude uncertainty in degrees, decoded via this
        //! formula: 2 ^ (8 - x) where "x" is the encoded value passed in this
        //! field. Unknown if set to CHRE_WIFI_LCI_UNCERTAINTY_UNKNOWN.
        //! Ref: RFC 6225 2.3.2
        uint8_t latitudeUncertainty;

        //! @see #latitudeUncertainty
        uint8_t longitudeUncertainty;

        //! Defines how to interpret altitude, set to a value from enum
        //! chreWifiLciAltitudeType
        uint8_t altitudeType;

        //! Uncertainty in altitude, decoded via this formula: 2 ^ (21 - x)
        //! where "x" is the encoded value passed in this field. Unknown if set
        //! to CHRE_WIFI_LCI_UNCERTAINTY_UNKNOWN. Only applies when altitudeType
        //! is CHRE_WIFI_LCI_ALTITUDE_TYPE_METERS. Ref: RFC 6225 2.4.5
        uint8_t altitudeUncertainty;
    } lci;

    //! Refer to CHRE_WIFI_RTT_RESULT_FLAGS
    uint8_t flags;

    //! Reserved; set to 0
    uint8_t reserved[7];
};

/**
 * Data structure sent with events of type CHRE_EVENT_WIFI_RANGING_RESULT.
 */
struct chreWifiRangingEvent {
    //! Indicates the version of the structure, for compatibility purposes.
    //! Clients do not normally need to worry about this field; the CHRE
    //! implementation guarantees that the client only receives the structure
    //! version it expects.
    uint8_t version;

    //! The number of ranging results included in the results array; matches the
    //! number of MAC addresses specified in the request
    uint8_t resultCount;

    //! Reserved; set to 0
    uint8_t reserved[2];

    //! Pointer to an array containing resultCount entries
    const struct chreWifiRangingResult *results;
};

/**
 * Indicates the WiFi NAN capabilities of the device. Must contain non-zero
 * values if WiFi NAN is supported.
 */
struct chreWifiNanCapabilities {
    //! Maximum length of the match filter arrays (applies to both tx and rx
    //! match filters).
    uint32_t maxMatchFilterLength;

    //! Maximum length of the service specific information byte array.
    uint32_t maxServiceSpecificInfoLength;

    //! Maximum length of the service name. Includes the NULL terminator.
    uint8_t maxServiceNameLength;

    //! Reserved for future use.
    uint8_t reserved[3];
};

/**
 * Data structure sent with events of type
 * CHRE_EVENT_WIFI_NAN_IDENTIFIER_RESULT
 */
struct chreWifiNanIdentifierEvent {
    //! A unique ID assigned by the NAN engine for the subscribe request
    //! associated with the cookie encapsulated in the async result below. The
    //! ID is set to 0 if there was a request failure in which case the async
    //! result below contains the appropriate error code indicating the failure
    //! reason.
    uint32_t id;

    //! Structure which contains the cookie associated with the publish/
    //! subscribe request, along with an error code that indicates request
    //! success or failure.
    struct chreAsyncResult result;
};

/**
 * Indicates the desired configuration for a WiFi NAN ranging request.
 */
struct chreWifiNanRangingParams {
    //! MAC address of the NAN device for which range is to be determined.
    uint8_t macAddress[CHRE_WIFI_BSSID_LEN];
};

/**
 * Configuration parameters specific to the Subscribe Function (Spec 4.1.1.1)
 */
struct chreWifiNanSubscribeConfig {
    //! Indicates the subscribe type, set to a value from @ref
    //! chreWifiNanSubscribeType.
    uint8_t subscribeType;

    //! UTF-8 name string that identifies the service/application. Must be NULL
    //! terminated. Note that the string length cannot be greater than the
    //! maximum length specified by @ref chreWifiNanCapabilities. No
    //! restriction is placed on the string case, since the service name
    //! matching is expected to be case insensitive.
    const char *service;

    //! An array of bytes (and the associated array length) of service-specific
    //! information. Note that the array length must be less than the
    //! maxServiceSpecificInfoLength parameter obtained from the NAN
    //! capabilities (@see struct chreWifiNanCapabilities).
    const uint8_t *serviceSpecificInfo;
    uint32_t serviceSpecificInfoSize;

    //! Ordered sequence of {length | value} pairs that specify match criteria
    //! beyond the service name. 'length' uses 1 byte, and its value indicates
    //! the number of bytes of the match criteria that follow. The length of
    //! the match filter array should not exceed the maximum match filter
    //! length obtained from @ref chreWifiNanGetCapabilities. When a service
    //! publish message discovery frame containing the Service ID being
    //! subscribed to is received, the matching is done as follows:
    //! Each {length | value} pair in the kth position (1 <= k <= #length-value
    //! pairs) is compared against the kth {length | value} pair in the
    //! matching filter field of the publish message.
    //! - For a kth position {length | value} pair in the rx match filter with
    //!   a length of 0, a match is declared regardless of the tx match filter
    //!   contents.
    //! - For a kth position {length | value} pair in the rx match with a non-
    //!   zero length, there must be an exact match with the kth position pair
    //!    in the match filter field of the received service descriptor for a
    //!    match to be found.
    //! Please refer to Appendix H of the NAN spec for examples on matching.
    //! The match filter length should not exceed the maxMatchFilterLength
    //! obtained from @ref chreWifiNanCapabilities.
    const uint8_t *matchFilter;
    uint32_t matchFilterLength;
};

/**
 * Data structure sent with events of type
 * CHRE_EVENT_WIFI_NAN_DISCOVERY_RESULT.
 */
struct chreWifiNanDiscoveryEvent {
    //! Identifier of the subscribe function instance that requested a
    //! discovery.
    uint32_t subscribeId;

    //! Identifier of the publisher on the remote NAN device.
    uint32_t publishId;

    //! NAN interface address of the publisher
    uint8_t publisherAddress[CHRE_WIFI_BSSID_LEN];

    //! An array of bytes (and the associated array length) of service-specific
    //! information. Note that the array length must be less than the
    //! maxServiceSpecificInfoLength parameter obtained from the NAN
    //! capabilities (@see struct chreWifiNanCapabilities).
    const uint8_t *serviceSpecificInfo;
    uint32_t serviceSpecificInfoSize;
};

/**
 * Data structure sent with events of type CHRE_EVENT_WIFI_NAN_SESSION_LOST.
 */
struct chreWifiNanSessionLostEvent {
    //! The original ID (returned by the NAN discovery engine) of the subscriber
    //! instance.
    uint32_t id;

    //! The ID of the previously discovered publisher on a peer NAN device that
    //! is no longer connected.
    uint32_t peerId;
};

/**
 * Data structure sent with events of type
 * CHRE_EVENT_WIFI_NAN_SESSION_TERMINATED.
 */
struct chreWifiNanSessionTerminatedEvent {
    //! The original ID (returned by the NAN discovery engine) of the subscriber
    //! instance that was terminated.
    uint32_t id;

    //! A value that maps to one of the termination reasons in @ref enum
    //! chreWifiNanTerminatedReason.
    uint8_t reason;

    //! Reserved for future use.
    uint8_t reserved[3];
};

/**
 * Retrieves a set of flags indicating the WiFi features supported by the
 * current CHRE implementation. The value returned by this function must be
 * consistent for the entire duration of the Nanoapp's execution.
 *
 * The client must allow for more flags to be set in this response than it knows
 * about, for example if the implementation supports a newer version of the API
 * than the client was compiled against.
 *
 * @return A bitmask with zero or more CHRE_WIFI_CAPABILITIES_* flags set
 *
 * @since v1.1
 */
uint32_t chreWifiGetCapabilities(void);

/**
 * Retrieves device-specific WiFi NAN capabilities, and populates them in
 * the @ref chreWifiNanCapabilities structure.
 *
 * @param capabilities Structure into which the WiFi NAN capabilities of
 *        the device are populated into. Must not be NULL.
 * @return true if WiFi NAN is supported, false otherwise.
 *
 * @since v1.6
 */
bool chreWifiNanGetCapabilities(struct chreWifiNanCapabilities *capabilities);

/**
 * Nanoapps must define CHRE_NANOAPP_USES_WIFI somewhere in their build
 * system (e.g. Makefile) if the nanoapp needs to use the following WiFi APIs.
 * In addition to allowing access to these APIs, defining this macro will also
 * ensure CHRE enforces that all host clients this nanoapp talks to have the
 * required Android permissions needed to listen to WiFi data by adding metadata
 * to the nanoapp.
 */
#if defined(CHRE_NANOAPP_USES_WIFI) || !defined(CHRE_IS_NANOAPP_BUILD)

/**
 * Manages a client's request to receive the results of WiFi scans performed for
 * other purposes, for example scans done to maintain connectivity and scans
 * requested by other clients. The presence of this request has no effect on the
 * frequency or configuration of the WiFi scans performed - it is purely a
 * registration by the client to receive the results of scans that would
 * otherwise occur normally. This should include all available scan results,
 * including those that are not normally sent to the applications processor,
 * such as Preferred Network Offload (PNO) scans. Scan results provided because
 * of this registration must not contain cached results - they are always
 * expected to contain the fresh results from a recent scan.
 *
 * An active scan monitor subscription must persist across temporary conditions
 * under which no WiFi scans will be performed, for example if WiFi is
 * completely disabled via user-controlled settings, or if the WiFi system
 * restarts independently of CHRE. Likewise, a request to enable a scan monitor
 * subscription must succeed under normal conditions, even in circumstances
 * where no WiFi scans will be performed. In these cases, the scan monitor
 * implementation must produce scan results once the temporary condition is
 * cleared, for example after WiFi is enabled by the user.
 *
 * These scan results are delivered to the Nanoapp's handle event callback using
 * CHRE_EVENT_WIFI_SCAN_RESULT.
 *
 * An active scan monitor subscription is not necessary to receive the results
 * of an on-demand scan request sent via chreWifiRequestScanAsync(), and it does
 * not result in duplicate delivery of scan results generated from
 * chreWifiRequestScanAsync().
 *
 * If no monitor subscription is active at the time of a request with
 * enable=false, it is treated as if an active subscription was successfully
 * ended.
 *
 * The result of this request is delivered asynchronously via an event of type
 * CHRE_EVENT_WIFI_ASYNC_RESULT. Refer to the note in {@link #chreAsyncResult}
 * for more details.
 *
 * @param enable Set to true to enable monitoring scan results, false to
 *        disable
 * @param cookie An opaque value that will be included in the chreAsyncResult
 *        sent in relation to this request.
 * @return true if the request was accepted for processing, false otherwise
 *
 * @since v1.1
 * @note Requires WiFi permission
 */
bool chreWifiConfigureScanMonitorAsync(bool enable, const void *cookie);

/**
 * Sends an on-demand request for WiFi scan results. This may trigger a new
 * scan, or be entirely serviced from cache, depending on the maxScanAgeMs
 * parameter.
 *
 * This resulting status of this request is delivered asynchronously via an
 * event of type CHRE_EVENT_WIFI_ASYNC_RESULT. The result must be delivered
 * within CHRE_WIFI_SCAN_RESULT_TIMEOUT_NS of the this request. Refer to the
 * note in {@link #chreAsyncResult} for more details.
 *
 * A successful result provided in CHRE_EVENT_WIFI_ASYNC_RESULT indicates that
 * the scan results are ready to be delivered in a subsequent event (or events,
 * which arrive consecutively without any other scan results in between)
 * of type CHRE_EVENT_WIFI_SCAN_RESULT.
 *
 * WiFi scanning must be disabled if both "WiFi scanning" and "WiFi" settings
 * are disabled at the Android level. In this case, the CHRE implementation is
 * expected to return a result with CHRE_ERROR_FUNCTION_DISABLED.
 *
 * It is not valid for a client to request a new scan while a result is pending
 * based on a previous scan request from the same client. In this situation, the
 * CHRE implementation is expected to return a result with CHRE_ERROR_BUSY.
 * However, if a scan is currently pending or in progress due to a request from
 * another client, whether within the CHRE or otherwise, the implementation must
 * not fail the request for this reason. If the pending scan satisfies the
 * client's request parameters, then the implementation should use its results
 * to satisfy the request rather than scheduling a new scan.
 *
 * @param params A set of parameters for the scan request. Must not be NULL.
 * @param cookie An opaque value that will be included in the chreAsyncResult
 *        sent in relation to this request.
 * @return true if the request was accepted for processing, false otherwise
 *
 * @since v1.1
 * @note Requires WiFi permission
 */
bool chreWifiRequestScanAsync(const struct chreWifiScanParams *params,
                              const void *cookie);

/**
 * Convenience function which calls chreWifiRequestScanAsync() with a default
 * set of scan parameters.
 *
 * @param cookie An opaque value that will be included in the chreAsyncResult
 *        sent in relation to this request.
 * @return true if the request was accepted for processing, false otherwise
 *
 * @since v1.1
 * @note Requires WiFi permission
 */
static inline bool chreWifiRequestScanAsyncDefault(const void *cookie) {
    static const struct chreWifiScanParams params = {
        /*.scanType=*/         CHRE_WIFI_SCAN_TYPE_NO_PREFERENCE,
        /*.maxScanAgeMs=*/     5000,  // 5 seconds
        /*.frequencyListLen=*/ 0,
        /*.frequencyList=*/    NULL,
        /*.ssidListLen=*/      0,
        /*.ssidList=*/         NULL,
        /*.radioChainPref=*/   CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT,
        /*.channelSet=*/       CHRE_WIFI_CHANNEL_SET_NON_DFS
    };
    return chreWifiRequestScanAsync(&params, cookie);
}

/**
 * Issues a request to initiate distance measurements using round-trip time
 * (RTT), aka Fine Timing Measurement (FTM), to one or more devices identified
 * by MAC address. Within CHRE, MACs are typically the BSSIDs of scanned APs
 * that have the CHRE_WIFI_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER flag set.
 *
 * This resulting status of this request is delivered asynchronously via an
 * event of type CHRE_EVENT_WIFI_ASYNC_RESULT. The result must be delivered
 * within CHRE_WIFI_RANGING_RESULT_TIMEOUT_NS of the this request. Refer to the
 * note in {@link #chreAsyncResult} for more details.
 *
 * WiFi RTT ranging must be disabled if any of the following is true:
 * - Both "WiFi" and "WiFi Scanning" settings are disabled at the Android level.
 * - The "Location" setting is disabled at the Android level.
 * In this case, the CHRE implementation is expected to return a result with
 * CHRE_ERROR_FUNCTION_DISABLED.
 *
 * A successful result provided in CHRE_EVENT_WIFI_ASYNC_RESULT indicates that
 * the results of ranging will be delivered in a subsequent event of type
 * CHRE_EVENT_WIFI_RANGING_RESULT. Note that the CHRE_EVENT_WIFI_ASYNC_RESULT
 * gives an overall status - for example, it is used to indicate failure if the
 * entire ranging request was rejected because WiFi is disabled. However, it is
 * valid for this event to indicate success, but RTT ranging to fail for all
 * requested devices - for example, they may be out of range. Therefore, it is
 * also necessary to check the status field in {@link #chreWifiRangingResult}.
 *
 * @param params Structure containing the parameters of the scan request,
 *        including the list of devices to attempt ranging.
 * @param cookie An opaque value that will be included in the chreAsyncResult
 *        sent in relation to this request.
 * @return true if the request was accepted for processing, false otherwise
 *
 * @since v1.2
 * @note Requires WiFi permission
 */
bool chreWifiRequestRangingAsync(const struct chreWifiRangingParams *params,
                                 const void *cookie);

/**
 * Helper function to populate an instance of struct chreWifiRangingTarget with
 * the contents of a scan result provided in struct chreWifiScanResult.
 * Populates other parameters that are not directly derived from the scan result
 * with default values.
 *
 * @param scanResult The scan result to parse as input
 * @param rangingTarget The RTT ranging target to populate as output
 *
 * @note Requires WiFi permission
 */
static inline void chreWifiRangingTargetFromScanResult(
        const struct chreWifiScanResult *scanResult,
        struct chreWifiRangingTarget *rangingTarget) {
    memcpy(rangingTarget->macAddress, scanResult->bssid,
           sizeof(rangingTarget->macAddress));
    rangingTarget->primaryChannel      = scanResult->primaryChannel;
    rangingTarget->centerFreqPrimary   = scanResult->centerFreqPrimary;
    rangingTarget->centerFreqSecondary = scanResult->centerFreqSecondary;
    rangingTarget->channelWidth        = scanResult->channelWidth;

    // Note that this is not strictly necessary (CHRE can see which API version
    // the nanoapp was built against, so it knows to ignore these fields), but
    // we do it here to keep things nice and tidy
    memset(rangingTarget->reserved, 0, sizeof(rangingTarget->reserved));
}

/**
 * Subscribe to a NAN service.
 *
 * Sends a subscription request to the NAN discovery engine with the
 * specified configuration parameters. If successful, a unique non-zero
 * subscription ID associated with this instance of the subscription
 * request is assigned by the NAN discovery engine. The subscription request
 * is active until explicitly canceled, or if the connection was interrupted.
 *
 * Note that CHRE forwards any discovery events that it receives to the
 * subscribe function instance, and does no duplicate filtering. If
 * multiple events of the same discovery are undesirable, it is up to the
 * platform NAN discovery engine implementation to implement redundancy
 * detection mechanisms.
 *
 * If WiFi is turned off by the user at the Android level, an existing
 * subscribe session is canceled, and a CHRE_EVENT_WIFI_ASYNC_RESULT event is
 * event is sent to the subscriber. Nanoapps are expected to register for user
 * settings notifications (@see chreUserSettingConfigureEvents), and
 * re-establish a subscribe session on a WiFi re-enabled settings changed
 * notification.
 *
 * @param config Service subscription configuration
 * @param cookie A value that the nanoapp uses to track this particular
 *        subscription request.
 * @return true if NAN is enabled and a subscription request was successfully
 *         made to the NAN engine. The actual result of the service discovery
 *         is sent via a CHRE_EVENT_WIFI_NAN_DISCOVERY_RESULT event.
 *
 * @since v1.6
 * @note Requires WiFi permission
 */
bool chreWifiNanSubscribe(struct chreWifiNanSubscribeConfig *config,
                          const void *cookie);

/**
 * Cancel a subscribe function instance.
 *
 * @param subscriptionId The ID that was originally assigned to this instance
 *        of the subscribe function.
 * @return true if NAN is enabled, the subscribe ID  was found and the instance
 *         successfully canceled.
 *
 * @since v1.6
 * @note Requires WiFi permission
 */
bool chreWifiNanSubscribeCancel(uint32_t subscriptionID);

/**
 * Request RTT ranging from a peer NAN device.
 *
 * Nanoapps can use this API to explicitly request measurement reports from
 * the peer device. Note that both end points have to support ranging for a
 * successful request. The MAC address of the peer NAN device for which ranging
 * is desired may be obtained either from a NAN service discovery or from an
 * out-of-band source (HAL service, BLE, etc.).
 *
 * If WiFi is turned off by the user at the Android level, an existing
 * ranging session is canceled, and a CHRE_EVENT_WIFI_ASYNC_RESULT event is
 * sent to the subscriber. Nanoapps are expected to register for user settings
 * notifications (@see chreUserSettingConfigureEvents), and perform another
 * ranging request on a WiFi re-enabled settings changed notification.
 *
 * A successful result provided in CHRE_EVENT_WIFI_ASYNC_RESULT indicates that
 * the results of ranging will be delivered in a subsequent event of type
 * CHRE_EVENT_WIFI_RANGING_RESULT.
 *
 * @param params Structure containing the parameters of the ranging request,
 *        including the MAC address of the peer NAN device to attempt ranging.
 * @param cookie An opaque value that will be included in the chreAsyncResult
 *        sent in relation to this request.
 * @return true if the request was accepted for processing, false otherwise.
 */
bool chreWifiNanRequestRangingAsync(const struct chreWifiNanRangingParams *params,
                                    const void *cookie);

#else  /* defined(CHRE_NANOAPP_USES_WIFI) || !defined(CHRE_IS_NANOAPP_BUILD) */
#define CHRE_WIFI_PERM_ERROR_STRING \
    "CHRE_NANOAPP_USES_WIFI must be defined when building this nanoapp in " \
    "order to refer to "
#define chreWifiConfigureScanMonitorAsync(...) \
    CHRE_BUILD_ERROR(CHRE_WIFI_PERM_ERROR_STRING \
                     "chreWifiConfigureScanMonitorAsync")
#define chreWifiRequestScanAsync(...) \
    CHRE_BUILD_ERROR(CHRE_WIFI_PERM_ERROR_STRING \
                     "chreWifiRequestScanAsync")
#define chreWifiRequestScanAsyncDefault(...) \
    CHRE_BUILD_ERROR(CHRE_WIFI_PERM_ERROR_STRING \
                     "chreWifiRequestScanAsyncDefault")
#define chreWifiRequestRangingAsync(...) \
    CHRE_BUILD_ERROR(CHRE_WIFI_PERM_ERROR_STRING "chreWifiRequestRangingAsync")
#define chreWifiRangingTargetFromScanResult(...) \
    CHRE_BUILD_ERROR(CHRE_WIFI_PERM_ERROR_STRING \
                     "chreWifiRangingTargetFromScanResult")
#define chreWifiNanSubscribe(...) \
    CHRE_BUILD_ERROR(CHRE_WIFI_PERM_ERROR_STRING "chreWifiNanSubscribe")
#define chreWifiNanSubscribeCancel(...) \
    CHRE_BUILD_ERROR(CHRE_WIFI_PERM_ERROR_STRING "chreWifiNanSubscribeCancel")
#define chreWifiNanRequestRangingAsync(...) \
    CHRE_BUILD_ERROR(CHRE_WIFI_PERM_ERROR_STRING "chreWifiNanRequestRangingAsync")
#endif  /* defined(CHRE_NANOAPP_USES_WIFI) || !defined(CHRE_IS_NANOAPP_BUILD) */

#ifdef __cplusplus
}
#endif

#endif  /* _CHRE_WIFI_H_ */
