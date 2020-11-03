#include "mcc_generated_files/mcc.h"
#include "sdCard.h"
#pragma warning disable 520     // warning: (520) function "xyz" is never called  3
#pragma warning disable 1498    // fputc.c:16:: warning: (1498) pointer (unknown)

void myTMR0ISR(void);
uint32_t incrementAddress(uint32_t sdCardAddress);
void printAscii();

typedef enum  {MIC_IDLE, MIC_AWAIT_BUFFER, MIC_ACQUIRE} myTMR0states_t;

#define BLOCK_SIZE          512
#define RATE                1600
#define MAX_NUM_BLOCKS      4

// Large arrays need to be defined as global even though you may only need to 
// use them in main.  This quirk will be important in the next two assignments.
uint8_t sdCardBuffer[BLOCK_SIZE];
uint8_t sdCardBuffer1[BLOCK_SIZE];
uint8_t sdCardBuffer2[BLOCK_SIZE];
const uint8_t sin[] = {128,	159,	187,	213,	233,	248,	255,	255,	248,	233,	213,	187,	159,	128,	97,	69,	43,	23,	8,	1,	1,	8,	23,	43,	69,	97};
#define SINE_WAVE_ARRAY_LENGTH sizeof(sin)

uint8_t buffer1Full = false;
uint8_t buffer2Full = false;
uint8_t fillBuffer1 = true;
uint8_t startCollect = false;
uint8_t stopCollect = false;
uint8_t collecting = false;
uint16_t sampleRate = 1600;   


//----------------------------------------------
// Main "function"
//----------------------------------------------

