#ifndef SMALL
#ifndef NO_STBIMAGE

#include "openiboot.h"
#include "lcd.h"
#include "util.h"
#include "framebuffer.h"
#include "buttons.h"
#include "timer.h"
#include "images/ConsolePNG.h"
#include "images/iPhoneOSPNG.h"
#include "images/AndroidOSPNG.h"
#include "images/ixPNG.h"
#include "images/ConsoleSelectedPNG.h"
#include "images/iPhoneOSSelectedPNG.h"
#include "images/AndroidOSSelectedPNG.h"
#include "images/ixSelectedPNG.h"
#include "images/HeaderPNG.h"
#include "images.h"
#include "actions.h"
#include "stb_image.h"
#include "pmu.h"
#include "nand.h"
#include "radio.h"
#include "hfs/fs.h"
#include "ftl.h"
#include "scripting.h"
#include "multitouch.h"

int globalFtlHasBeenRestored; /* global variable to tell wether a ftl_restore has been done*/

static uint32_t FBWidth;
static uint32_t FBHeight;


static uint32_t* imgiPhoneOS;
static int imgiPhoneOSWidth;
static int imgiPhoneOSHeight;
static int imgiPhoneOSX;
static int imgiPhoneOSY;

static uint32_t* imgConsole;
static int imgConsoleWidth;
static int imgConsoleHeight;
static int imgConsoleX;
static int imgConsoleY;

static uint32_t* imgAndroidOS;
static int imgAndroidOSWidth;
static int imgAndroidOSHeight;
static int imgAndroidOSX;
static int imgAndroidOSY;

static uint32_t* imgix;
static int imgixWidth;
static int imgixHeight;
static int imgixX;
static int imgixY;

static uint32_t* imgiPhoneOSSelected;
static uint32_t* imgixSelected;
static uint32_t* imgConsoleSelected;
static uint32_t* imgAndroidOSSelected;

static uint32_t* imgHeader;
static int imgHeaderWidth;
static int imgHeaderHeight;
static int imgHeaderX;
static int imgHeaderY;

typedef enum MenuSelection {
	MenuSelectioniPhoneOS,
	MenuSelectionConsole,
	MenuSelectionAndroidOS,
	MenuSelectionix
} MenuSelection;

static MenuSelection Selection;

volatile uint32_t* OtherFramebuffer;

static void drawSelectionBox() {
	volatile uint32_t* oldFB = CurFramebuffer;

	CurFramebuffer = OtherFramebuffer;
	currentWindow->framebuffer.buffer = CurFramebuffer;
	OtherFramebuffer = oldFB;

	if(Selection == MenuSelectioniPhoneOS) {
		framebuffer_draw_image(imgiPhoneOSSelected, imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight);
		framebuffer_draw_image(imgConsole, imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight);
		framebuffer_draw_image(imgAndroidOS, imgAndroidOSX, imgAndroidOSY, imgAndroidOSWidth, imgAndroidOSHeight);
		framebuffer_draw_image(imgix, imgixX, imgixY, imgixWidth, imgixHeight);
	}

	if(Selection == MenuSelectionConsole) {
		framebuffer_draw_image(imgiPhoneOS, imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight);
		framebuffer_draw_image(imgConsoleSelected, imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight);
		framebuffer_draw_image(imgAndroidOS, imgAndroidOSX, imgAndroidOSY, imgAndroidOSWidth, imgAndroidOSHeight);
		framebuffer_draw_image(imgix, imgixX, imgixY, imgixWidth, imgixHeight);
	}

	if(Selection == MenuSelectionAndroidOS) {
		framebuffer_draw_image(imgiPhoneOS, imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight);
		framebuffer_draw_image(imgConsole, imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight);
		framebuffer_draw_image(imgAndroidOSSelected, imgAndroidOSX, imgAndroidOSY, imgAndroidOSWidth, imgAndroidOSHeight);
		framebuffer_draw_image(imgix, imgixX, imgixY, imgixWidth, imgixHeight);
	}

	if(Selection == MenuSelectionix) {
		framebuffer_draw_image(imgiPhoneOS, imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight);
		framebuffer_draw_image(imgConsole, imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight);
		framebuffer_draw_image(imgAndroidOS, imgAndroidOSX, imgAndroidOSY, imgAndroidOSWidth, imgAndroidOSHeight);
		framebuffer_draw_image(imgixSelected, imgixX, imgixY, imgixWidth, imgixHeight);
	}

	lcd_window_address(2, (uint32_t) CurFramebuffer);
}

