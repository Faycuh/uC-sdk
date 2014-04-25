#include <BoardConsole.h>

#include "gpio.h"
#include "ssp.h"
#include "sdcard.h"

#define MAX_RETRIES 10000

/*
 * The SDCard specifications are out of whack. Seriously. I've used a bunch of ressources to write
 * that source code. Here's a list of references I used:
 *
 * http://users.ece.utexas.edu/~valvano/EE345M/SD_Physical_Layer_Spec.pdf
 * http://alumni.cs.ucr.edu/~amitra/sdcard/Additional/sdcard_appnote_foust.pdf
 * http://elm-chan.org/docs/mmc/mmc_e.html
 *
 * Now, funnily enough, most of these informations (plus various other sources codes I've read)
 * contain contradictory information. So at the end, I'm not really sure that code fully works
 * everywhere. If you end up using that code, and stumble upon an sdcard that doesn't work, please
 * let me know and potentially send me said sdcard, so I can correct the code.
 *
 */

enum {
    GO_IDLE_STATE = 0,
    SEND_OP_COND = 1,
    SEND_IF_COND = 8,
    SEND_CSD = 9,
    SEND_CID = 10,
    STOP_TRANSMISSION = 12,
    SET_BLOCKLEN = 16,
    READ_SINGLE_BLOCK = 17,
    READ_MULTIPLE_BLOCK = 18,
    SET_BLOCK_COUNT = 23,
    WRITE_BLOCK = 24,
    WRITE_MULTIPLE_BLOCK = 25,
    READ_OCR = 58,

    APP_CMD = 55,

    APP_SEND_OP_COND = 41,
    SET_WR_BLOCK_ERASE_COUNT = 23,

    R1 = 0,
    R1B = 1,
    R2 = 2,
    R3 = 5,

    IDLE_STATE = 1 << 0,
    ERASE_RESET = 1 << 1,
    ILLEGAL_COMMAND = 1 << 2,
    COMMAND_CRC_ERROR = 1 << 3,
    ERASE_SEQUENCE_ERROR = 1 << 4,
    ADDRESS_ERROR = 1 << 5,
    PARAMETER_ERROR = 1 << 6,
};

static int sdcard_cmd(sdcard_t * sdcard, int cmd, uint8_t arg[4], int response_type, uint8_t response[5]) {
    // from http://elm-chan.org/docs/mmc/mmc_e.html
    // and http://elm-chan.org/docs/mmc/gx1/cmd.png
    BoardConsolePrintf("  sending cmd %02x\n", cmd);
    gpio_set(sdcard->cs, 0);
    ssp_write(sdcard->ssp, (cmd & 0x3f) | 0x40);
    int i;
    for (i = 0; i < 4; i++)
        ssp_write(sdcard->ssp, arg[i]);
    ssp_write(sdcard->ssp, 0x95); // supposed to be dummy CRC; but not during the very first command.
    // The first command is GO_IDLE_STATE (CMD0) with arg = 0. 0x95 is the CRC for such command.
    // See page 6 of http://alumni.cs.ucr.edu/~amitra/sdcard/Additional/sdcard_appnote_foust.pdf

    int retries = MAX_RETRIES;
    while (--retries) {
        response[0] = ssp_read(sdcard->ssp);
        if ((response[0] & 0x80) == 0)
            break;
        if (response[0] != 0xff)
            BoardConsolePrintf("Got odd idle byte: %02x\n", response[0]);
    }

    if (retries == 0) {
        BoardConsolePrintf("Timeout while waiting for response (got %02x).\n", response[0]);
        sdcard->got_timeout = 1;
        gpio_set(sdcard->cs, 1);
        return 0;
    }

    for (i = 1; i < response_type - 1; i++)
        response[i] = ssp_read(sdcard->ssp);

    if (response_type == R1B) {
        BoardConsolePuts("Waiting...");
        retries = MAX_RETRIES;
        while (--retries)
            (void) ssp_read(sdcard->ssp);
        if (retries == 0) {
            sdcard->got_timeout = 1;
            gpio_set(sdcard->cs, 1);
            return 0;
        }
        ssp_write(sdcard->ssp, 0xff);
    }

    gpio_set(sdcard->cs, 1);
    return 1;
}

static void error_out(sdcard_t * sdcard) {
    gpio_set(sdcard->cs, 1);
    ssp_write(sdcard->ssp, 0xff);
    sdcard->error_state = 1;
}