void main(void) {

    uint8_t status;
    uint16_t i;
    uint32_t sdCardAddress = 0x00000000;
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
    
    printf("Lab 09\r\n");
    printf("SD card testing\r\n");
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
                printf("SD card address:  ");
                printf("%04x", sdCardAddress >> 16);
                printf(":");
                printf("%04x", sdCardAddress & 0X0000FFFF);
                printf("\r\n");
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
                for (uint16_t i = 0; i < BLOCK_SIZE; i++) {
                    sdCardBuffer[i] = sin[sinIndex];
                    if (++sinIndex >= SINE_WAVE_ARRAY_LENGTH)
                        sinIndex = 0;
                }
                
                SDCARD_WriteBlock(sdCardAddress, sdCardBuffer);
                while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);

                printf("Write block sin wave values:\r\n");
                printf("    Address:    ");
                printf("%04x", sdCardAddress >> 16);
                printf(":");
                printf("%04x", sdCardAddress & 0X0000FFFF);
                printf("\r\n");
                printf("    Status:     %02x\r\n", status);
                
                sdCardAddress = incrementAddress(sdCardAddress);
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
            
            case 'W': {
                printf("Press any key to start recording audio and press any key to stop recording.");
                while (!EUSART1_DataReady);
                EUSART1_Read(); // throw away key
                // enter loop; await key press or flag
                while (!EUSART1_DataReady) {
                    if (buffer1Full) {
                        SDCARD_WriteBlock(sdCardAddress, sdCardBuffer1);
                        while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        buffer1Full = false;
                        sdCardAddress = incrementAddress(sdCardAddress);
                    }
                    if (buffer2Full) {
                        SDCARD_WriteBlock(sdCardAddress, sdCardBuffer2);
                        while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        buffer2Full = false;
                        sdCardAddress = incrementAddress(sdCardAddress);                        
                    }
                }
                EUSART1_Read(); // discard
                stopCollect = true;
                // wait until the ISR is finished (should gather last block of data)
                while (buffer1Full || buffer2Full || collecting) {
                    if (buffer1Full) {
                        SDCARD_WriteBlock(sdCardAddress, sdCardBuffer1);
                        while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        buffer1Full = false;
                        sdCardAddress = incrementAddress(sdCardAddress);
                    }
                    if (buffer2Full) {
                        SDCARD_WriteBlock(sdCardAddress, sdCardBuffer2);
                        while ((status = SDCARD_PollWriteComplete()) == WRITE_NOT_COMPLETE);
                        buffer2Full = false;
                        sdCardAddress = incrementAddress(sdCardAddress);
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
                
                SDCARD_ReadBlock(sdCardAddress , sdCardBuffer);
                sdCardAddress = incrementAddress(sdCardAddress);
                 
                uint16_t iterator = 0; 
                while ((!EUSART1_DataReady) && (iterator != BLOCK_SIZE)) {
                    uint16_t printVal = sdCardBuffer[iterator];
                    printf("%d\r\n", printVal);
                    iterator++; 
                }
                if (EUSART1_DataReady)
                    EUSART1_Read();
               
                printf("Spooled 512 out of the 512 blocks.\r\n"); 
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
                printf("    Address:    ");
                printf("%04x", sdCardAddress >> 16);
                printf(":");
                printf("%04x", sdCardAddress & 0X0000FFFF);
                printf("\r\n");
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
    static myTMR0states_t state = MIC_IDLE; 
    static uint8_t *buffer = sdCardBuffer1;
   
    uint8_t mic = ADRESH; 
    
    switch(state) { 
        case MIC_IDLE: 
            if (startCollect == true) {
                collecting = true;
                state = MIC_ACQUIRE; 
                bufferIndex = 0; 
                buffer = sdCardBuffer1;
                fillBuffer1 = true;
                startCollect = true;
            }
            break; 

            // This state should never be encountered; but just in case
        case MIC_AWAIT_BUFFER:
            if ((fillBuffer1 && buffer1Full) || (!fillBuffer1 && buffer2Full)) {
                break; // keep waiting
            }
            // fall through
            
        case MIC_ACQUIRE: 
            buffer[bufferIndex++] = mic; 
            
            if(bufferIndex == 512) {
                if (fillBuffer1) {
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
                fillBuffer1 = !fillBuffer1;
                bufferIndex = 0;
                if (stopCollect) {
                    collecting = false;
                    state = MIC_IDLE;
                }
            }

            break; 
    }
} // end myTMR0ISR

void printAscii() {
    printf("                                                                                \r\n");
    printf("                                  777777777777                                  \r\n");
    printf("                            7777IIIIIIIIIIIIII?I7777                            \r\n");
    printf("                        777IIII77777777777777777IIIII777                        \r\n");
    printf("                     77III7777IIIIIII?????IIIIIII7777II?777                     \r\n");
    printf("                  77III777III???+++++++++++++++???III7777I?777                  \r\n");
    printf("                77II777II???++++++++++++++++++++++++??II777III77                \r\n");
    printf("              77II777II?+++++++++++++++++++++++++++++++??II77III77              \r\n");
    printf("             7II77II??+++++++++++++++++++++++++++++++++++??II77I?77             \r\n");
    printf("           77II77I??++++++++++++++++++++++++++++++++++++++++?II77II77           \r\n");
    printf("          77I77II?+++++++++++++++++++++++++++++++++++++++++++??I77I?77          \r\n");
    printf("         7?I7II?+++++++++++++++++++++++++++++++++++++++++++++++?II7III7         \r\n");
    printf("        7?I7II?+++++++++++++++++++++++++++++++++++++++++++++++++?II7III7        \r\n");
    printf("       7?III??++++++++++++++++++++++++++++++++++++++++++++++++++++?I7III7       \r\n");
    printf("      7IIII?++=...........+++++++++++++++++++++++++++:..........=++?III?77      \r\n");
    printf("     77I+=~.........................:+++++~.........................:=+I?77     \r\n");
    printf("     7?I=~,.........................................................,~=III7     \r\n");
    printf("    7III~,...........................................................,~II?77    \r\n");
    printf("    7?II?+...........................................................+?III?7    \r\n");
    printf("   77?II?+...........................+++++...........................+??II?77   \r\n");
    printf("   7?II?+++..........................+++++..........................~++?II?I7   \r\n");
    printf("   7?I??+++.........................+++++++.........................+++??I??7   \r\n");
    printf("  77????+++~........................+++++++.........................++++???+77  \r\n");
    printf("  77???+++++.......................+++++++++.......................+++++????77  \r\n");
    printf("  7I???++++++.....................=++++++++++.....................++++++????I7  \r\n");
    printf("  7I???++++++=...................~++++++++++++....................++++++???+I7  \r\n");
    printf("  7I+?+++++++++.................+++++++++++++++.................+++++++++?+?I7  \r\n");
    printf("  77+++++++++++++............=++++++++++++++++++++............++++++++++++++77  \r\n");
    printf("   7+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++?7   \r\n");
    printf("   7+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++?7   \r\n");
    printf("   7I+++++++++++++..+++++++++++++++++++++++++++++++++++++++..+++++++++++=++I7   \r\n");
    printf("   77+==++++++++++,..+++++++++++++++++++++++++++++++++++++...++++++++++===?77   \r\n");
    printf("    7?===++++++++++...+++++++++++++++++++++++++++++++++++...++++++++++===+I7    \r\n");
    printf("    77=~~=++++++++++...+++++++++++++++++++++++++++++++++...++++++++++=~~=?77    \r\n");
    printf("     7I~~~=++++++++++:...+++++++++++++++++++++++++++++~...++++++++++=~~~+I7     \r\n");
    printf("      7?~:~=+++++++++++...+++++++++++++++++++++++++++...+++++++++++=~:~+I7      \r\n");
    printf("      77=:::=++++++++++++....+++++++++++++++++++++=...~+++++++++++=~::=?77      \r\n");
    printf("       77=:,:=+++++++++++++.....+++++++++++++++.....+++++++++++++=:::~?77       \r\n");
    printf("        77=:,:~++++++++++++++=...................:++++++++++++++~:,:~?77        \r\n");
    printf("         77?:,,:=+++++++++++++++++...........+++++++++++++++++=~,,:+?77         \r\n");
    printf("           7I~,,,~=++++++++++++++++++++++++++++++++++++++++++~:,,:+I7           \r\n");
    printf("            77?:,,:~=++++++++++++++++++++++++++++++++++++++~:,,:+?77            \r\n");
    printf("             77I~:,,:~=+++++++++++++++++++++++++++++++++=~:,,:~+I77             \r\n");
    printf("               77I~:,,,:~=+++++++++++++++++++++++++++=~:,,,:~+I77               \r\n");
    printf("                 77I?:,,,,:~==+++++++++++++++++++==~:,,,,:+?I77                 \r\n");
    printf("                   777?=::,,,,::~~===========~~:::,,,::~+?777                   \r\n");
    printf("                      777I?~:::,,,,,,,:,,,,,,,,,:::~+?I777                      \r\n");
    printf("                         7777I??+~~~::::::~~~~++??I7777                         \r\n");
    printf("                              777777IIIIIIII777777                              \r\n");
    printf("                                                                                \r\n");
    printf("                                                                                \r\n");
}
