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

extern const String VERSION;
extern const String BUILD_DATE;

extern const PROGMEM char CN_active[];
extern const PROGMEM char CN_ActiveHigh[];
extern const PROGMEM char CN_activedelay[];
extern const PROGMEM char CN_activevalue[];
extern const PROGMEM char CN_ActiveLow[];
extern const PROGMEM char CN_addr[];
extern const PROGMEM char CN_advancedView[];
extern const PROGMEM char CN_allleds [];
extern const PROGMEM char CN_ap_fallback [];
extern const PROGMEM char CN_ap_timeout [];
extern const PROGMEM char CN_ap_reboot [];
extern const PROGMEM char CN_appendnullcount [];
extern const PROGMEM char CN_b[];
extern const PROGMEM char CN_b16[];
extern const PROGMEM char CN_baudrate[];
extern const PROGMEM char CN_blanktime[];
extern const PROGMEM char CN_bridge[];
extern const PROGMEM char CN_brightness[];
extern const PROGMEM char CN_cfgver[];
extern const PROGMEM char CN_channels[];
extern const PROGMEM char CN_clean[];
extern const PROGMEM char CN_clock_pin[];
extern const PROGMEM char CN_cmd[];
extern const PROGMEM char CN_color[];
extern const PROGMEM char CN_color_order[];
extern const PROGMEM char CN_Configuration_File_colon [];
extern const PROGMEM char CN_config[];
extern const PROGMEM char CN_connected[];
extern const PROGMEM char CN_count[];
extern const PROGMEM char CN_currenteffect[];
extern const PROGMEM char CN_cs_pin[];
extern const PROGMEM char CN_currentlimit[];
extern const PROGMEM char CN_current_sequence[];
extern const PROGMEM char CN_data_pin[];
extern const PROGMEM char CN_device [];
extern const PROGMEM char CN_dhcp[];
extern const PROGMEM char CN_duration[];
extern const PROGMEM char CN_effect[];
extern const PROGMEM char CN_effect_list[];
extern const PROGMEM char CN_EffectAllLeds[];
extern const PROGMEM char CN_EffectBrightness[];
extern const PROGMEM char CN_EffectColor[];
extern const PROGMEM char CN_EffectMirror[];
extern const PROGMEM char CN_EffectReverse[];
extern const PROGMEM char CN_EffectSpeed[];
extern const PROGMEM char CN_EffectWhiteChannel[];
extern const PROGMEM char CN_Effect[];
extern const PROGMEM char CN_effects[];
extern const PROGMEM char CN_en[];
extern const PROGMEM char CN_enabled[];
extern const PROGMEM char CN_errors[];
extern const PROGMEM char CN_ESP32[];
extern const PROGMEM char CN_ESP8266[];
extern const PROGMEM char CN_ESPixelStick [];
extern const PROGMEM char CN_eth[];
extern const PROGMEM char CN_EthDrv[];
extern const PROGMEM char CN_false [];
extern const PROGMEM char CN_File[];
extern const PROGMEM char CN_file[];
extern const PROGMEM char CN_filename[];
extern const PROGMEM char CN_files[];
extern const PROGMEM char CN_Frequency[];
extern const PROGMEM char CN_fseqfilename[];
extern const PROGMEM char CN_gateway[];
extern const PROGMEM char CN_g[];
extern const PROGMEM char CN_gamma[];
extern const PROGMEM char CN_get[];
extern const PROGMEM char CN_gen_ser_hdr[];
extern const PROGMEM char CN_gen_ser_ftr[];
extern const PROGMEM char CN_gid[];
extern const PROGMEM char CN_group_size[];
extern const PROGMEM char CN_hadisco[];
extern const PROGMEM char CN_haprefix[];
extern const PROGMEM char CN_Heap_colon [];
extern const PROGMEM char CN_HostName [];
extern const PROGMEM char CN_hostname [];
extern const PROGMEM char CN_id[];
extern const PROGMEM char CN_Idle[];
extern const PROGMEM char CN_init[];
extern const PROGMEM char CN_interframetime[];
extern const PROGMEM char CN_inv[];
extern const PROGMEM char CN_ip[];
extern const PROGMEM char CN_input[];
extern const PROGMEM char CN_input_config[];
extern const PROGMEM char CN_last_clientIP[];
extern const PROGMEM char CN_lwt[];
extern const PROGMEM char CN_mac[];
extern const PROGMEM char CN_mdc_pin[];
extern const PROGMEM char CN_mdio_pin[];
extern const PROGMEM char CN_Max[];
extern const PROGMEM char CN_Min[];
extern const PROGMEM char CN_minussigns[];
extern const PROGMEM char CN_mirror [];
extern const PROGMEM char CN_miso_pin[];
extern const PROGMEM char CN_mode [];
extern const PROGMEM char CN_mode_name [];
extern const PROGMEM char CN_mosi_pin[];
extern const PROGMEM char CN_multicast [];
extern const PROGMEM char CN_name [];
extern const PROGMEM char CN_NeedAutoConfig [];
extern const PROGMEM char CN_netmask [];
extern const PROGMEM char CN_network [];
extern const PROGMEM char CN_num_chan[];
extern const PROGMEM char CN_num_packets[];
extern const PROGMEM char CN_output[];
extern const PROGMEM char CN_output_config[];
extern const PROGMEM char CN_packet_errors[];
extern const PROGMEM char CN_passphrase[];
extern const PROGMEM char CN_password[];
extern const PROGMEM char CN_Paused[];
extern const PROGMEM char CN_pixel_count[];
extern const PROGMEM char CN_polarity[];
extern const PROGMEM char CN_port[];
extern const PROGMEM char CN_Platform[];
extern const PROGMEM char CN_play[];
extern const PROGMEM char CN_playFseq[];
extern const PROGMEM char CN_playlist [];
extern const PROGMEM char CN_plussigns [];
extern const PROGMEM char CN_power_pin[];
extern const PROGMEM char CN_prependnullcount [];
extern const PROGMEM char CN_pwm [];
extern const PROGMEM char CN_remote[];
extern const PROGMEM char CN_r[];
extern const PROGMEM char CN_rev[];
extern const PROGMEM char CN_reverse[];
extern const PROGMEM char CN_rssi[];
extern const PROGMEM char CN_sca[];
extern const PROGMEM char CN_seconds_elapsed[];
extern const PROGMEM char CN_seconds_played[];
extern const PROGMEM char CN_seconds_remaining[];
extern const PROGMEM char CN_sequence_filename[];
extern const PROGMEM char CN_slashset[];
extern const PROGMEM char CN_slashstatus[];
extern const PROGMEM char CN_speed[];
extern const PROGMEM char CN_ssid [];
extern const PROGMEM char CN_sta_timeout [];
extern const PROGMEM char CN_stars[];
extern const PROGMEM char CN_state[];
extern const PROGMEM char CN_status [];
extern const PROGMEM char CN_status_name[];
extern const PROGMEM char CN_subnet[];
extern const PROGMEM char CN_SyncOffset[];
extern const PROGMEM char CN_system[];
extern const PROGMEM char CN_textSLASHplain[];
extern const PROGMEM char CN_time[];
extern const PROGMEM char CN_time_elapsed[];
extern const PROGMEM char CN_TimeRemaining[];
extern const PROGMEM char CN_time_remaining[];
extern const PROGMEM char CN_topic[];
extern const PROGMEM char CN_topicset[];
extern const PROGMEM char CN_trig[];
extern const PROGMEM char CN_true[];
extern const PROGMEM char CN_type[];
extern const PROGMEM char CN_ui[];
extern const PROGMEM char CN_unichanlim[];
extern const PROGMEM char CN_unifirst[];
extern const PROGMEM char CN_unilast[];
extern const PROGMEM char CN_universe[];
extern const PROGMEM char CN_universe_limit [];
extern const PROGMEM char CN_universe_start[];
extern const PROGMEM char CN_updateinterval[];
extern const PROGMEM char CN_user[];
extern const PROGMEM char CN_version[];
extern const PROGMEM char CN_Version[];
extern const PROGMEM char CN_weus[];
extern const PROGMEM char CN_wifi[];
extern const PROGMEM char CN_WiFiDrv[];
extern const PROGMEM char CN_XP[];
extern const PROGMEM char CN_zig_size[];
