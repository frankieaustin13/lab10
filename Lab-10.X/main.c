//--------------------------------------------------------------------
// Name:            Frankie Austin, Zachary Gamble
// Date:            Fall 2020
// Purpose:         Lab 09
//
// Assisted by:     Microchips 18F26K22 Tech Docs, instructions 
//-
//- Academic Integrity Statement: I certify that, while others may have
//- assisted me in brain storming, debugging and validating this program,
//- the program itself is my own work. I understand that submitting code
//- which is the work of other individuals is a violation of the course
//- Academic Integrity Policy and may result in a zero credit for the
//- assignment, or course failure and a report to the Academic Dishonesty
//- Board. I also understand that if I knowingly give my original work to
//- another individual that it could also result in a zero credit for the
//- assignment, or course failure and a report to the Academic Dishonesty
//- Board.
//------------------------------------------------------------------------
#include "mcc_generated_files/mcc.h"
#include "sdCard.h"
#pragma warning disable 520     // warning: (520) function "xyz" is never called  3
#pragma warning disable 1498    // fputc.c:16:: warning: (1498) pointer (unknown)

void myTMR0ISR(void);
void printAddress(uint32_t address);
uint32_t incrementAddress(uint32_t sdCardAddress);
void printAscii();

typedef enum  {IDLE, PLAY_AWAIT_BUFFER, PLAYBACK, MIC_AWAIT_BUFFER, MIC_ACQUIRE} myTMR0states_t;

#define BLOCK_SIZE          512
#define RATE                1600
#define MAX_NUM_BLOCKS      4
#define NUM_WRITE_FAILURES  128

// Large arrays need to be defined as global even though you may only need to 
// use them in main.  This quirk will be important in the next two assignments.
uint8_t sdCardBuffer[BLOCK_SIZE];
uint8_t sdCardBuffer1[BLOCK_SIZE];
uint8_t sdCardBuffer2[BLOCK_SIZE];
uint32_t writeFailureTable[NUM_WRITE_FAILURES]; // store previous n write failures
uint8_t writeFailureStatus[NUM_WRITE_FAILURES];
const uint8_t sin[] = {128,	159,	187,	213,	233,	248,	255,	255,	248,	233,	213,	187,	159,	128,	97,	69,	43,	23,	8,	1,	1,	8,	23,	43,	69,	97};
#define SINE_WAVE_ARRAY_LENGTH sizeof(sin)

uint8_t buffer1Full = false;
uint8_t buffer2Full = false;
uint8_t startCollect = false;
uint8_t stopCollect = false;
uint8_t collecting = false;
uint8_t stopPlayback = false;
uint8_t doPlayback = false;
uint16_t sampleRate = 1600;   


//----------------------------------------------
// Main "function"
//----------------------------------------------

