/*
* ConstNames.hpp - List of strings that can be reused
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2021, 2022 Shelby Merrick
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

#include "ESPixelStick.h"

const CN_PROGMEM char CN_active                   [] = "active";
const CN_PROGMEM char CN_ActiveHigh               [] = "ActiveHigh";
const CN_PROGMEM char CN_ActiveLow                [] = "ActiveLow";
const CN_PROGMEM char CN_activedelay              [] = "activedelay";
const CN_PROGMEM char CN_activevalue              [] = "activevalue";
const CN_PROGMEM char CN_addr                     [] = "addr";
const CN_PROGMEM char CN_advancedView             [] = "advancedView";
const CN_PROGMEM char CN_allleds                  [] = "allleds";
const CN_PROGMEM char CN_ap_fallback              [] = "ap_fallback";
const CN_PROGMEM char CN_ap_timeout               [] = "ap_timeout";
const CN_PROGMEM char CN_ap_reboot                [] = "ap_reboot";
const CN_PROGMEM char CN_appendnullcount          [] = "appendnullcount";
const CN_PROGMEM char CN_b                        [] = "b";
const CN_PROGMEM char CN_b16                      [] = "b16";
const CN_PROGMEM char CN_baudrate                 [] = "baudrate";
const CN_PROGMEM char CN_blanktime                [] = "blanktime";
const CN_PROGMEM char CN_bridge                   [] = "bridge";
const CN_PROGMEM char CN_brightness               [] = "brightness";
const CN_PROGMEM char CN_cfgver                   [] = "cfgver";
const CN_PROGMEM char CN_channels                 [] = "channels";
const CN_PROGMEM char CN_clean                    [] = "clean";
const CN_PROGMEM char CN_clock_pin                [] = "clock_pin";
const CN_PROGMEM char CN_cmd                      [] = "cmd";
const CN_PROGMEM char CN_color                    [] = "color";
const CN_PROGMEM char CN_color_order              [] = "color_order";
const CN_PROGMEM char CN_Configuration_File_colon [] = "Configuration File: ";
const CN_PROGMEM char CN_config                   [] = "config";
const CN_PROGMEM char CN_connected                [] = "connected";
const CN_PROGMEM char CN_count                    [] = "count";
const CN_PROGMEM char CN_currenteffect            [] = "currenteffect";
const CN_PROGMEM char CN_currentlimit             [] = "currentlimit";
const CN_PROGMEM char CN_cs_pin                   [] = "cs_pin";
const CN_PROGMEM char CN_current_sequence         [] = "current_sequence";
const CN_PROGMEM char CN_data_pin                 [] = "data_pin";
const CN_PROGMEM char CN_device                   [] = "device";
const CN_PROGMEM char CN_dhcp                     [] = "dhcp";
const CN_PROGMEM char CN_Dotfseq                  [] = ".fseq";
const CN_PROGMEM char CN_Dotpl                    [] = ".pl";
const CN_PROGMEM char CN_duration                 [] = "duration";
const CN_PROGMEM char CN_effect                   [] = "effect";
const CN_PROGMEM char CN_effect_list              [] = "effect_list";
const CN_PROGMEM char CN_EffectAllLeds            [] = "EffectAllLeds";
const CN_PROGMEM char CN_EffectBrightness         [] = "EffectBrightness";
const CN_PROGMEM char CN_EffectColor              [] = "EffectColor";
const CN_PROGMEM char CN_EffectMirror             [] = "EffectMirror";
const CN_PROGMEM char CN_EffectReverse            [] = "EffectReverse";
const CN_PROGMEM char CN_EffectSpeed              [] = "EffectSpeed";
const CN_PROGMEM char CN_EffectWhiteChannel       [] = "EffectWhiteChannel";
const CN_PROGMEM char CN_Effect                   [] = "Effect";
const CN_PROGMEM char CN_effects                  [] = "effects";
const CN_PROGMEM char CN_en                       [] = "en";
const CN_PROGMEM char CN_enabled                  [] = "enabled";
const CN_PROGMEM char CN_errors                   [] = "errors";
const CN_PROGMEM char CN_ESP32                    [] = "ESP32";
const CN_PROGMEM char CN_ESP8266                  [] = "ESP8266";
const CN_PROGMEM char CN_ESPixelStick             [] = "ESPixelStick";
const CN_PROGMEM char CN_eth                      [] = "eth";
const CN_PROGMEM char CN_EthDrv                   [] = "EthDrv";
const CN_PROGMEM char CN_false                    [] = "false";
const CN_PROGMEM char CN_File                     [] = "File";
const CN_PROGMEM char CN_file                     [] = "file";
const CN_PROGMEM char CN_filename                 [] = "filename";
const CN_PROGMEM char CN_files                    [] = "files";
const CN_PROGMEM char CN_Frequency                [] = "Frequency";
const CN_PROGMEM char CN_fseqfilename             [] = "fseqfilename";
const CN_PROGMEM char CN_g                        [] = "g";
const CN_PROGMEM char CN_gamma                    [] = "gamma";
const CN_PROGMEM char CN_gateway                  [] = "gateway";
const CN_PROGMEM char CN_get                      [] = "get";
const CN_PROGMEM char CN_gen_ser_hdr              [] = "gen_ser_hdr";
const CN_PROGMEM char CN_gen_ser_ftr              [] = "gen_ser_ftr";
const CN_PROGMEM char CN_gid                      [] = "gid";
const CN_PROGMEM char CN_group_size               [] = "group_size";
const CN_PROGMEM char CN_Heap_colon               [] = "Heap: ";
const CN_PROGMEM char CN_hadisco                  [] = "hadisco";
const CN_PROGMEM char CN_haprefix                 [] = "haprefix";
const CN_PROGMEM char CN_HostName                 [] = "HostName";
const CN_PROGMEM char CN_hostname                 [] = "hostname";
const CN_PROGMEM char CN_id                       [] = "id";
const CN_PROGMEM char CN_Idle                     [] = "Idle";
const CN_PROGMEM char CN_init                     [] = "init";
const CN_PROGMEM char CN_interframetime           [] = "interframetime";
const CN_PROGMEM char CN_inv                      [] = "inv";
const CN_PROGMEM char CN_ip                       [] = "ip";
const CN_PROGMEM char CN_input                    [] = "input";
const CN_PROGMEM char CN_input_config             [] = "input_config";
const CN_PROGMEM char CN_last_clientIP            [] = "last_clientIP";
const CN_PROGMEM char CN_lwt                      [] = "lwt";
const CN_PROGMEM char CN_mac                      [] = "mac";
const CN_PROGMEM char CN_mdc_pin                  [] = "mdc_pin";
const CN_PROGMEM char CN_mdio_pin                 [] = "mdio_pin";
const CN_PROGMEM char CN_Max                      [] = "Max";
const CN_PROGMEM char CN_Min                      [] = "Min";
const CN_PROGMEM char CN_minussigns               [] = "-----";
const CN_PROGMEM char CN_mirror                   [] = "mirror";
const CN_PROGMEM char CN_miso_pin                 [] = "miso_pin";
const CN_PROGMEM char CN_mode                     [] = "mode";
const CN_PROGMEM char CN_mode_name                [] = "mode_name";
const CN_PROGMEM char CN_mosi_pin                 [] = "mosi_pin";
const CN_PROGMEM char CN_multicast                [] = "multicast";
const CN_PROGMEM char CN_name                     [] = "name";
const CN_PROGMEM char CN_NeedAutoConfig           [] = "NeedAutoConfig";
const CN_PROGMEM char CN_netmask                  [] = "netmask";
const CN_PROGMEM char CN_network                  [] = "network";
const CN_PROGMEM char CN_num_chan                 [] = "num_chan";
const CN_PROGMEM char CN_num_packets              [] = "num_packets";
const CN_PROGMEM char CN_output                   [] = "output";
const CN_PROGMEM char CN_output_config            [] = "output_config";
const CN_PROGMEM char CN_packet_errors            [] = "packet_errors";
const CN_PROGMEM char CN_passphrase               [] = "passphrase";
const CN_PROGMEM char CN_password                 [] = "password";
const CN_PROGMEM char CN_Paused                   [] = "Paused";
const CN_PROGMEM char CN_pixel_count              [] = "pixel_count";
const CN_PROGMEM char CN_Platform                 [] = "Platform";
const CN_PROGMEM char CN_play                     [] = "play";
const CN_PROGMEM char CN_playFseq                 [] = "playFseq";
const CN_PROGMEM char CN_playlist                 [] = "playlist";
const CN_PROGMEM char CN_plussigns                [] = "+++++";
const CN_PROGMEM char CN_polarity                 [] = "polarity";
const CN_PROGMEM char CN_port                     [] = "port";
const CN_PROGMEM char CN_power_pin                [] = "power_pin";
const CN_PROGMEM char CN_prependnullcount         [] = "prependnullcount";
const CN_PROGMEM char CN_pwm                      [] = "pwm";
const CN_PROGMEM char CN_r                        [] = "r";
const CN_PROGMEM char CN_remote                   [] = "remote";
const CN_PROGMEM char CN_rev                      [] = "rev";
const CN_PROGMEM char CN_reverse                  [] = "reverse";
const CN_PROGMEM char CN_RMT                      [] = "RMT";
const CN_PROGMEM char CN_rssi                     [] = "rssi";
const CN_PROGMEM char CN_sca                      [] = "sca";
const CN_PROGMEM char CN_seconds_elapsed          [] = "seconds_elapsed";
const CN_PROGMEM char CN_seconds_played           [] = "seconds_played";
const CN_PROGMEM char CN_seconds_remaining        [] = "seconds_remaining";
const CN_PROGMEM char CN_sequence_filename        [] = "sequence_filename";
const CN_PROGMEM char CN_slashset                 [] = "/set";
const CN_PROGMEM char CN_slashstatus              [] = "/status";
const CN_PROGMEM char CN_speed                    [] = "speed";
const CN_PROGMEM char CN_ssid                     [] = "ssid";
const CN_PROGMEM char CN_sta_timeout              [] = "sta_timeout";
const CN_PROGMEM char CN_stars                    [] = "***";
const CN_PROGMEM char CN_state                    [] = "state";
const CN_PROGMEM char CN_status                   [] = "status";
const CN_PROGMEM char CN_status_name              [] = "status_name";
const CN_PROGMEM char CN_subnet                   [] = "subnet";
const CN_PROGMEM char CN_SyncOffset               [] = "SyncOffset";
const CN_PROGMEM char CN_system                   [] = "system";
const CN_PROGMEM char CN_textSLASHplain           [] = "text/plain";
const CN_PROGMEM char CN_time                     [] = "time";
const CN_PROGMEM char CN_time_elapsed             [] = "time_elapsed";
const CN_PROGMEM char CN_TimeRemaining            [] = "TimeRemaining";
const CN_PROGMEM char CN_time_remaining           [] = "time_remaining";
const CN_PROGMEM char CN_topic                    [] = "topic";
const CN_PROGMEM char CN_topicset                 [] = "topicset";
const CN_PROGMEM char CN_trig                     [] = "trig";
const CN_PROGMEM char CN_true                     [] = "true";
const CN_PROGMEM char CN_type                     [] = "type";
const CN_PROGMEM char CN_ui                       [] = "ui";
const CN_PROGMEM char CN_unichanlim               [] = "unichanlim";
const CN_PROGMEM char CN_unifirst                 [] = "unifirst";
const CN_PROGMEM char CN_unilast                  [] = "unilast";
const CN_PROGMEM char CN_universe                 [] = "universe";
const CN_PROGMEM char CN_universe_limit           [] = "universe_limit";
const CN_PROGMEM char CN_universe_start           [] = "universe_start";
const CN_PROGMEM char CN_updateinterval           [] = "updateinterval";
const CN_PROGMEM char CN_user                     [] = "user";
const CN_PROGMEM char CN_version                  [] = "version";
const CN_PROGMEM char CN_Version                  [] = "Version";
const CN_PROGMEM char CN_weus                     [] = "weus";
const CN_PROGMEM char CN_wifi                     [] = "wifi";
const CN_PROGMEM char CN_WiFiDrv                  [] = "WiFiDrv";
const CN_PROGMEM char CN_XP                       [] = "XP";
const CN_PROGMEM char CN_zig_size                 [] = "zig_size";