static int touch_watcher()
{
    uint8_t isFound = 0;
    multitouch_run();
    if (multitouch_ispoint_inside_region(imgiPhoneOSX, imgiPhoneOSY, imgiPhoneOSWidth, imgiPhoneOSHeight) == TRUE) {
        Selection = MenuSelectioniPhoneOS;
        isFound = 1;
    }
    else if (multitouch_ispoint_inside_region(imgConsoleX, imgConsoleY, imgConsoleWidth, imgConsoleHeight) == TRUE) {
        Selection = MenuSelectionConsole;
        isFound = 1;
    }
    else if (multitouch_ispoint_inside_region(imgAndroidOSX, imgAndroidOSY, imgAndroidOSWidth, imgAndroidOSHeight) == TRUE) {
        Selection = MenuSelectionAndroidOS;
        isFound = 1;
    }
    else if (multitouch_ispoint_inside_region(imgixX, imgixY, imgixWidth, imgixHeight) == TRUE) {
        Selection = MenuSelectionAndroidOS;
        isFound = 1;
    }
    
    if (isFound ==1) {
        drawSelectionBox();
        return TRUE;
    }
    return FALSE;
}

static void toggle(int forward) {
	if(forward)
	{
		if(Selection == MenuSelectioniPhoneOS)
			Selection = MenuSelectionConsole;
		else if(Selection == MenuSelectionConsole)
			Selection = MenuSelectionAndroidOS;
		else if(Selection == MenuSelectionAndroidOS)
			Selection = MenuSelectionix;
		else if(Selection == MenuSelectionix)
			Selection = MenuSelectioniPhoneOS;
	} else
	{
		if(Selection == MenuSelectioniPhoneOS)
			Selection = MenuSelectionix;
		else if(Selection == MenuSelectionAndroidOS)
			Selection = MenuSelectionConsole;
		else if(Selection == MenuSelectionConsole)
			Selection = MenuSelectioniPhoneOS;
		else if(Selection == MenuSelectionix)
			Selection = MenuSelectionAndroidOS;
	}

	drawSelectionBox();
}

