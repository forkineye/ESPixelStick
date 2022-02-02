/*
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x009000, 0x004000,
otadata,  data, ota,     0x00E000, 0x002000,
phy_init, data, phy,     0x00F000, 0x001000
app0,     app,  ota_0,   0x010000, 0x1D0000,
app1,     app,  ota_1,   0x1E0000, 0x1D0000,
spiffs,   data, spiffs,  0x3B0000, 0x050000,
*/

MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  irom0_0_seg :                         org = 0x40201010, len = 0x79ff0
}

PROVIDE ( _FS_start = 0x4027B000 );
PROVIDE ( _FS_end = 0x402FB000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x2000 );
PROVIDE ( _EEPROM_start = 0x402fb000 );
/* The following symbols are DEPRECATED and will be REMOVED in a future release */
PROVIDE ( _SPIFFS_start = 0x4027B000 );
PROVIDE ( _SPIFFS_end = 0x402FB000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x2000 );

INCLUDE "local.eagle.app.v6.common.ld"