/*
* DX4050_CHIP_RESETTER.c
*
* Created: 01-11-2015 04:42:36
* Author: Wojciech Cybowski
* https://github.com/wcyb/cartridge_chip_resetter
*
*/

#define F_CPU 8000000UL //8MHz

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <util/delay.h>
#include <avr/sfr_defs.h>

#define blackChipReadAddr 0x27 //4 bit ID, 4 bit "NACK"
#define magentaChipReadAddr 0xA7
#define yellowChipReadAddr 0xE7
#define cyanChipReadAddr 0x67

#define blackChipWriteAddr 0x3F //4 bit ID, 4 bit "NACK"
#define magentaChipWriteAddr 0xBF
#define yellowChipWriteAddr 0xFF
#define cyanChipWriteAddr 0x7F

#define dataWriteSize 4
#define dataReadSize 32
#define startEndSize 10
//----------------------
#define redLed 4
#define greenLed 1
#define blueLed 2
#define yellowLed 5
#define whiteLed 7
#define offLed 0
//----------------------
#define delay100khz 0x0A
#define delay40khz 0x19
#define delay10khz 0x64
//----------------------
#define chipPrt PORTC
#define gndDet PINC0
#define en PINC1
#define clk PINC2
#define data PINC3
//----------------------
const uint8_t startData[] = {6, 0, 17, 96, 1, 6, 0, 17, 96, 0}; //header packet
const uint8_t endData[] = {6, 0, 1, 96, 1, 6, 0, 17, 96, 0}; //trailer packet
//4 bit adresses of black, magenta, yellow, cyan chips and 4 bit "ACK", then first byte values for each color, second byte value and 3 byte trailer
const uint8_t dataToCheck[] = {44, 172, 236, 108, 195, 67, 131, 3, 101, 103, 102, 104, 12, 98, 39};
volatile bool startResetting = false; //if true then user pressed the button
volatile uint8_t cartridgeChipData[dataReadSize] = {0};
volatile uint8_t resetChipData[dataWriteSize] = {0}; //this array will hold information for writing to the connected chip
//----------------------
uint8_t findConnectedChip(void); //0 - nothing connected, 1 - black, 2 - magenta, 3 - yellow, 4 - cyan
uint8_t readDataFromChip(uint8_t); //argument value 1-4 depends on found chip, returns 1 if the read data is wrong, 0 if all is ok
uint8_t resetInkCounter(uint8_t); //argument value 1-4 depends on found chip, returns 1 if resetting was not ok, 0 if all is ok
void sendData(const uint8_t[], uint8_t); //sends array of data, args are array and array size
void clearArray(volatile uint8_t[], uint8_t); //clears given array, arguments are array and array size
void blinkLed(uint8_t, uint8_t); //arguments are blink type and error mode, 0 - error(error mode can be set from 1 to 3), 1 - black resetted, 2 - magenta, 3 - yellow, 4 - cyan
void pulseAndSetEn(void); //pulses and leaves the EN pin in high state

int main(void)
{
    DDRB = 0x7; //pins 1, 2, 3 are outputs, the rest are inputs
    DDRC = 0xE; //pins 2, 3, 4 are outputs
    DDRD = 0x0; //set input type for the button, the rest of pins are also inputs
    PORTB = 0xF8;
    PORTC = 0x30;
    PORTD = 0xFB;
    //----------------------------------------------
    EICRA |= (1 << ISC01); //INT0 on falling edge
    EIMSK |= (1 << INT0); //enable INT0
    //----------------------------------------------
    chipPrt &= ~(1 << en); //set defaults
    chipPrt &= ~(1 << clk);
    chipPrt &= ~(1 << data);
    //----------------------------------------------
    sei(); //enable interrupts

    while (1)
    {
        if (startResetting == true)
        {
            EIMSK &= ~(1 << INT0); //disable INT0 for the time of resetting
            sendData(startData, startEndSize);
            uint8_t foundChip = findConnectedChip();
            if (foundChip == 0) //if nothing was found
            {
                blinkLed(0, 1); //indicate that chip was not found
            }
            else //if some chip was found
            {
                uint8_t readingResult = readDataFromChip(foundChip);
                if (readingResult == 1)
                {
                    blinkLed(0, 2); //indicate that wrong data was read
                }
                else
                {
                    uint8_t resetResult = resetInkCounter(foundChip);
                    if (resetResult == 1)
                    {
                        blinkLed(0, 3);
                    }
                    else
                    {
                        blinkLed(foundChip, 0); //if reset was ok then indicate it
                    }
                }
            }
            sendData(endData, startEndSize);
            clearArray(cartridgeChipData, dataReadSize);
            clearArray(resetChipData, dataWriteSize);
            EIMSK |= (1 << INT0); //enable INT0 again
            EIFR |= (1 << INTF0); //clear INT0 flag, this will eliminate additional reading after release of button
            startResetting = false; //end resetting
        }
    }
}

