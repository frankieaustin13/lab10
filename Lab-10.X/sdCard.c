#include "mcc_generated_files/mcc.h"
#include "sdCard.h"

#pragma warning disable 373

//--------------------------------------------------------------
// http://elm-chan.org/docs/mmc/mmc_e.html
//--------------------------------------------------------------

//--------------------------------------------------------------
//--------------------------------------------------------------

void SDCARD_ReadBlock(uint32_t addr, uint8_t sdCardBuffer[]) {

    uint16_t i = 0;

    CS_SetLow();
    SPI2_ExchangeByte(0xFF);
    SPI2_ExchangeByte(0x40 | CMD_READ_BLOCK);
    SPI2_ExchangeByte((uint8_t) ((addr >> 24) & 0xFF));
    SPI2_ExchangeByte((uint8_t) ((addr >> 16) & 0xFF));
    SPI2_ExchangeByte((uint8_t) ((addr >> 8) & 0xFF));
    SPI2_ExchangeByte((uint8_t) (addr & 0xFF));
    SPI2_ExchangeByte(0xFF);

    // Wait for R1 response
    while (SPI2_ExchangeByte(0xFF) == 0xFF);

    // Wait for start token at the start of the data packet
    while (SPI2_ExchangeByte(0xFF) == 0xFF);

    // Read in one block, 512 bytes
    for (i = 0; i < 512; i++)
        sdCardBuffer[i] = SPI2_ExchangeByte(0xFF);

    // Finally chew up the 2-byte CRC
    SPI2_ExchangeByte(0xFF);
    SPI2_ExchangeByte(0xFF);

    CS_SetHigh();

}


//--------------------------------------------------------------
//--------------------------------------------------------------

void SDCARD_WriteBlock(uint32_t addr, uint8_t sdCardBuffer[]) {

    uint16_t i = 0;

    CS_SetLow();

    SPI2_ExchangeByte(0xFF);
    SPI2_ExchangeByte(0x40 | CMD_WRITE_BLOCK);
    SPI2_ExchangeByte((uint8_t) ((addr >> 24) & 0xFF));
    SPI2_ExchangeByte((uint8_t) ((addr >> 16) & 0xFF));
    SPI2_ExchangeByte((uint8_t) ((addr >> 8) & 0xFF));
    SPI2_ExchangeByte((uint8_t) (addr & 0xFF));
    SPI2_ExchangeByte(0xFF);

    // Wait for R1 response
    while (SPI2_ExchangeByte(0xFF) == 0xFF);

    // Send at least one byte to buffer data packet
    SPI2_ExchangeByte(0xFF);
    SPI2_ExchangeByte(0xFF);
    SPI2_ExchangeByte(0xFF);

    // Start data packet with a 1 byte data token
    SPI2_ExchangeByte(START_TOKEN);

    // followed by a 512 byte data block
    for (i = 0; i < 512; i++)
        SPI2_ExchangeByte(sdCardBuffer[i]);

    CS_SetHigh();

}


//--------------------------------------------------------------
// 
//--------------------------------------------------------------

uint8_t SDCARD_PollWriteComplete(void) {

    uint8_t status;

    CS_SetLow();
    status = SPI2_ExchangeByte(CMD_SEND_STATUS);
    CS_SetHigh();

    if (status == 0xFF) {
        return (WRITE_NOT_COMPLETE);
    } else {
        // REad out all 32-bits
        (void) SPI2_ExchangeByte(0xFF);
        (void) SPI2_ExchangeByte(0xFF);
        (void) SPI2_ExchangeByte(0xFF);
        return (status);
    }

}


//--------------------------------------------------------------
// 
//--------------------------------------------------------------

void SDCARD_Initialize(uint8_t verbose) {

    uint8_t response;

    //--------------------------------------------------------------
    // Send at least 74 clock cycles to SD card to tell it that we want
    // to communicate using SPI
    for (int i = 0; i < 10; i++) {
        SPI2_ExchangeByte(0xFF);
    }

    //--------------------------------------------------------------
    // Put in IDLE state to reset the SD Card processor state
    CS_SetLow();
    SPI2_ExchangeByte(0xFF);
    SPI2_ExchangeByte(0x40 | CMD_GO_IDLE_STATE);
    SPI2_ExchangeByte(0x00);
    SPI2_ExchangeByte(0x00);
    SPI2_ExchangeByte(0x00);
    SPI2_ExchangeByte(0x00);
    SPI2_ExchangeByte(0x95);
    SPI2_ExchangeByte(0xFF);
    response = SPI2_ExchangeByte(0xFF);
    CS_SetHigh();
    if (verbose == true) printf("CMD0, Reset Response: %x\r\n", response);

    //--------------------------------------------------------------
    // Put in OPERATION CONDITION state to run initialization sequence
    do {
        CS_SetLow();
        SPI2_ExchangeByte(0xFF);
        SPI2_ExchangeByte(0x40 | CMD_SEND_OP_COND);
        SPI2_ExchangeByte(0x00);
        SPI2_ExchangeByte(0x00);
        SPI2_ExchangeByte(0x00);
        SPI2_ExchangeByte(0x00);
        SPI2_ExchangeByte(0xFF);
        SPI2_ExchangeByte(0xFF);
        response = SPI2_ExchangeByte(0xFF);
        CS_SetHigh();
    } while (response != 0);
    if (verbose == true) printf("CMD1, Init Response: %x\r\n", response);

    //--------------------------------------------------------------
    // Define the number of bytes per block to equal 512
    do {
        CS_SetLow();
        SPI2_ExchangeByte(0xFF);
        SPI2_ExchangeByte(0x40 | CMD_SET_BLOCK_LENGTH);
        SPI2_ExchangeByte(0x00);
        SPI2_ExchangeByte(0x00);
        SPI2_ExchangeByte(0x02);
        SPI2_ExchangeByte(0x00);
        SPI2_ExchangeByte(0xFF);
        SPI2_ExchangeByte(0xFF);
        response = SPI2_ExchangeByte(0xFF);
        CS_SetHigh();
    } while (response == 0xFF);
    if (verbose == true) printf("Block Length Response: %x\r\n", response);

}

//--------------------------------------------------------------
// 
//--------------------------------------------------------------

void hexDumpBuffer(uint8_t sdCardBuffer[]) {

    uint16_t i;

    printf("\r\n\n");

    for (i = 0; i < 512; i++) {
        if (i != 0 && i % 8 == 0) printf(" ");
        if (i != 0 && i % 16 == 0) {
            printf("  ");
            for (int j = i - 16; j < i; j++) {
                if (sdCardBuffer[j] < 32) {
                    printf(".");
                } else {
                    printf("%c", sdCardBuffer[j]);
                }
            }
            printf("\r\n");
        }

        printf("%02x ", sdCardBuffer[i]);
    }

    printf("   ");
    for (int j = i - 16; j < i; j++) {
        if (sdCardBuffer[j] < 32) {
            printf(".");
        } else {
            printf("%c", sdCardBuffer[j]);
        }
    }
    printf("\r\n");
}

