/*
* SG2100N_CHIP_RESETTER.c
*
* Created: 28-08-2017 03:25:14
* Author: Wojciech Cybowski
* https://github.com/wcyb/cartridge_chip_resetter
*
*/

#define F_CPU 8000000UL //8MHz

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <util/delay.h>
#include "i2cmaster.h"

#define chipAddrC 0xA2 //address of the cyan gel chip
#define chipAddrM 0xA4 //address of the magenta gel chip
#define chipAddrY 0xA6 //address of the yellow gel chip
#define chipAddrB 0xA0 //address of the black gel chip
#define chipAddrW 0xA8 //address of the waste tank chip

#define chipDataSize 1
#define chipTypeSize 2

#define redLed 4
#define greenLed 1
#define blueLed 2
#define yellowLed 5
#define whiteLed 7
#define offLed 0

volatile bool startResetting = false; //if true then user pressed the button
bool chipTypeOk = false; //if true then we can start resetting
bool resettedOk = false; //if true then chip was resetted successfully
const uint8_t chipsAddr[] = {chipAddrC, chipAddrM, chipAddrY, chipAddrB, chipAddrW};
const uint8_t gelType[chipTypeSize] = {227, 18};
const uint8_t wasteType[chipTypeSize] = {227, 1};
const uint8_t dataToCheck[chipDataSize] = {100};
volatile uint8_t readData[chipDataSize] = {0};
volatile uint8_t readChipType[chipTypeSize] = {0};

uint8_t findChip(void); //searches for a gel or waste tank chip, returns a number from 1 to 5 in order C M Y B W, or 0 if a chip was not found
void writeZeros(uint8_t, uint8_t, uint8_t); //writes given number of zeros starting from given address
uint8_t checkZeros(uint8_t, uint8_t, uint8_t); //reads data and checks if all read bytes are 0
void writeOnes(uint8_t, uint8_t, uint8_t); //writes given number of ones starting from given address
uint8_t checkOnes(uint8_t, uint8_t, uint8_t); //reads data and checks if all read bytes are 1
void blinkLed(uint8_t, uint8_t); //arguments are blink type and error mode, 0 - error(error mode can be set from 1 to 3), 1 - cyan resetted, 2 - magenta, 3 - yellow, 4 - black, 5 - waste tank