int menu_setup(int timeout, int defaultOS) {
	FBWidth = currentWindow->framebuffer.width;
	FBHeight = currentWindow->framebuffer.height;	

	imgiPhoneOS = framebuffer_load_image(dataiPhoneOSPNG, dataiPhoneOSPNG_size, &imgiPhoneOSWidth, &imgiPhoneOSHeight, TRUE);
	imgiPhoneOSSelected = framebuffer_load_image(dataiPhoneOSSelectedPNG, dataiPhoneOSSelectedPNG_size, &imgiPhoneOSWidth, &imgiPhoneOSHeight, TRUE);
	imgConsole = framebuffer_load_image(dataConsolePNG, dataConsolePNG_size, &imgConsoleWidth, &imgConsoleHeight, TRUE);
	imgConsoleSelected = framebuffer_load_image(dataConsoleSelectedPNG, dataConsoleSelectedPNG_size, &imgConsoleWidth, &imgConsoleHeight, TRUE);
	imgAndroidOS = framebuffer_load_image(dataAndroidOSPNG, dataAndroidOSPNG_size, &imgAndroidOSWidth, &imgAndroidOSHeight, TRUE);
	imgAndroidOSSelected = framebuffer_load_image(dataAndroidOSSelectedPNG, dataAndroidOSSelectedPNG_size, &imgAndroidOSWidth, &imgAndroidOSHeight, TRUE);
	imgix = framebuffer_load_image(dataixPNG, dataixPNG_size, &imgixWidth, &imgixHeight, TRUE);
	imgixSelected = framebuffer_load_image(dataixSelectedPNG, dataixSelectedPNG_size, &imgixWidth, &imgixHeight, TRUE);
	imgHeader = framebuffer_load_image(dataHeaderPNG, dataHeaderPNG_size, &imgHeaderWidth, &imgHeaderHeight, TRUE);

	bufferPrintf("menu: images loaded\r\n");

	imgiPhoneOSX = (FBWidth - imgiPhoneOSWidth) / 2;
	imgiPhoneOSY = 67;

	imgConsoleX = (FBWidth - imgConsoleWidth) / 2;
	imgConsoleY = 190;

	imgAndroidOSX = (FBWidth - imgAndroidOSWidth) / 2;
	imgAndroidOSY = 313;

	imgixX = 20;
	imgixY = 313;

	imgHeaderX = (FBWidth - imgHeaderWidth) / 2;
	imgHeaderY = 0;

	framebuffer_draw_image(imgHeader, imgHeaderX, imgHeaderY, imgHeaderWidth, imgHeaderHeight);


	framebuffer_setloc(0, 47);
	framebuffer_setcolors(COLOR_WHITE, COLOR_BLACK);
	framebuffer_print_force(OPENIBOOT_VERSION_STR);
	framebuffer_setloc(0, 0);

	switch(defaultOS){
		case 0:
			Selection = MenuSelectioniPhoneOS;
			break;
		case 1:
			Selection = MenuSelectionAndroidOS;
			break;
		case 2:
			Selection = MenuSelectionConsole;
			break;
		case 3:
			Selection = MenuSelectionix;
			break;
		default:
			Selection = MenuSelectioniPhoneOS;
	}

	OtherFramebuffer = CurFramebuffer;
	CurFramebuffer = (volatile uint32_t*) NextFramebuffer;

	drawSelectionBox();

	pmu_set_iboot_stage(0);

	memcpy((void*)NextFramebuffer, (void*) CurFramebuffer, NextFramebuffer - (uint32_t)CurFramebuffer);

	uint64_t startTime = timer_get_system_microtime();
	uint64_t powerStartTime = timer_get_system_microtime();
	int timeoutLeft = (timeout / 1000);
	while(TRUE) {
		char timeoutstr[4] = "";
		if(timeout >= 0){
			if(timeoutLeft == 9){
				sprintf(timeoutstr, "  ");
				framebuffer_setloc(49, 47);
				framebuffer_print_force(timeoutstr);
				framebuffer_setloc(0,0);
			}
			sprintf(timeoutstr, "%d", timeoutLeft);
			if(has_elapsed(startTime, (uint64_t)(timeout - (timeoutLeft * 1000)) * 1000)){
				timeoutLeft -= 1;
			}
			framebuffer_setloc(49, 47);
			framebuffer_print_force(timeoutstr);
			framebuffer_setloc(0,0);
			drawSelectionBox();
		} else if(timeout == -1) {
			timeoutLeft = -1;
			sprintf(timeoutstr, "  ");
			framebuffer_setloc(49, 47);
			framebuffer_print_force(timeoutstr);
			framebuffer_setloc(0,0);
			drawSelectionBox();
		}
        	if (isMultitouchLoaded) {
            		if(touch_watcher()) {
                		break;
            		} else {
	        	        startTime = timer_get_system_microtime();
	        	        udelay(100000);
  	        	}
        	}
#ifdef CONFIG_IPOD
		if(buttons_is_pushed(BUTTONS_HOLD)) {
			toggle(FALSE);
			startTime = timer_get_system_microtime();
			udelay(200000);
		}
#endif
#ifndef CONFIG_IPOD
		if(buttons_is_pushed(BUTTONS_HOLD)) {

			if(has_elapsed(powerStartTime, (uint64_t)300 * 1000)) {
				
			} else if(has_elapsed(powerStartTime, (uint64_t)200 * 1000)) {
				toggle(TRUE);
			} 

			if(has_elapsed(powerStartTime, (uint64_t)2000 * 1000)) {
				pmu_poweroff();
			}
			udelay(200000);
		} else {
			powerStartTime = timer_get_system_microtime();
			udelay(200000);
		}
		if(!buttons_is_pushed(BUTTONS_VOLUP)) {
			toggle(FALSE);
			startTime = timer_get_system_microtime();
			timeout = -1;
			udelay(200000);
		}
		if(!buttons_is_pushed(BUTTONS_VOLDOWN)) {
			toggle(TRUE);
			startTime = timer_get_system_microtime();
			timeout = -1;
			udelay(200000);
		}
#endif
		if(buttons_is_pushed(BUTTONS_HOME)) {
			timeout = -1;
			break;
		}
		if(timeout > 0 && has_elapsed(startTime, (uint64_t)timeout * 1000)) {
			bufferPrintf("menu: timed out, selecting current item\r\n");
			break;
		}
		udelay(10000);
	}

	if(Selection == MenuSelectioniPhoneOS) {
		Image* image = images_get(fourcc("ibox"));
		if(image == NULL)
			image = images_get(fourcc("ibot"));
		void* imageData;
		images_read(image, &imageData);
		chainload((uint32_t)imageData);
	}

	if(Selection == MenuSelectionConsole) {
		// Reset framebuffer back to original if necessary
		if((uint32_t) CurFramebuffer == NextFramebuffer)
		{
			CurFramebuffer = OtherFramebuffer;
			currentWindow->framebuffer.buffer = CurFramebuffer;
			lcd_window_address(2, (uint32_t) CurFramebuffer);
		}

		framebuffer_setdisplaytext(TRUE);
		framebuffer_clear();
	}

	if(Selection == MenuSelectionAndroidOS) {
		// Reset framebuffer back to original if necessary
		if((uint32_t) CurFramebuffer == NextFramebuffer)
		{
			CurFramebuffer = OtherFramebuffer;
			currentWindow->framebuffer.buffer = CurFramebuffer;
			lcd_window_address(2, (uint32_t) CurFramebuffer);
		}

		framebuffer_setdisplaytext(TRUE);
		framebuffer_clear();

#ifndef NO_HFS
#ifndef CONFIG_IPOD
		radio_setup();
#endif
		nand_setup();
		fs_setup();
		if(globalFtlHasBeenRestored) /* if ftl has been restored, sync it, so kernel doesn't have to do a ftl_restore again */
		{
			if(ftl_sync())
			{
				bufferPrintf("ftl synced successfully");
			}
			else
			{
				bufferPrintf("error syncing ftl");
			}
		}

		pmu_set_iboot_stage(0);
		startScripting("linux"); //start script mode if there is a script file
		boot_linux_from_files();
#endif
	}
	if(Selection == MenuSelectionix) {
		// Reset framebuffer back to original if necessary
		if((uint32_t) CurFramebuffer == NextFramebuffer)
		{
			CurFramebuffer = OtherFramebuffer;
			currentWindow->framebuffer.buffer = CurFramebuffer;
			lcd_window_address(2, (uint32_t) CurFramebuffer);
		}

		framebuffer_setdisplaytext(TRUE);
		framebuffer_clear();

#ifndef NO_HFS
#ifndef CONFIG_IPOD
		radio_setup();
#endif
		nand_setup();
		fs_setup();
		if(globalFtlHasBeenRestored) /* if ftl has been restored, sync it, so kernel doesn't have to do a ftl_restore again */
		{
			if(ftl_sync())
			{
				bufferPrintf("ftl synced successfully");
			}
			else
			{
				bufferPrintf("error syncing ftl");
			}
		}

		pmu_set_iboot_stage(0);
		startScripting("linux"); //start script mode if there is a script file
		boot_ix_from_files();
#endif
	}

	return 0;
}

#endif
#endif