int sdcard_init(sdcard_t * sdcard) {
    int i;

    BoardConsolePuts("Starting sdcard init..");

    sdcard->got_timeout = 0;
    sdcard->error_state = 0;
    sdcard->v2 = 0;
    sdcard->ccs = 0;

    BoardConsolePuts("Configuring SSP.");
    ssp_config(sdcard->ssp, 400000); // standard says we can ping the card at 400khz first.
    BoardConsolePuts("Configuring CS.");
    gpio_config(sdcard->cs, pin_dir_write);
    gpio_set(sdcard->cs, 1);

    // from http://elm-chan.org/docs/mmc/gx1/sdinit.png
    // and http://alumni.cs.ucr.edu/~amitra/sdcard/Additional/sdcard_appnote_foust.pdf
    // first, let's send 74 dummy clocks, with MOSI = 1 and CS = 1; that's about 10 bytes equal to 0xff.
    BoardConsolePuts("Waiting 74+ cycles.");
    for (i = 0; i < 10; i++)
        ssp_write(sdcard->ssp, 0xff);

    BoardConsolePuts("Waiting 16+ cycles.");
    // then, we need 16 dummy clocks, with MOSI = 1 and CS = 0. Two bytes.
    gpio_set(sdcard->cs, 0);
    for (i = 0; i < 2; i++)
        ssp_write(sdcard->ssp, 0xff);

    uint8_t argument[4] = { 0, 0, 0, 0 };
    uint8_t response[5];

    // the card ought to be ready for us to send things now; let's start with tuning on SPI mode
    BoardConsolePuts("Sending GO_IDLE_STATE command.");
    if (!sdcard_cmd(sdcard, GO_IDLE_STATE, argument, R1, response)) {
        BoardConsolePuts("  --> timeouted.");
        error_out(sdcard);
        return 0;
    }
    BoardConsolePrintf("  --> %02x\n", response[0]);
    if (response[0] != IDLE_STATE) {
        BoardConsolePuts("Not in idle state...\n");
        error_out(sdcard);
        return 0;
    }

    BoardConsolePuts("Sending SEND_IF_COND command to see if this is a v2 card.");
    argument[0] = 0;
    argument[1] = 0;
    argument[2] = 1;
    argument[3] = 0xaa;
    if (!sdcard_cmd(sdcard, SEND_IF_COND, argument, R3, response)) {
        BoardConsolePuts("  --> timeouted.");
        error_out(sdcard);
        return 0;
    }

    BoardConsolePrintf("  --> %02x\n", response[0]);
    if ((response[0] & IDLE_STATE) != IDLE_STATE) {
        BoardConsolePuts("Not in idle state...\n");
        error_out(sdcard);
        return 0;
    }

    if ((response[0] & ILLEGAL_COMMAND) != ILLEGAL_COMMAND) {
        // The card accepted the command, so it's a v2.
        BoardConsolePuts("Card is a v2");
        sdcard->v2 = 1;
    }

    int retries = MAX_RETRIES;

    BoardConsolePuts("Initializing the SDCard by sending APP_SEND_OP_COND");
    while (--retries) {
        argument[0] = 0;
        argument[1] = 0;
        argument[2] = 0;
        argument[3] = 0;
        // We need to send APP_SEND_OP_COND, which needs APP_CMD before.
        BoardConsolePuts("Sending APP_CMD.");
        if (!sdcard_cmd(sdcard, APP_CMD, argument, R1, response)) {
            BoardConsolePuts("  --> timeouted.");
            error_out(sdcard);
            return 0;
        }
        BoardConsolePrintf("  --> %02x\n", response[0]);
        argument[0] = 0;
        argument[1] = 0;
        argument[2] = 0;
        argument[3] = sdcard->v2 ? 0x40 : 0;
        BoardConsolePuts("Sending APP_SEND_OP_COND.");
        if (!sdcard_cmd(sdcard, APP_SEND_OP_COND, argument, R3, response)) {
            BoardConsolePuts("  --> timeouted.");
            error_out(sdcard);
            return 0;
        }
        BoardConsolePrintf("  --> %02x %02x %02x %02x %02x\n", response[0], response[1], response[2], response[3], response[4]);
        // if bit 0 is set, card is busy, so we need to loop again
        if (((response[0] & IDLE_STATE) == 0) && (response[1] & 0x80)) // this is supposed to mean the power up routine is done
            break;
    }

    if (retries == 0) {
        BoardConsolePuts("Gave up initializing the sdcard");
        sdcard->got_timeout = 1;
        error_out(sdcard);
        return 0;
    }

    if (response[0] != 0) {
        BoardConsolePuts("Bad response to APP_SEND_OP_COND");
        error_out(sdcard);
        return 0;
    }

    // We're in fact supposed to retry using the SEND_OP_COND message instead of erroring out, but I'd
    // like to get an actual sdcard with these requirements to properly test it. In the meantime, I'll
    // just error out. It seems to be the "MMC V3" sdcard btw.

    if (sdcard->v2) {
        int retries = MAX_RETRIES;
        // check the OCR information now
        BoardConsolePuts("Sending READ_OCR");
        while (--retries) {
            argument[0] = 0;
            argument[1] = 0;
            argument[2] = 0;
            argument[3] = 0;
            if (!sdcard_cmd(sdcard, READ_OCR, argument, R3, response)) {
                BoardConsolePuts("  --> timeouted.");
                error_out(sdcard);
                return 0;
            }
            BoardConsolePrintf("  --> %02x %02x %02x %02x %02x\n", response[0], response[1], response[2], response[3], response[4]);
            if (((response[0] & IDLE_STATE) == 0) && (response[1] & 0x80)) // same as above
                break;
        }

        if (retries == 0) {
            BoardConsolePuts("Gave up initializing the sdcard");
            sdcard->got_timeout = 1;
            error_out(sdcard);
            return 0;
        }

        sdcard->ccs = response[1] & 0x40 ? 1 : 0;
    }

    BoardConsolePrintf("SDCard init success. v2 = %s, ccs = %s\n", sdcard->v2 ? "true" : "false", sdcard->ccs ? "true" : "false");

    return 1;
}
 