void main(void) {

    uint8_t status;
    uint16_t i;
    uint32_t sdCardAddress = 0x00000000;
    uint32_t writeStartAddress = 0x00000000;
    uint32_t writeEndAddress = 0x00000000;
    char cmd, letter;

    letter = '0';

    SYSTEM_Initialize();
    CS_SetHigh();

    // Provide Baud rate generator time to stabilize before splash screen
    TMR0_WriteTimer(0x0000);
    INTCONbits.TMR0IF = 0;
    while (INTCONbits.TMR0IF == 0);

    TMR0_SetInterruptHandler(myTMR0ISR);

    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();

    printAscii();
    
    printf("Lab 10\r\n");
    printf("SD card + playback\r\n");
    printf("Dev'21\r\n");
    printf("No configuration of development board\r\n"); // print a nice command prompt    
    
    SPI2_Close();
    SPI2_Open(SPI2_DEFAULT);
    
    for (;;) {
        printf("> "); // print a nice command prompt
        while (!EUSART1_DataReady);// wait for incoming data on USART
        cmd = EUSART1_Read();
        printf("%c\r\n", cmd);
        switch (cmd) { // and do what it tells you to do

                //--------------------------------------------
                // Reply with help menu
                //--------------------------------------------
            case '?':
                printf("\r\n-------------------------------------------------\r\n");
                printf("\tPlay length in blocks: %d\r\n", (writeEndAddress-writeStartAddress)>>9);
                printf("\tsdCardAddress: ");
                printf("%04x", sdCardAddress >> 16);
                printf(":");
                printf("%04x", sdCardAddress & 0X0000FFFF);
                printf("\r\n");
                printf("\tsample rate: %dus\r\n", sampleRate / 16);
                printf("-------------------------------------------------\r\n");
                printf("?: help menu\r\n");
                printf("o: k\r\n");
                printf("Z: Reset processor\r\n");
                printf("z: Clear the terminal\r\n");
                printf("-------------------------------------------------\r\n");
                printf("i: Initialize SD card\r\n");
                printf("-------------------------------------------------\r\n");
                printf("a/A decrease/increase read address\r\n");
                printf("r: read a block of %d bytes from SD card\r\n", BLOCK_SIZE);
                printf("1: write a perfect 26 value sine wave to the SD card\r\n", BLOCK_SIZE);
                printf("-------------------------------------------------\r\n");
                printf("+/- Increase/Decrease the sample rate by 10 us\r\n");
                printf("W: Write microphone => SD card at 1600 us\r\n");
                printf("s: spool memory to a csv file\r\n");
                printf("-------------------------------------------------\r\n");
                break;

                //--------------------------------------------
                // Reply with "k", used for PC to PIC test
                //--------------------------------------------
            case 'o':
                printf("o:	ok\r\n");
                break;

                //--------------------------------------------
                // Reset the processor after clearing the terminal
                //--------------------------------------------                      
            case 'Z':
                for (i = 0; i < 40; i++) printf("\n");
                RESET();
                break;

                //--------------------------------------------
                // Clear the terminal
                //--------------------------------------------                      
            case 'z':
                for (i = 0; i < 40; i++) printf("\n");
                break;
                
            case '1': {
                static uint8_t sinIndex = 0;
                static uint32_t writeAddress = 0x00000000; 
                writeStartAddress = sdCardAddress;
                
                //printAddress(writeAddress);
                
                //for (uint8_t j = 0; j < 128 && !EUSART1_DataReady; j++){ 
                    for (uint16_t i = 0; i < BLOCK_SIZE; i++) {
                        sdCardBuffer[i] = sin[sinIndex];
                        if (++sinIndex >= SINE_WAVE_ARRAY_LENGTH)
                            sinIndex = 0;
                    }
                    //do {
                        SDCARD_WriteBlock(writeAddress, sdCardBuffer);
                        while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                    //} while (status != 5);
                    //printf("%X\r", j);
                    writeAddress = incrementAddress(writeAddress); 
                //}
                //if (EUSART1_DataReady)
                    //EUSART1_Read();
                                
                writeEndAddress = writeAddress;
  
                printf("Write block sin wave values:\r\n");
                printf("Amount of blocks stored: %d\r\n", (writeEndAddress - writeStartAddress)>>9); 
                printAddress(writeStartAddress);
                printAddress(writeEndAddress);
                printf("    Status:     %02x\r\n", status);
            }                                
                break;
            
            case '+': 
                sampleRate = sampleRate + 160; 
                break; 
                    
            case '-': 
                if (sampleRate > 320 ) { 
                    sampleRate = sampleRate - 160; 
                } else { 
                    sampleRate = sampleRate; 
                }
                break; 
                
            case 'P': {
                uint32_t readAddress = writeStartAddress;
                buffer1Full = false;
                buffer2Full = false;
                doPlayback = true;
                while (!EUSART1_DataReady && readAddress != writeEndAddress) {
                    while (buffer1Full);
                    SDCARD_ReadBlock(readAddress, sdCardBuffer1);
                    buffer1Full = true;
                    readAddress = incrementAddress(readAddress);
                    if (readAddress == writeEndAddress)
                        break;
                    while (buffer2Full);
                    SDCARD_ReadBlock(readAddress, sdCardBuffer2);
                    buffer2Full = true;
                    readAddress = incrementAddress(readAddress);
                }
                stopPlayback = true;
                if (EUSART1_DataReady) {
                    EUSART1_Read();
                }
                while (doPlayback); // wait for it to finish
            }
            break;
            
            case 'W': {
                printf("Press any key to start recording audio and press any key to stop recording.");
                while (!EUSART1_DataReady);
                EUSART1_Read(); // throw away key
                writeStartAddress = sdCardAddress;
                uint32_t writeAddress = sdCardAddress;
                uint8_t failureIndex = 0;
                startCollect = true;
                // enter loop; await key press or flag
                while (!EUSART1_DataReady) {
                    if (buffer1Full) {
                        SDCARD_WriteBlock(writeAddress, sdCardBuffer1);
                        while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        if ((status & 0x1F) != 0x5) {
                            SDCARD_WriteBlock(writeAddress, sdCardBuffer1);
                            while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        }
                        buffer1Full = false;
                        if ((status & 0x1F) != 0x5 && failureIndex < NUM_WRITE_FAILURES) {
                            writeFailureTable[failureIndex] = writeAddress;
                            writeFailureStatus[failureIndex] = status;
                            failureIndex++;
                        }
                        writeAddress = incrementAddress(writeAddress);
                    }
                    if (buffer2Full) {
                        SDCARD_WriteBlock(writeAddress, sdCardBuffer2);
                        while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        if ((status & 0x1F) != 0x5) {
                            SDCARD_WriteBlock(writeAddress, sdCardBuffer2);
                            while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        }
                        buffer2Full = false;
                        if ((status & 0x1F) != 0x5 && failureIndex < NUM_WRITE_FAILURES) {
                            writeFailureTable[failureIndex] = writeAddress;
                            writeFailureStatus[failureIndex] = status;
                            failureIndex++;
                        }
                        writeAddress = incrementAddress(writeAddress);                        
                    }
                }
                EUSART1_Read(); // discard
                stopCollect = true;
                // wait until the ISR is finished (should gather last block of data)
                while (buffer1Full || buffer2Full || collecting) {
                    if (buffer1Full) {
                        SDCARD_WriteBlock(writeAddress, sdCardBuffer1);
                        while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        if ((status & 0x1F) != 0x5) {
                            SDCARD_WriteBlock(writeAddress, sdCardBuffer1);
                            while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        }
                        buffer1Full = false;
                        if ((status & 0x1F) != 0x5 && failureIndex < NUM_WRITE_FAILURES) {
                            writeFailureTable[failureIndex] = writeAddress;
                            writeFailureStatus[failureIndex] = status;
                            failureIndex++;
                        }
                        writeAddress = incrementAddress(writeAddress);
                    }
                    if (buffer2Full) {
                        SDCARD_WriteBlock(writeAddress, sdCardBuffer2);
                        while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        if ((status & 0x1F) != 0x5) {
                            SDCARD_WriteBlock(writeAddress, sdCardBuffer2);
                            while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        }
                        buffer2Full = false;
                        if ((status & 0x1F) != 0x5 && failureIndex < NUM_WRITE_FAILURES) {
                            writeFailureTable[failureIndex] = writeAddress;
                            writeFailureStatus[failureIndex] = status;
                            failureIndex++;
                        }
                        writeAddress = incrementAddress(writeAddress);
                    }
                }
                writeEndAddress = writeAddress;
                printf ("\r\n");
                if (failureIndex > 0) {
                    printf ("Listing write failures: \r\n");
                    for (uint8_t i = 0; i < failureIndex; i++) {
                        uint32_t addr = writeFailureTable[i];
                        printf("Status code: %d ---- Address: ", writeFailureStatus[i]);
                        printf("%04x", addr >> 16);
                        printf(":");
                        printf("%04x\r\n", addr & 0X0000FFFF);
                    }
                }
            }
                break;
                
            case 's':
                
                printf("You may terminate spooling at anytime with a key press.\r\n");
                printf("To spool terminal contents into a file follow these instructions:\r\n");
                printf("\r\n"); 
                printf("Right mouse click on the upper left of the PuTTY window\r\n"); 
                printf("Select:     Change settings...\r\n"); 
                printf("Select:     Logging\r\n"); 
                printf("Select:     Session logging: All session output\r\n"); 
                printf("Log file name: Browse and provide a .csv extension to your file name\r\n");
                printf("Select:     Apply\r\n"); 
                printf("Press any key to start\r\n");
                
                while (!EUSART1_DataReady);
                EUSART1_Read(); // throw away key
                
                uint32_t address = writeStartAddress;
                uint32_t endAddress = writeEndAddress;
                uint32_t numBlocks = (endAddress - address) >> 9;
                uint32_t printedBlocks = 0;
                
                while (!EUSART1_DataReady && address != endAddress) {
                    SDCARD_ReadBlock(address, sdCardBuffer);
                    address = incrementAddress(address);

                    for (uint16_t i = 0; i != BLOCK_SIZE; i++) {
                        uint16_t printVal = sdCardBuffer[i];
                        printf("%d\r\n", printVal);
                    }
                    printedBlocks++;
                }
                if (EUSART1_DataReady)
                    EUSART1_Read();
               
                printf("Spooled %u out of the %u blocks.\r\n", printedBlocks, numBlocks); 
                printf("To close the file follow these instructions: \r\n");          
                printf("Right mouse click on the upper left of the PuTTY window\r\n"); 
                printf("Select:     Change settings...\r\n"); 
                printf("Select:     Logging\r\n"); 
                printf("Select:     Session  logging: None\r\n"); 
                printf("Select:     Apply \r\n");        
          
                break; 

 
                //-------------------------------------------- 
                // Init SD card to get it read to perform read/write
                // Will hang in infinite loop on error.
                //--------------------------------------------    
            case 'i':
                SPI2_Close();
                SPI2_Open(SPI2_DEFAULT);    // Reset the SPI channel for SD card communication
                SDCARD_Initialize(true);
                break;

                //--------------------------------------------
                // Increase or decrease block address
                //--------------------------------------------                 
            case 'A':
            case 'a':
                if (cmd == 'a') {
                    sdCardAddress -= BLOCK_SIZE;
                    if (sdCardAddress >= 0x04000000) {
                        printf("Underflowed to high address\r\n");
                        sdCardAddress = 0x04000000 - BLOCK_SIZE;
                    } else {
                        printf("Decreased address\r\n");
                    }
                } else {
                    sdCardAddress += BLOCK_SIZE;
                    if (sdCardAddress >= 0x04000000) {
                        printf("Overflowed to low address\r\n");
                        sdCardAddress = 0x00000000;
                    } else {
                        printf("Increased address\r\n");
                    }
                }

                // 32-bit integers need printed as a pair of 16-bit integers
                printf("SD card address:  ");
                printf("%04x", sdCardAddress >> 16);
                printf(":");
                printf("%04x", sdCardAddress & 0X0000FFFF);
                printf("\r\n");
                break;

                //--------------------------------------------
                // r: read a block of BLOCK_SIZE bytes from SD card                
                //--------------------------------------------
            case 'r':
                SDCARD_ReadBlock(sdCardAddress, sdCardBuffer);
                printf("Read block: \r\n");
                printAddress(sdCardAddress);
                hexDumpBuffer(sdCardBuffer);
                break;

                //--------------------------------------------
                // If something unknown is hit, tell user
                //--------------------------------------------
            default:
                printf("Unknown key %c\r\n", cmd);
                break;
        } // end switch
    } // end while 
} // end main