int main(void)
{
    DDRB = 0x07; //set output for LED
    DDRD &= ~(1 << PIND2); //set input for the button
	PORTB = 0xF8; //pull-up unused pins to vcc
	PORTC = 0x3F; //pull-up
	PORTD = 0xFB; //pull-up
    //----------------------------------------------
    EICRA |= (1 << ISC01); //INT0 on falling edge
    EIMSK |= (1 << INT0); //enable INT0
    //----------------------------------------------
    i2c_init(); //initialize I2C library
    sei(); //enable interrupts

    while (1)
    {
        if (startResetting == true)
        {
            EIMSK &= ~(1 << INT0); //disable INT0 for the time of resetting
            uint8_t foundChip = findChip(); //search for the connected chip
            if (foundChip != 0) //chip was found
            {
                foundChip--; //subtract 1 because the function returns 0 when chip is not found, so all numbers are +1
                i2c_write(0x0);
                i2c_rep_start(chipsAddr[foundChip] + I2C_READ);
                readChipType[0] = i2c_readAck();
                readChipType[1] = i2c_readNak();
                i2c_stop();

                for (uint8_t i = 0; i < chipTypeSize; i++) //check if chip type matches
                {
                    if (readChipType[i] != gelType[i] && readChipType[i] != wasteType[i]) //check if read type don't matches any known value
                    {
                        chipTypeOk = false;
                        break;
                    }
                    chipTypeOk = true;
                }

                if (chipTypeOk == false)
                {
                    blinkLed(0, 2); //show that the chip type is wrong, stop resetting
                }
                else
                {
                    uint8_t zerosCheck = 0; //this var will indicate if needed data is 0's
                    uint8_t onesCheck = 0xFF; //this var will indicate if needed data is 1's
                    if (foundChip < 4) //if we reset a gel chip
                    {
                        writeZeros(chipsAddr[foundChip], 1, 0x06);
                        writeOnes(chipsAddr[foundChip], 1, 0x07);

                        i2c_start_wait(chipsAddr[foundChip] + I2C_WRITE); //set device address and write mode, repeated start
                        i2c_write(0x08); //initial ink level
                        i2c_write(100); //reset to full
                        i2c_stop();

                        writeZeros(chipsAddr[foundChip], 1, 0x09);
                        writeOnes(chipsAddr[foundChip], 6, 0x10);
                        writeOnes(chipsAddr[foundChip], 8, 0x18);
                        writeOnes(chipsAddr[foundChip], 1, 0x28);
                        writeZeros(chipsAddr[foundChip], 1, 0x29);
                        writeOnes(chipsAddr[foundChip], 22, 0x2A);
                        writeOnes(chipsAddr[foundChip], 11, 0x43);
                        writeOnes(chipsAddr[foundChip], 49, 0x4F);

                        //now check if data was written successfully
                        //read previously written values back from EEPROM
                        i2c_start_wait(chipsAddr[foundChip] + I2C_WRITE); //set device address and write mode
                        i2c_write(0x08); //initial ink level
                        i2c_rep_start(chipsAddr[foundChip] + I2C_READ); //set device address and read mode
                        readData[0] = i2c_readNak(); //read one byte from EEPROM
                        i2c_stop();

                        zerosCheck |= checkZeros(chipsAddr[foundChip], 1, 0x06);
                        zerosCheck |= checkZeros(chipsAddr[foundChip], 1, 0x09);
                        zerosCheck |= checkZeros(chipsAddr[foundChip], 1, 0x29);

                        onesCheck &= checkOnes(chipsAddr[foundChip], 1, 0x07);
                        onesCheck &= checkOnes(chipsAddr[foundChip], 6, 0x10);
                        onesCheck &= checkOnes(chipsAddr[foundChip], 8, 0x18);
                        onesCheck &= checkOnes(chipsAddr[foundChip], 1, 0x28);
                        onesCheck &= checkOnes(chipsAddr[foundChip], 22, 0x2A);
                        onesCheck &= checkOnes(chipsAddr[foundChip], 11, 0x43);
                        onesCheck &= checkOnes(chipsAddr[foundChip], 49, 0x4F);

                        for (uint8_t i = 0; i < chipDataSize; i++) //check if chip was resetted successfully
                        {
                            if (readData[i] != dataToCheck[i])
                            {
                                resettedOk = false;
                                break;
                            }
                            resettedOk = true;
                        }

                        if (resettedOk == true && zerosCheck == 0 && onesCheck == 0xFF) //also check if all needed data have the right value
                        {
                            blinkLed(foundChip + 1, 0); //resetting was successful (+1 because error is mode 0)
                        }
                        else
                        {
                            blinkLed(0, 3);
                        }
                    }
                    else //otherwise we reset the waste tank chip
                    {
                        writeZeros(chipsAddr[foundChip], 5, 0x04);
                        writeZeros(chipsAddr[foundChip], 74, 0x14);
                        writeZeros(chipsAddr[foundChip], 160, 0x5F);

                        //now check if data was written successfully
                        zerosCheck |= checkZeros(chipsAddr[foundChip], 5, 0x04);
                        zerosCheck |= checkZeros(chipsAddr[foundChip], 74, 0x14);
                        zerosCheck |= checkZeros(chipsAddr[foundChip], 160, 0x5F);

                        //we don't need to read any custom data from the chip

                        if (zerosCheck == 0) //also check if all needed data is zeroed
                        {
                            blinkLed(foundChip + 1, 0); //resetting was successful (+1 because error is mode 0)
                        }
                        else
                        {
                            blinkLed(0, 3);
                        }
                    }
                }
            }
            else //if something is wrong then blink
            {
                i2c_stop();
                blinkLed(0, 1); //error, 1 blink
            }
            EIMSK |= (1 << INT0); //enable INT0 again
            EIFR |= (1 << INTF0); //clear INT0 flag, this will eliminate additional reading after release of the button
            startResetting = false; //end resetting
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//Search for the chip
//////////////////////////////////////////////////////////////////////////
uint8_t findChip(void)
{
    for (uint8_t i = 0; i < 5; i++)
    {
        if (i2c_start(chipsAddr[i] + I2C_WRITE) == 0) { return i + 1; } //if we get 0 then we can connect to the chip, else end the search procedure
    }
    return 0; //if chip is not found then return 0
}

//////////////////////////////////////////////////////////////////////////
//Function writes given number of zeros starting from given address
//////////////////////////////////////////////////////////////////////////
void writeZeros(uint8_t chipAddr, uint8_t howMany, uint8_t whereStart)
{
    for (uint8_t i = 0; i < howMany; i++)
    {
        i2c_start_wait(chipAddr + I2C_WRITE); //set device address and write mode
        i2c_write(whereStart + i);
        i2c_write(0x0);
        i2c_stop(); //do it for every byte because of page write (8 bytes per page), so this way it will be shorter in code
    }
}

//////////////////////////////////////////////////////////////////////////
//Function checks if there is given number of zeros counting from given address
//////////////////////////////////////////////////////////////////////////
uint8_t checkZeros(uint8_t chipAddr, uint8_t howMany, uint8_t whereStart)
{
    uint8_t readByte = 0;

    i2c_start(chipAddr + I2C_WRITE); //set device address and write mode
    i2c_write(whereStart);
    i2c_rep_start(chipAddr + I2C_READ); //set device address and read mode
    for (uint8_t i = 0; i < howMany - 1; i++) //read -1 bytes because we must NACK the last one
    {
        readByte = readByte | i2c_readAck(); //read one byte from EEPROM
    }
    readByte = readByte | i2c_readNak(); //read one byte from EEPROM
    i2c_stop();
    return readByte;
}

//////////////////////////////////////////////////////////////////////////
//Function writes given number of ones starting from given address
//////////////////////////////////////////////////////////////////////////
void writeOnes(uint8_t chipAddr, uint8_t howMany, uint8_t whereStart)
{
    for (uint8_t i = 0; i < howMany; i++)
    {
        i2c_start_wait(chipAddr + I2C_WRITE); //set device address and write mode
        i2c_write(whereStart + i);
        i2c_write(0xFF);
        i2c_stop(); //do it for every byte because of page write (8 bytes per page), so this way it will be shorter in code
    }
}

//////////////////////////////////////////////////////////////////////////
//Function checks if there is given number of ones counting from given address
//////////////////////////////////////////////////////////////////////////
uint8_t checkOnes(uint8_t chipAddr, uint8_t howMany, uint8_t whereStart)
{
    uint8_t readByte = 0xFF;

    i2c_start(chipAddr + I2C_WRITE); //set device address and write mode
    i2c_write(whereStart);
    i2c_rep_start(chipAddr + I2C_READ); //set device address and read mode
    for (uint8_t i = 0; i < howMany - 1; i++) //read -1 bytes because we must NACK the last one
    {
        readByte = readByte & i2c_readAck(); //read one byte from EEPROM
    }
    readByte = readByte & i2c_readNak(); //read one byte from EEPROM
    i2c_stop();
    return readByte;
}

//////////////////////////////////////////////////////////////////////////
//Function blinks LED with given color
//////////////////////////////////////////////////////////////////////////
void blinkLed(uint8_t mode, uint8_t errorMode)
{
    //0 - error, 1 - black resetted, 2 - magenta, 3 - yellow, 4 - cyan, 5 - waste tank
    switch (mode)
    {
        case 0: //error, 1 white blink - chip not found, 2 white blinks - wrong data read, 3 white blinks - ink counter not resetted
            for (uint8_t i = 0; i < errorMode; i++)
            {
                PORTB = whiteLed;
                _delay_ms(250);
                PORTB = offLed;
                _delay_ms(250);
            }
            break;

        case 1: //cyan resetted
            PORTB = blueLed;
            _delay_ms(3000);
            PORTB = offLed;
            break;

        case 2: //magenta resetted
            PORTB = redLed;
            _delay_ms(3000);
            PORTB = offLed;
            break;

        case 3: //yellow resetted
            PORTB = yellowLed;
            _delay_ms(3000);
            PORTB = offLed;
            break;

        case 4: //black resetted
            PORTB = whiteLed;
            _delay_ms(3000);
            PORTB = offLed;
            break;

        case 5: //waste tank resetted
            PORTB = greenLed;
            _delay_ms(3000);
            PORTB = offLed;
            break;
    }
}

ISR(INT0_vect)
{
    startResetting = true;
}