uint8_t findConnectedChip(void) //0 - nothing connected, 1 - black, 2 - magenta, 3 - yellow, 4 - cyan
{
    uint8_t chipAddresses[] = {blackChipReadAddr, magentaChipReadAddr, yellowChipReadAddr, cyanChipReadAddr};
    uint8_t actualAddress = 0;
    uint8_t chipResponse = 0;

    if (bit_is_set(PINC, gndDet))
    {
        return 0; //if gndDetect is high then chip is not connected
    } //else go on

    for (uint8_t addrNum = 0; addrNum < 4; addrNum++)
    {
        actualAddress = chipAddresses[addrNum]; //first select one of the addresses
        pulseAndSetEn(); //indicate that next transmission will occur
        for (uint8_t i = 128; i > 8; i /= 2) //MSB first, send only first nibble of the address
        {
            chipPrt &= ~(1 << clk); //send first part of the address
            if (actualAddress & i) //if on actualAddress bit position i is 1 then change the data line state
            {
                chipPrt |= (1 << data);
            }
            else
            {
                chipPrt &= ~(1 << data);
            }
            _delay_us(delay40khz); //after setting a state on the data pin, wait a while to let output value latch into chip input
            chipPrt |= (1 << clk);
            _delay_us(delay100khz); //on state should be shorter than off state
        }
        chipPrt &= ~(1 << clk);
        chipPrt &= ~(1 << data); //now read the response from the chip
        DDRC = 0x6; //set the data line as input for now
        for (uint8_t bitNum = 8; bitNum > 0; bitNum /= 2)
        {
            chipPrt &= ~(1 << clk);
            _delay_us(delay40khz);
            chipPrt |= (1 << clk);
            if (bit_is_set(PINC, data))
            {
                chipResponse |= bitNum; //set 1 on given bit
            }
            _delay_us(delay100khz);
        }
        chipPrt &= ~(1 << clk);
        DDRC = 0xE; //set the data line as output
        chipPrt &= ~(1 << data);
        if (chipResponse == 0x0C) //chip is found, this part should always be 0x0C "ACK"
        {
            chipPrt &= ~(1 << clk);
            chipPrt &= ~(1 << data);
            chipPrt &= ~(1 << en);
            DDRC = 0xE; //set the data line as output
            return addrNum + 1; //the data line will stay in input mode in case a cartridge chip was found
        }
        chipPrt &= ~(1 << en);
        chipResponse = 0; //in case that a chip with given address was not found, reset response
    }
    return 0; //if chip was not found then return 0
}

uint8_t readDataFromChip(uint8_t inkColor) //argument value 1-4 depends on found chip, returns 1 if the read data is wrong, 0 if all is ok
{
    uint8_t chipAddresses[] = {blackChipReadAddr, magentaChipReadAddr, yellowChipReadAddr, cyanChipReadAddr};
    uint8_t actualAddress = 0;
    uint8_t temp = 0;

    DDRC = 0xE; //set the data line as output
    pulseAndSetEn();
    actualAddress = chipAddresses[inkColor - 1];
    for (uint8_t i = 128; i > 8; i /= 2) //MSB first, now we know the address so we will start a transmission to this address
    {
        chipPrt &= ~(1 << clk);
        if (actualAddress & i) //if on actualAddress bit position i is 1 then change the data line state
        {
            chipPrt |= (1 << data);
        }
        else
        {
            chipPrt &= ~(1 << data);
        }
        _delay_us(delay40khz);
        chipPrt |= (1 << clk);
        _delay_us(delay100khz);
    }
    chipPrt &= ~(1 << clk);
    chipPrt &= ~(1 << data); //after transmitting the address of found chip we can read response
    DDRC = 0x6; //set the data line as input for now

    cartridgeChipData[0] = actualAddress & 0xF0; //keep the chip ID and prepare for "ACK" or "NACK"
    actualAddress = cartridgeChipData[0];
    for (uint8_t bitNum = 8; bitNum > 0; bitNum /= 2) //read the second nibble of the first byte
    {
        chipPrt &= ~(1 << clk);
        _delay_us(delay40khz);
        chipPrt |= (1 << clk);
        if (bit_is_set(PINC, data))
        {
            actualAddress |= bitNum; //set 1 on given bit
        }
        _delay_us(delay100khz);
    }
    chipPrt &= ~(1 << clk);
    cartridgeChipData[0] = actualAddress; //now we have the first byte

    for (uint8_t i = 1; i < dataReadSize; i++)
    {
        for (uint8_t bitNum = 128; bitNum > 0; bitNum /= 2)
        {
            chipPrt &= ~(1 << clk);
            _delay_us(delay40khz);
            chipPrt |= (1 << clk);
            if (bit_is_set(PINC, data))
            {
                temp |= bitNum; //set 1 on given bit
            }
            _delay_us(delay100khz);
        }
        chipPrt &= ~(1 << clk);
        cartridgeChipData[i] = temp;
        temp = 0;
    }
    chipPrt &= ~(1 << en);
    DDRC = 0xE; //set the data line as output
    chipPrt &= ~(1 << data);
    //now we can check the received data
    if (cartridgeChipData[0] != dataToCheck[inkColor - 1]) //check if the connected chip send "ACK"
    {
        return 1;
    }
    if (cartridgeChipData[12] != dataToCheck[3 + inkColor] || cartridgeChipData[13] != dataToCheck[7 + inkColor]) //check if the chip ID corresponds to the ink color
    {
        return 1;
    }
    for (uint8_t i = 14; i > 11; i--) //check the last 3 trail bytes
    {
        if (cartridgeChipData[9 + i] != dataToCheck[i])
        {
            return 1;
        }
    }
    return 0; //if everything is ok
}

