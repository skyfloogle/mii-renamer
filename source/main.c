#include <gccore.h>
#include <wiikeyboard/keyboard.h>
#include <wiiuse/wpad.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

bool quitapp = false;

#define WINDOW_SIZE 18

#define WPAD_DPAD (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_UP | WPAD_BUTTON_DOWN)

struct mii_t
{
	u8 data1[2];
	u16 name[10];
	u8 data2[32];
	u16 creator[10];
};

struct mii_db
{
	u8 header[4];
	struct mii_t mii[100];
	u8 fill[20];
	u8 db_data[0x1D4DE];
	u16 crc;
};

struct mii_db db ATTRIBUTE_ALIGN(32);

// Enum for standard ANSI VT colors (0-7)
typedef enum {
    BLACK = 0,
    RED = 1,
    GREEN = 2,
    YELLOW = 3,
    BLUE = 4,
    MAGENTA = 5,
    CYAN = 6,
    WHITE = 7
} Color;

// Function to set the terminal cursor position at X,Y cordinates.
static void SetCursor(int x, int y) {
    printf("\x1b[%d;%dH", y,x);
}

// Function to set background and foreground color on console.
static void SetColors(Color background, Color foreground) {
    printf("\x1b[%dm", 40 + background); // BG
    printf("\x1b[%dm", 30 + foreground); // FG
}

static void ClearBelowCursor() {
    printf("\x1b[0J");
}

// Function to clear the screen.
static void ClearScreen() {
    printf("\x1b[2J");
}

// Critical Error Display.
static void StopCritical(const char* msg) {
    // Clear the display with blue.
    SetColors(BLUE, WHITE);
    ClearScreen();
    SetCursor(0,2);

    // Error
    printf("DEMO ERROR\n");
    SetColors(BLACK, YELLOW);
    printf(msg);
    SetColors(BLUE, WHITE);
    printf("\nPress HOME or RESET to exit.\n\n");

    // Critial Loop.
    while(true) {
        // Scan WPAD
        WPAD_ScanPads();
        s32 pressed[4]; // State on every remote.

        // Get the remote states.
        for(int rmt = 0; rmt < 4; rmt++) pressed[rmt] = WPAD_ButtonsDown(rmt);

        // Check for home on each remote, and check reset button.
        for(int rmt = 0; rmt < 4; rmt++) if(pressed[rmt] & WPAD_BUTTON_HOME || SYS_ResetButtonDown()) break;
        
        // Wait VSync.
        VIDEO_WaitVSync();
    }

    // Notify that the system is exiting.
    printf("\nEXITING...\n");
    exit(1);
}

u32 read_inputs(void) {
	// Call WPAD_ScanPads each loop, this reads the latest controller states
	WPAD_ScanPads();

	// WPAD_ButtonsDown tells us which buttons were pressed in this loop
	// this is a "one shot" state which will not fire again until the button
	// has been released
	u32 pressed = WPAD_ButtonsDown(0);

	// We return to the launcher application via exit
	if (pressed & WPAD_BUTTON_HOME)
		quitapp = true;

	if (SYS_ResetButtonDown())
		quitapp = true;

	return pressed;
}

void anykey(void) {
	printf("Press any key to return.\n");
	while (true) {
		if (read_inputs()) return;
		VIDEO_WaitVSync();
	}
}

int read_dpad(void) {
	static int das = 0;
	static int das_inputs = 0;

	u32 held = WPAD_ButtonsHeld(0);

	if ((held & WPAD_DPAD) != das_inputs) {
		das_inputs = held & WPAD_DPAD;
		das = 0;
	} else {
		das += 1;
	}

	if (das == 1 || (das > 30 && das % 4 == 0)) {
		return das_inputs;
	}
	return 0;
}

void mii_rename(u16 *name_ptr) {
	SetCursor(0, 5);
	ClearBelowCursor();
	int cursor = 0;
	char name[10];
	for (int j = 0; j < 10; j++) {
		name[j] = name_ptr[j];
	}
	int name_len = strnlen(name, 10);

	// Check name safety
	for (int j = 0; j < name_len; j++) {
		if ((name_ptr[j] < 0x20 || name_ptr[j] >= 0x7f)) {
			printf("This Mii's name contains non-ASCII characters, which aren't supported.\n");
			anykey();
			return;
		}
	}

	printf("Left/Right to move cursor, Up/Down to change letter,\nA to confirm, B to cancel, HOME to exit\n");

	do {
		u32 pressed = read_inputs();

		if (pressed & WPAD_BUTTON_A) {
			for (int j = 0; j < 10; j++) {
				name_ptr[j] = name[j];
			}
			return;
		}
		if (pressed & WPAD_BUTTON_B) return;

		u32 current_dpad = read_dpad();

		cursor += ((current_dpad & WPAD_BUTTON_RIGHT) != 0) - ((current_dpad & WPAD_BUTTON_LEFT) != 0);
		if (cursor < 0) cursor = name_len;
		if (cursor >= name_len + 1) cursor = 0;

		int char_change = ((current_dpad & WPAD_BUTTON_UP) != 0) - ((current_dpad & WPAD_BUTTON_DOWN) != 0);
		if (char_change != 0) {
			bool going_outside_range = name[cursor] + char_change < 0x20 || name[cursor] + char_change >= 0x7f;
			if (name[cursor] == 0 || (cursor == 0 && going_outside_range)) {
				if (cursor < 9 && name[cursor] == 0) name[cursor+1] = 0;
				if (char_change > 0) name[cursor] = 0x20;
				else name[cursor] = 0x7e;
			} else {
				name[cursor] += char_change;
				if (name[cursor] < 0x20 || name[cursor] >= 0x7f) {
					name[cursor] = 0;
				}
			}
			name_len = strnlen(name, 10);
		}

		SetCursor(0, 9);
		ClearBelowCursor();
		printf("[%.10s]", name);
		SetCursor(cursor + 1, 10);
		printf("^");

		// Wait for the next frame
		VIDEO_WaitVSync();
	} while (!quitapp);
}

