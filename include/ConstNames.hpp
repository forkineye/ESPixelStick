#pragma once
/*
* ConstNames.hpp - List of strings that can be reused
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#ifdef ARDUINO_ARCH_ESP8266
    // putting const strings in PROGMEM does not work on the ESP8266
#   define CN_PROGMEM
#else // !ARDUINO_ARCH_ESP8266
#   define CN_PROGMEM PROGMEM
#endif // !def ARDUINO_ARCH_ESP8266

extern const String VERSION;
extern const String BUILD_DATE;

extern const CN_PROGMEM char CN_active [];
extern const CN_PROGMEM char CN_ActiveHigh [];
extern const CN_PROGMEM char CN_activedelay [];
extern const CN_PROGMEM char CN_activevalue [];
extern const CN_PROGMEM char CN_ActiveLow [];
extern const CN_PROGMEM char CN_addr [];
extern const CN_PROGMEM char CN_advancedView [];
extern const CN_PROGMEM char CN_allleds [];
extern const CN_PROGMEM char CN_ap_channel [];
extern const CN_PROGMEM char CN_ap_fallback [];
extern const CN_PROGMEM char CN_ap_passphrase [];
extern const CN_PROGMEM char CN_ap_ssid [];
extern const CN_PROGMEM char CN_ap_timeout [];
extern const CN_PROGMEM char CN_ap_reboot [];
extern const CN_PROGMEM char CN_appendnullcount [];
extern const CN_PROGMEM char CN_applicationSLASHjson [];
extern const CN_PROGMEM char CN_b [];
extern const CN_PROGMEM char CN_b16 [];
extern const CN_PROGMEM char CN_baudrate [];
extern const CN_PROGMEM char CN_BlankOnStop [];
extern const CN_PROGMEM char CN_blanktime [];
extern const CN_PROGMEM char CN_bridge [];
extern const CN_PROGMEM char CN_brightness [];
extern const CN_PROGMEM char CN_brightnessEnd [];
extern const CN_PROGMEM char CN_cfgver [];
extern const CN_PROGMEM char CN_channels [];
extern const CN_PROGMEM char CN_clean [];
extern const CN_PROGMEM char CN_clock_pin [];
extern const CN_PROGMEM char CN_cmd [];
extern const CN_PROGMEM char CN_color [];
extern const CN_PROGMEM char CN_color_order [];
extern const CN_PROGMEM char CN_Configuration_File_colon [];
extern const CN_PROGMEM char CN_config [];
extern const CN_PROGMEM char CN_connected [];
extern const CN_PROGMEM char CN_count [];
extern const CN_PROGMEM char CN_currenteffect [];
extern const CN_PROGMEM char CN_cs_pin [];
extern const CN_PROGMEM char CN_currentlimit [];
extern const CN_PROGMEM char CN_current_sequence [];
extern const CN_PROGMEM char CN_data_pin [];
extern const CN_PROGMEM char CN_device [];
extern const CN_PROGMEM char CN_dhcp [];
extern const CN_PROGMEM char CN_Default [];
extern const CN_PROGMEM char CN_Disabled [];
extern const CN_PROGMEM char CN_dnsp [];
extern const CN_PROGMEM char CN_dnss [];
extern const CN_PROGMEM char CN_Dotfseq [];
extern const CN_PROGMEM char CN_Dotjson [];
extern const CN_PROGMEM char CN_Dotpl [];
extern const CN_PROGMEM char CN_DMX [];
extern const CN_PROGMEM char CN_duration [];
extern const CN_PROGMEM char CN_effect [];
extern const CN_PROGMEM char CN_effect_list [];
extern const CN_PROGMEM char CN_EffectAllLeds [];
extern const CN_PROGMEM char CN_EffectBrightness [];
extern const CN_PROGMEM char CN_EffectColor [];
extern const CN_PROGMEM char CN_EffectMirror [];
extern const CN_PROGMEM char CN_EffectReverse [];
extern const CN_PROGMEM char CN_EffectSpeed [];
extern const CN_PROGMEM char CN_EffectWhiteChannel [];
extern const CN_PROGMEM char CN_Effect [];
extern const CN_PROGMEM char CN_effects [];
extern const CN_PROGMEM char CN_en [];
extern const CN_PROGMEM char CN_enabled [];
extern const CN_PROGMEM char CN_entry [];
extern const CN_PROGMEM char CN_errors [];
extern const CN_PROGMEM char CN_ESP32 [];
extern const CN_PROGMEM char CN_ESP8266 [];
extern const CN_PROGMEM char CN_ESPixelStick [];
extern const CN_PROGMEM char CN_eth [];
extern const CN_PROGMEM char CN_EthDrv [];
extern const CN_PROGMEM char CN_false [];
extern const CN_PROGMEM char CN_File [];
extern const CN_PROGMEM char CN_file [];
extern const CN_PROGMEM char CN_filename [];
extern const CN_PROGMEM char CN_files [];
extern const CN_PROGMEM char CN_FPPoverride [];
extern const CN_PROGMEM char CN_Frequency [];
extern const CN_PROGMEM char CN_fseqfilename [];
extern const CN_PROGMEM char CN_gateway [];
extern const CN_PROGMEM char CN_g [];
extern const CN_PROGMEM char CN_gamma [];
extern const CN_PROGMEM char CN_GECE [];
extern const CN_PROGMEM char CN_get [];
extern const CN_PROGMEM char CN_gen_ser_hdr [];
extern const CN_PROGMEM char CN_gen_ser_ftr [];
extern const CN_PROGMEM char CN_gid [];
extern const CN_PROGMEM char CN_group_size [];
extern const CN_PROGMEM char CN_GS8208 [];
extern const CN_PROGMEM char CN_hadisco [];
extern const CN_PROGMEM char CN_haprefix [];
extern const CN_PROGMEM char CN_Heap_colon [];
extern const CN_PROGMEM char CN_HostName [];
extern const CN_PROGMEM char CN_hostname [];
extern const CN_PROGMEM char CN_hv [];
extern const CN_PROGMEM char CN_id [];
extern const CN_PROGMEM char CN_Idle [];
extern const CN_PROGMEM char CN_init [];
extern const CN_PROGMEM char CN_interframetime [];
extern const CN_PROGMEM char CN_inv [];
extern const CN_PROGMEM char CN_ip [];
extern const CN_PROGMEM char CN_input [];
extern const CN_PROGMEM char CN_input_config [];
extern const CN_PROGMEM char CN_last_clientIP [];
extern const CN_PROGMEM char CN_long [];
extern const CN_PROGMEM char CN_lwt [];
extern const CN_PROGMEM char CN_mac [];
extern const CN_PROGMEM char CN_MarqueeGroups [];
extern const CN_PROGMEM char CN_mdc_pin [];
extern const CN_PROGMEM char CN_mdio_pin [];
extern const CN_PROGMEM char CN_Max [];
extern const CN_PROGMEM char CN_MaxChannels [];
extern const CN_PROGMEM char CN_Min [];
extern const CN_PROGMEM char CN_minussigns [];
extern const CN_PROGMEM char CN_mirror [];
extern const CN_PROGMEM char CN_miso_pin [];
extern const CN_PROGMEM char CN_mode [];
extern const CN_PROGMEM char CN_mode_name [];
extern const CN_PROGMEM char CN_mosi_pin [];
extern const CN_PROGMEM char CN_multicast [];
extern const CN_PROGMEM char CN_name [];
extern const CN_PROGMEM char CN_NeedAutoConfig [];
extern const CN_PROGMEM char CN_netmask [];
extern const CN_PROGMEM char CN_network [];
extern const CN_PROGMEM char CN_No_LocalFileToPlay [];
extern const CN_PROGMEM char CN_num_chan [];
extern const CN_PROGMEM char CN_num_packets [];
extern const CN_PROGMEM char CN_off [];
extern const CN_PROGMEM char CN_on [];
extern const CN_PROGMEM char CN_output [];
extern const CN_PROGMEM char CN_output_config [];
extern const CN_PROGMEM char CN_OutputSpi [];
extern const CN_PROGMEM char CN_OutputUart [];
extern const CN_PROGMEM char CN_packet_errors [];
extern const CN_PROGMEM char CN_passphrase [];
extern const CN_PROGMEM char CN_password [];
extern const CN_PROGMEM char CN_Paused [];
extern const CN_PROGMEM char CN_pixel_count [];
extern const CN_PROGMEM char CN_Platform [];
extern const CN_PROGMEM char CN_play [];
extern const CN_PROGMEM char CN_playcount [];
extern const CN_PROGMEM char CN_playFseq [];
extern const CN_PROGMEM char CN_playlist [];
extern const CN_PROGMEM char CN_plussigns [];
extern const CN_PROGMEM char CN_polarity [];
extern const CN_PROGMEM char CN_PollCounter [];
extern const CN_PROGMEM char CN_port [];
extern const CN_PROGMEM char CN_power_pin [];
extern const CN_PROGMEM char CN_prependnullcount [];
extern const CN_PROGMEM char CN_pwm [];
extern const CN_PROGMEM char CN_reading [];
extern const CN_PROGMEM char CN_Relay [];
extern const CN_PROGMEM char CN_remote [];
extern const CN_PROGMEM char CN_Renard [];
extern const CN_PROGMEM char CN_r [];
extern const CN_PROGMEM char CN_rev [];
extern const CN_PROGMEM char CN_reverse [];
extern const CN_PROGMEM char CN_RMT [];
extern const CN_PROGMEM char CN_rssi [];
extern const CN_PROGMEM char CN_sca [];
extern const CN_PROGMEM char CN_sdspeed [];
extern const CN_PROGMEM char CN_seconds_elapsed [];
extern const CN_PROGMEM char CN_seconds_played [];
extern const CN_PROGMEM char CN_seconds_remaining [];
extern const CN_PROGMEM char CN_SendFppSync [];
extern const CN_PROGMEM char CN_sensor [];
extern const CN_PROGMEM char CN_sequence_filename [];
extern const CN_PROGMEM char CN_Serial [];
extern const CN_PROGMEM char CN_Servo_PCA9685 [];
extern const CN_PROGMEM char CN_slashset [];
extern const CN_PROGMEM char CN_slashstatus [];
extern const CN_PROGMEM char CN_speed [];
extern const CN_PROGMEM char CN_ssid [];
extern const CN_PROGMEM char CN_sta_timeout [];
extern const CN_PROGMEM char CN_stars [];
extern const CN_PROGMEM char CN_state [];
extern const CN_PROGMEM char CN_status [];
extern const CN_PROGMEM char CN_status_name [];
extern const CN_PROGMEM char CN_StayInApMode [];
extern const CN_PROGMEM char CN_subnet [];
extern const CN_PROGMEM char CN_SyncOffset [];
extern const CN_PROGMEM char CN_system [];
extern const CN_PROGMEM char CN_textSLASHplain [];
extern const CN_PROGMEM char CN_time [];
extern const CN_PROGMEM char CN_time_elapsed [];
extern const CN_PROGMEM char CN_TimeRemaining [];
extern const CN_PROGMEM char CN_time_remaining [];
extern const CN_PROGMEM char CN_TLS3001 [];
extern const CN_PROGMEM char CN_TM1814 [];
extern const CN_PROGMEM char CN_tsensortopic [];
extern const CN_PROGMEM char CN_topic [];
extern const CN_PROGMEM char CN_topicset [];
extern const CN_PROGMEM char CN_transitions [];
extern const CN_PROGMEM char CN_trig [];
extern const CN_PROGMEM char CN_true [];
extern const CN_PROGMEM char CN_type [];
extern const CN_PROGMEM char CN_UCS1903 [];
extern const CN_PROGMEM char CN_UCS8903 [];
extern const CN_PROGMEM char CN_ui [];
extern const CN_PROGMEM char CN_unichanlim [];
extern const CN_PROGMEM char CN_unifirst [];
extern const CN_PROGMEM char CN_unilast [];
extern const CN_PROGMEM char CN_units [];
extern const CN_PROGMEM char CN_universe [];
extern const CN_PROGMEM char CN_universe_limit [];
extern const CN_PROGMEM char CN_universe_start [];
extern const CN_PROGMEM char CN_updateinterval [];
extern const CN_PROGMEM char CN_user [];
extern const CN_PROGMEM char CN_version [];
extern const CN_PROGMEM char CN_Version [];
extern const CN_PROGMEM char CN_weus [];
extern const CN_PROGMEM char CN_wifi [];
extern const CN_PROGMEM char CN_WiFiDrv [];
extern const CN_PROGMEM char CN_WS2801 [];
extern const CN_PROGMEM char CN_WS2811 [];
extern const CN_PROGMEM char CN_XP [];
extern const CN_PROGMEM char CN_zig_size [];

extern const CN_PROGMEM char MN_01 [];
extern const CN_PROGMEM char MN_02 [];
extern const CN_PROGMEM char MN_03 [];
extern const CN_PROGMEM char MN_04 [];
extern const CN_PROGMEM char MN_05 [];
extern const CN_PROGMEM char MN_06 [];
extern const CN_PROGMEM char MN_07 [];
extern const CN_PROGMEM char MN_08 [];
extern const CN_PROGMEM char MN_09 [];
extern const CN_PROGMEM char MN_10 [];
extern const CN_PROGMEM char MN_11 [];
extern const CN_PROGMEM char MN_12 [];
extern const CN_PROGMEM char MN_13 [];
extern const CN_PROGMEM char MN_14 [];
extern const CN_PROGMEM char MN_15 [];
extern const CN_PROGMEM char MN_16 [];
extern const CN_PROGMEM char MN_17 [];
extern const CN_PROGMEM char MN_18 [];
extern const CN_PROGMEM char MN_19 [];
extern const CN_PROGMEM char MN_20 [];
extern const CN_PROGMEM char MN_21 [];
extern const CN_PROGMEM char MN_22 [];
extern const CN_PROGMEM char MN_2 [];
