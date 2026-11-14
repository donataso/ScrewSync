#ifndef display_config_h
#define display_config_h

#define DISPLAY_UPDATE_INTERVAL_MS 100

// Display -> firmware command frame lead byte (docs/screen.md); must match
// pages/lib/link.nx
#define DISPLAY_CMD_LEAD_BYTE 0x7C
#define DISPLAY_PING_INTERVAL_MS 500
#define DISPLAY_PING_TIMEOUT_MS 1500
// How long a page change may keep the display from answering
#define DISPLAY_PAGE_LOAD_TIMEOUT_MS 3000
// Must match baud in program.nx
#define DISPLAY_LINK_BAUD 230400
// .tft upload (whmi-wri), whatever baud "connect" used
#define DISPLAY_UPLOAD_BAUD 460800
// How long a stored-on-upload .tft waits for the reflashed display to answer
#define DISPLAY_RECONNECT_TIMEOUT_MS 30000

// Must match tmJog.tim in pages/page0.page.toml
#define JOG_REPEAT_INTERVAL_MS 100

#endif