void update_crc(void) {
	u8 *buf = (u8*)&db;
	u32 len = sizeof(db) - sizeof(db.crc);

	u16 crc = 0x0000;
	int byte_idx, bit_idx, cnt;

	for (byte_idx = 0; byte_idx < (int)len; byte_idx++)
	{
		for (bit_idx = 7; bit_idx >= 0; bit_idx--)
			crc = (((crc << 1) | ((buf[byte_idx] >> bit_idx) & 0x1)) ^ (((crc & 0x8000) != 0) ? 0x1021 : 0));
	}

	for (cnt = 16; cnt > 0; cnt--)
		crc = ((crc << 1) ^ (((crc & 0x8000) != 0) ? 0x1021 : 0));

	db.crc = crc;
}

void save_db(void) {
	SetCursor(0, 5);
	ClearBelowCursor();
	printf("Saving... ");
	VIDEO_WaitVSync();

	update_crc();

	int db_fd = ISFS_Open("/shared2/menu/FaceLib/RFL_DB.dat", ISFS_OPEN_RW);
	if (db_fd < 0) {
		printf("\nCould not open RFL_DB.dat, save aborted! Error code: %d\n", db_fd);
		anykey();
		return;
	}

	int ret = ISFS_Write(db_fd, &db, sizeof(db));
	ISFS_Close(db_fd);
	if (ret != sizeof(db)) {
		printf("\nFailed to write db, it may have been corrupted! Error code: %d\n", ret);
	} else {
		printf("Done!\n");
	}
	anykey();
	return;
}

static int mii_count = 0;
static int mii_ids[100] = {0};

void redraw_mii_list(int camera) {
	SetCursor(0, 8);
	ClearBelowCursor();
	for (int i = 0; i < WINDOW_SIZE && i + camera < mii_count; i++) {
		char name[10];
		for (int j = 0; j < 10; j++) {
			name[j] = db.mii[mii_ids[i + camera]].name[j];
		}
		printf(" %.10s\n", name);
	}
}

void mii_selector_reset(int camera) {
	SetCursor(0, 5);
	ClearBelowCursor();
	printf("D-Pad to select, A to rename, + to save, HOME to exit\n");
	redraw_mii_list(camera);
}

void mii_selector(void) {
	int cursor = 0;
	int camera = 0;
	mii_selector_reset(camera);
	do {
		u32 pressed = read_inputs();

		if (pressed & WPAD_BUTTON_PLUS) {
			save_db();
			mii_selector_reset(camera);
			pressed = 0;
		}

		if (pressed & WPAD_BUTTON_A) {
			mii_rename(db.mii[mii_ids[cursor]].name);
			mii_selector_reset(camera);
			pressed = 0;
		}

		SetCursor(0, 8 + cursor - camera);
		printf(" ");

		u32 current_dpad = read_dpad();

		cursor += ((current_dpad & WPAD_BUTTON_DOWN) != 0) - ((current_dpad & WPAD_BUTTON_UP) != 0);

		if (cursor < 0) {
			cursor = mii_count - 1;
		}
		if (cursor >= mii_count) {
			cursor = 0;
		}

		if (mii_count > WINDOW_SIZE) {
			if (cursor < camera) {
				camera = cursor;
				redraw_mii_list(camera);
			}
			if (camera <= cursor - WINDOW_SIZE) {
				camera = cursor - WINDOW_SIZE + 1;
				redraw_mii_list(camera);
			}
		}
		SetCursor(0, 8 + cursor - camera);
		printf(">");

		// Wait for the next frame
		VIDEO_WaitVSync();
	} while (!quitapp);
}

int main(void) {
	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	WPAD_Init();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	// Initialise the console, required for printf
	console_init(xfb, 20, 20, rmode->fbWidth-20, rmode->xfbHeight-20,
				 rmode->fbWidth * VI_DISPLAY_PIX_SZ);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Clear the framebuffer
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	// Make the display visible
	VIDEO_SetBlack(false);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	// The console understands VT terminal escape codes
	// This positions the cursor on row 2, column 0
	// we can use variables for this with format codes too
	// e.g. printf ("\x1b[%d;%dH", row, column );
	printf("\x1b[2;0HMii Renamer by Floogle\n");
	printf("https://github.com/skyfloogle/mii-renamer\n");

    // Initialize ISFS
    if(ISFS_Initialize() != ISFS_OK) {
        StopCritical("Failed to initialize ISFS\n");
	}

	int db_fd = ISFS_Open("/shared2/menu/FaceLib/RFL_DB.dat", ISFS_OPEN_RW);
	if (db_fd < 0) {
		printf("Error code %d\n", db_fd);
		StopCritical("Could not open RFL_DB.dat\n");
	}

	int ret = ISFS_Read(db_fd, &db, sizeof(db));

	ISFS_Close(db_fd);

	for (int i = 0; i < 100; i++) {
		if (db.mii[i].name[0] != 0) {
			mii_ids[mii_count++] = i;
		}
	}
	
	if (ret != sizeof(db)) {
		printf("Error code %d\n", ret);
		StopCritical("Error reading RFL_DB.dat\n");
	}

	mii_selector();

	return 0;
}