uint8_t resetInkCounter(uint8_t inkColor) //argument value 1-4 depends on found chip, returns 1 if resetting was not ok, 0 if all is ok
{
    uint8_t chipAddresses[] = {blackChipWriteAddr, magentaChipWriteAddr, yellowChipWriteAddr, cyanChipWriteAddr};
    uint8_t temp = 0;

    for (uint8_t i = 0; i < dataWriteSize; i++) //first copy data that we will need
    {
        resetChipData[i] = cartridgeChipData[i + 1]; //copy data after the first byte(read address), because we use a different address when writing data
    }
    resetChipData[3] = 0; //reset ink usage

    DDRC = 0xE; //set the data line as output
    pulseAndSetEn();
    temp = chipAddresses[inkColor - 1];
    for (uint8_t i = 128; i > 0; i /= 2) //MSB first
    {
        chipPrt &= ~(1 << clk);
        if (temp & i) //if on actualAddress bit position i is 1 then change the data line state
        {
            chipPrt |= (1 << data);
        }
        else
        {
            chipPrt &= ~(1 << data);
        }
        _delay_us(delay10khz);
        chipPrt |= (1 << clk);
        _delay_us(delay100khz);
    }
    chipPrt &= ~(1 << clk);
    chipPrt &= ~(1 << data);
    temp = 0;
    for (uint8_t pos = 0; pos < dataWriteSize; pos++) //start writing of zeroed ink usage count
    {
        temp = resetChipData[pos];
        for (uint8_t i = 128; i > 0; i /= 2) //MSB first
        {
            chipPrt &= ~(1 << clk);
            if (temp & i) //if on actualAddress bit position i is 1 then change the data line state
            {
                chipPrt |= (1 << data);
            }
            else
            {
                chipPrt &= ~(1 << data);
            }
            _delay_us(delay10khz);
            chipPrt |= (1 << clk);
            _delay_us(delay100khz);
        }
        _delay_ms(6); //wait for writing of the sent byte
        chipPrt &= ~(1 << clk);
        chipPrt &= ~(1 << data); //change the state of the data line in case the last bit was 1
    }
    chipPrt &= ~(1 << en);
    //now check if writing was successful
    clearArray(cartridgeChipData, dataReadSize);
    uint8_t readResult = readDataFromChip(inkColor);
    if (readResult == 1)
    {
        return 1;
    }
    for (uint8_t i = 0; i < dataWriteSize; i++) //if chip was read ok then check if was resetted correctly
    {
        if (resetChipData[i] != cartridgeChipData[i + 1])
        {
            return 1;
        }
    }
    return 0;
}

void sendData(const uint8_t dataToSend[], uint8_t sizeOfData)
{
    uint8_t temp = 0;
    DDRC = 0xE; //set the data line as output
    pulseAndSetEn();
    for (uint8_t pos = 0; pos < sizeOfData; pos++)
    {
        temp = dataToSend[pos];
        for (uint8_t i = 128; i > 0; i /= 2) //MSB first
        {
            chipPrt &= ~(1 << clk);
            if (temp & i) //if on actualAddress bit position i is 1 then change the data line state
            {
                chipPrt |= (1 << data);
            }
            else
            {
                chipPrt &= ~(1 << data);
            }
            _delay_us(delay40khz);
            chipPrt |= (1 << clk);
            _delay_us(delay100khz);
        }
        chipPrt &= ~(1 << clk);
        chipPrt &= ~(1 << data);
    }
    chipPrt &= ~(1 << en);
}

void clearArray(volatile uint8_t arrayToClear[], uint8_t sizeOfArray) //clears given array, arguments are array and array size
{
    for (uint8_t i = 0; i < sizeOfArray; i++)
    {
        arrayToClear[i] = 0;
    }
}

void blinkLed(uint8_t mode, uint8_t errorMode)
{
    //0 - error, 1 - black, 2 - magenta, 3 - yellow, 4 - cyan
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

        case 1: //black resetted
            PORTB = whiteLed;
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

        case 4: //cyan resetted
            PORTB = blueLed;
            _delay_ms(3000);
            PORTB = offLed;
            break;
    }
}

void pulseAndSetEn(void)
{
    chipPrt |= (1 << en);
    _delay_us(60);
    chipPrt &= ~(1 << en);
    _delay_us(5);
    chipPrt |= (1 << en);
}

ISR(INT0_vect)
{
    startResetting = true;
}