uint32_t incrementAddress(uint32_t sdCardAddress)
{
    sdCardAddress += BLOCK_SIZE;
    if (sdCardAddress >= 0x04000000) 
        sdCardAddress = 0x00000000;
    return sdCardAddress;
}

uint32_t decrementAddress(uint32_t sdCardAddress) {
    sdCardAddress -= BLOCK_SIZE;    
    if (sdCardAddress >= 0x04000000) 
        sdCardAddress = 0x04000000 - BLOCK_SIZE;
    return sdCardAddress;
}

void printAddress(uint32_t address)
{
    printf("    Address:    ");
    printf("%04x", address >> 16);
    printf(":");
    printf("%04x", address & 0X0000FFFF);
    printf("\r\n");
}

//----------------------------------------------
// As configured, we are hoping to get a toggle
// every 100us - this will require some work.
//
// You will be starting an ADC conversion here and
// storing the results (when you reenter) into a global
// variable and setting a flag, alerting main that 
// it can read a new value.
//
// !!!MAKE SURE THAT TMR0 has 0 TIMER PERIOD in MCC!!!!
//----------------------------------------------

void myTMR0ISR(void) {
    ADCON0bits.GO_NOT_DONE = 1; // start a new conversion
    // tell main that we have a new value
    TMR0_WriteTimer(0x10000 - (sampleRate - TMR0_ReadTimer()));
    static uint16_t bufferIndex = 0; 
    static myTMR0states_t state = IDLE; 
    static uint8_t *buffer = sdCardBuffer1;
    static uint8_t useBuffer1 = true;
   
    uint16_t mic = ADRESH; 
    
    switch(state) { 
        case IDLE: 
            if (startCollect) {
                collecting = true;
                stopCollect = false;
                state = MIC_ACQUIRE; 
                bufferIndex = 0; 
                buffer = sdCardBuffer1;
                useBuffer1 = true;
                startCollect = false;
            } else if (doPlayback) {
                useBuffer1 = true;
                stopPlayback = false;
                buffer = sdCardBuffer1;
                bufferIndex = 0;
                state = PLAYBACK;
            }
            break; 
        
        case PLAY_AWAIT_BUFFER:
            if ((useBuffer1 && buffer1Full) || (!useBuffer1 && buffer2Full)) {
                break; // keep waiting
            }
            // fallthrough
        case PLAYBACK:
            EPWM1_LoadDutyValue(buffer[bufferIndex++]); 
            
            if(bufferIndex == 512) {
                if (useBuffer1) {
                    buffer1Full = false;
                    buffer = sdCardBuffer2;
                    if (!buffer2Full)
                        state = MIC_AWAIT_BUFFER;
                } else {
                    buffer2Full = false;
                    buffer = sdCardBuffer1;
                    if (!buffer1Full)
                        state = MIC_AWAIT_BUFFER;
                }
                useBuffer1 = !useBuffer1;
                bufferIndex = 0;
                if (stopPlayback) {
                    stopPlayback = false;
                    state = IDLE;
                    doPlayback = false;
                }
            }
            break;

            // This state should never be encountered; but just in case
        case MIC_AWAIT_BUFFER:
            if ((useBuffer1 && buffer1Full) || (!useBuffer1 && buffer2Full)) {
                break; // keep waiting
            }
            // fall through
            
        case MIC_ACQUIRE: 
            buffer[bufferIndex++] = mic; 
            
            if(bufferIndex == 512) {
                if (useBuffer1) {
                    buffer1Full = true;
                    buffer = sdCardBuffer2;
                    if (buffer2Full)
                        state = MIC_AWAIT_BUFFER;
                } else {
                    buffer2Full = true;
                    buffer = sdCardBuffer1;
                    if (buffer1Full)
                        state = MIC_AWAIT_BUFFER;
                }
                useBuffer1 = !useBuffer1;
                bufferIndex = 0;
                if (stopCollect) {
                    stopCollect = false;
                    collecting = false;
                    state = IDLE;
                }
            }

            break; 
    }
} // end myTMR0ISR

