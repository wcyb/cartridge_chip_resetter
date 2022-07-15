/*
* SP112_CHIP_RESETTER.c
*
* Created: 04-04-2015 01:47:37
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

#define chipAddr 0xA6 //I2C address of the cartridge chip

#define chipDataSize 11
#define cartridgeTypeSize 2

volatile bool startResetting = false; //if true then user pressed the chip reset button
bool cartridgeTypeOk = false; //if true then we can start resetting procedure
bool resettedOk = false; //if true then chip was resetted successfully
const uint8_t cartridgeType[cartridgeTypeSize] = {32, 0}; //default cartridge type data
const uint8_t dataToCheck[chipDataSize] = {3, 1, 1, 100, 52, 48, 55, 49, 54, 54, 100};
volatile uint8_t readData[chipDataSize] = {0}; //holds data read from the chip
volatile uint8_t readCartridgeType[cartridgeTypeSize] = {0}; //holds cartridge type read from the chip

void writeZeros(uint8_t, uint8_t); //writes given number of zeros starting from the specified address
uint8_t checkZeros(uint8_t, uint8_t); //reads data and checks if all read bytes are 0
void ledBlink(uint8_t); //blinks LED, 1 - reset successfull, 2 - wrong cartridge chip type, 3 - other error

int main(void)
{
    DDRB |= (1 << PINB0); //set pin as output for LED
    DDRD &= ~(1 << PIND2); //set pin as input for the chip reset button
    PORTB = 0xFE; //switch off LED
    PORTC = 0x3F;
    PORTD = 0xFB;
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
            uint8_t isChipOk = i2c_start(chipAddr + I2C_WRITE); //if we get 0 then we can connect to the chip, else end procedure
            if (isChipOk == 0) //chip responded
            {
                i2c_write(0x0);
                i2c_rep_start(chipAddr + I2C_READ);
                readCartridgeType[0] = i2c_readAck();
                readCartridgeType[1] = i2c_readNak();
                i2c_stop();

                for (uint8_t i = 0; i < cartridgeTypeSize; i++) //check if cartridge chip type matches
                {
                    if (readCartridgeType[i] != cartridgeType[i])
                    {
                        cartridgeTypeOk = false;
                        break;
                    }
                    cartridgeTypeOk = true;
                }

                if (cartridgeTypeOk == false)
                {
                    ledBlink(2); //show that cartridge type is wrong, stop resetting
                }
                else
                {
                    uint8_t zerosCheck = 0; //this var will indicate if needed data are 0's

                    i2c_start(chipAddr + I2C_WRITE);
                    i2c_write(0x04); //set address of data to 4
                    i2c_write(3); //change type of cartridge to standard
                    i2c_write(1); //change type of cartridge to standard
                    i2c_write(1); //change type of cartridge to standard
                    i2c_stop(); //set stop conditon = release bus

                    writeZeros(1, 0x07);
                    
					i2c_start_wait(chipAddr + I2C_WRITE); //set device address and write mode, repeated start
                    i2c_write(0x08); //initial toner level
                    i2c_write(100);
                    i2c_stop();
					
                    writeZeros(1, 0x09);

                    i2c_start_wait(chipAddr + I2C_WRITE); //set device address and write mode, repeated start
                    i2c_write(0x0A); //write EDP code (407166)
                    i2c_write(52); //write number in ASCII
                    i2c_write(48);
                    i2c_write(55);
                    i2c_write(49);
                    i2c_write(54);
                    i2c_write(54);
                    i2c_stop();

                    writeZeros(20, 0x18);

                    i2c_start_wait(chipAddr + I2C_WRITE); //remaining toner level
                    i2c_write(0x2C);
                    i2c_write(100);
                    i2c_stop();

                    writeZeros(83, 0x2D); //reset all other data

                    //now check if data was written successfully
                    //read previously written value back from EEPROM
                    i2c_start_wait(chipAddr + I2C_WRITE); //set device address and write mode
                    i2c_write(0x04); //cartridge type
                    i2c_rep_start(chipAddr + I2C_READ); //set device address and read mode
                    readData[0] = i2c_readAck(); //read one byte from EEPROM
                    readData[1] = i2c_readAck();
                    readData[2] = i2c_readNak();
                    i2c_stop();

                    i2c_start(chipAddr + I2C_WRITE); //set device address and write mode
                    i2c_write(0x08); //initial toner level
                    i2c_rep_start(chipAddr + I2C_READ); //set device address and read mode
                    readData[3] = i2c_readNak(); //read one byte from EEPROM
                    i2c_stop();

                    i2c_start(chipAddr + I2C_WRITE); //set device address and write mode
                    i2c_write(0x0A); //EDP code
                    i2c_rep_start(chipAddr + I2C_READ); //set device address and read mode
                    readData[4] = i2c_readAck(); //read one byte from EEPROM
                    readData[5] = i2c_readAck();
                    readData[6] = i2c_readAck();
                    readData[7] = i2c_readAck();
                    readData[8] = i2c_readAck();
                    readData[9] = i2c_readNak();
                    i2c_stop();

                    i2c_start(chipAddr + I2C_WRITE); //set device address and write mode
                    i2c_write(0x2C); //remaining toner level
                    i2c_rep_start(chipAddr + I2C_READ); //set device address and read mode
                    readData[10] = i2c_readNak(); //read one byte from EEPROM
                    i2c_stop();

                    zerosCheck |= checkZeros(1, 0x07);
                    zerosCheck |= checkZeros(1, 0x09);
                    zerosCheck |= checkZeros(20, 0x18);
                    zerosCheck |= checkZeros(83, 0x2D);

                    for (uint8_t i = 0; i < chipDataSize; i++) //check if chip was resetted successfully
                    {
                        if (readData[i] != dataToCheck[i])
                        {
                            resettedOk = false;
                            break;
                        }
                        resettedOk = true;
                    }

                    if (resettedOk == true && zerosCheck == 0) //check also if all needed data is zeroed
                    {
                        ledBlink(1);
                    }
                    else
                    {
                        ledBlink(3);
                    }
                }
            }
            else //if something is wrong then blink 3 times
            {
                i2c_stop();
                ledBlink(3); //select mode 3
            }
            EIMSK |= (1 << INT0); //enable INT0 again
            EIFR |= (1 << INTF0); //clear INT0 flag, this will eliminate additional reset after release of the chip reset button
            startResetting = false; //end resetting
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//Writes given number of zeros starting from the specified address.
//////////////////////////////////////////////////////////////////////////
void writeZeros(uint8_t howMany, uint8_t whereStart)
{
    for (uint8_t i = 0; i < howMany; i++)
    {
        i2c_start_wait(chipAddr + I2C_WRITE); //set device address and write mode
        i2c_write(whereStart + i);
        i2c_write(0x0);
        i2c_stop(); //do it for every byte because of page write (8 bytes per page), so this way the code will be shorter (no page change code)
    }
}

//////////////////////////////////////////////////////////////////////////
//Checks if there is given number of zeros, starts counting from the specified address.
//////////////////////////////////////////////////////////////////////////
uint8_t checkZeros(uint8_t howMany, uint8_t whereStart)
{
    uint8_t readByte = 0;

    i2c_start(chipAddr + I2C_WRITE); //set device address and write mode
    i2c_write(whereStart);
    i2c_rep_start(chipAddr + I2C_READ); //set device address and read mode
    for (uint8_t i = 0; i < howMany - 1; i++) //read -1 bytes because we must NACK last one
    {
        readByte = readByte | i2c_readAck(); //read one byte from EEPROM
    }
    readByte = readByte | i2c_readNak(); //read one byte from EEPROM
    i2c_stop();
	
    return readByte;
}

void ledBlink(uint8_t blinkType)
{
    switch (blinkType)
    {
        case 1: //all ok
            PORTB |= (1 << PINB0); //turn on LED
            _delay_ms(3000);
            PORTB &= ~(1 << PINB0);
            break;

        case 2: //error, wrong chip type, 2 blinks
            PORTB |= (1 << PINB0); //turn on LED
            _delay_ms(250);
            PORTB &= ~(1 << PINB0);
            _delay_ms(250);
            PORTB |= (1 << PINB0); //turn on LED
            _delay_ms(250);
            PORTB &= ~(1 << PINB0);
            break;

        case 3: //error, 3 blinks
            PORTB |= (1 << PINB0); //turn on LED
            _delay_ms(250);
            PORTB &= ~(1 << PINB0);
            _delay_ms(250);
            PORTB |= (1 << PINB0); //turn on LED
            _delay_ms(250);
            PORTB &= ~(1 << PINB0);
            _delay_ms(250);
            PORTB |= (1 << PINB0); //turn on LED
            _delay_ms(250);
            PORTB &= ~(1 << PINB0);
            break;
    }
}

ISR(INT0_vect) //runs when the chip reset button is pressed
{
    startResetting = true;
}