void printAscii() {
    printf("...........................................................\r\n");
    printf(".....................?????????????????.....................\r\n");
    printf(".................?????????????????????????.................\r\n");
    printf("..............???????????????????????????????..............\r\n");
    printf("............???????????????????????????????????............\r\n");
    printf("..........???????????????????????????????????????..........\r\n");
    printf(".........?????????????????????????????????????????.........\r\n");
    printf(".......?????????????????????????????????????????????.......\r\n");
    printf("......???????????????????????????????????????????????......\r\n");
    printf(".....?????????????????????????????????????????????????.....\r\n");
    printf("....????:::::::::::?????????????????????:::::::::::????....\r\n");
    printf("...??::::::::::::::::::::?????????::::::::::::::::::::??...\r\n");
    printf("..???:::::::::::::::::::::::::::::::::::::::::::::::::???..\r\n");
    printf("..????::::::::::::::::::::::???::::::::::::::::::::::????..\r\n");
    printf(".??????::::::::::::::::::::?????:::::::::::::::::::::?????.\r\n");
    printf(".???????::::::::::::::::::???????:::::::::::::::::::??????.\r\n");
    printf(".???????:::::::::::::::::?????????::::::::::::::::::??????.\r\n");
    printf(".????????:::::::::::::::???????????:::::::::::::::????????.\r\n");
    printf(".??????????::::::::::?????????????????::::::::::??????????.\r\n");
    printf("..???????????????????????????????????????????????????????..\r\n");
    printf("..???????????????????????????????????????????????????????..\r\n");
    printf("...??????????????????????????????????????????????????????..\r\n");
    printf("...?????????????:::?????????????????????:::?????????????...\r\n");
    printf("....?????????????:::???????????????????:::??????????????...\r\n");
    printf("....??????????????:::?????????????????:::??????????????....\r\n");
    printf(".....??????????????::::?????????????::::??????????????.....\r\n");
    printf("......???????????????:::::???????:::::???????????????......\r\n");
    printf(".......?????????????????:::::::::::?????????????????.......\r\n");
    printf(".........?????????????????????????????????????????.........\r\n");
    printf("..........???????????????????????????????????????..........\r\n");
    printf("............???????????????????????????????????............\r\n");
    printf("..............???????????????????????????????..............\r\n");
    printf(".................?????????????????????????.................\r\n");
    printf(".....................?????????????????.....................\r\n");
    printf("...........................................................\r\n");
}
