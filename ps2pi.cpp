#include "ps2pi.h"
#include <wiringPi.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
void ps2pi_t::clk(bool h)  {
	digitalWrite(clkPin, h);
	delayMicroseconds(CLK_DELAY);
}

// Initialize the I/O pins and set up the controller for the desired mode.
//      TODO:
//              This function is hard coded to configure controller for analog mode.
//              Must also implement input parameters to choose what mode to use.
//              If you want digital mode or analog mode with all pressures then use reInitController()
//      Inputs:
//                      --!! NOTE !!-- wiringPiSetupPhys(), wiringPiSetupGpio() OR wiringPiSetup()
//                              should be called first. The following pins refer to either the gpio or the
//                              physical pins, depending on how wiring pi was set up.
//              _commandPin - The RPi pin that is connected to the COMMAND line of PS2 remote
//      _dataPin        - The RPi pin that is connected to the DATA line of PS2 remote
//      _clkPin         - The RPi pin that is connected to the CLOCK line of PS2 remote
//              _attnPin        - The RPi pin that is connected to the ATTENTION line of PS2 remote
//      Returns:
//                      0 success
//                      -1 error
unsigned char ps2pi_t::begin(int _commandPin, int _dataPin, int _clkPin,
							 int _attnPin)  {
	commandPin = _commandPin;
	dataPin = _dataPin;
	clkPin = _clkPin;
	attnPin = _attnPin;
	controllerMode = ANALOGMODE;
	pinMode(commandPin, OUTPUT);
	pinMode(attnPin, OUTPUT);
	pinMode(clkPin, OUTPUT);
	pinMode(dataPin, INPUT);
	controller_type = 0xff;
	pullUpDnControl(dataPin, PUD_UP);	// Data pin is open collector, needs pullup.
	for (int tmout = 0; tmout < MAX_RETRIES; tmout++) {
		transmitCmdString(type_read, sizeof(type_read));
		if (PS2data[1] == 0x73) {
			controller_type = 0x73;
		}

		// Transmit the enter config command.
		transmitCmdString(enterConfigMode, sizeof(enterConfigMode));

		// Set mode to analog mode and lock it there.
		transmitCmdString(set_mode_analog_lock,
						  sizeof(set_mode_analog_lock));
		delay(CMD_DELAY);

		// Return all pressures
		//      transmitCmdString(config_AllPressure, sizeof(config_AllPressure));
		// Exit config mode.
		transmitCmdString(exitConfigMode, sizeof(exitConfigMode));
		readPS2();

		// If read was successful (controller indicates it is in analog mode), break this config loop.
		if (PS2data[1] == controllerMode) {
			return 0;
		}
	}
	return -1;
}


// Transmit and receive one byte
//Inputs:
//              tx_byte         - The byte to transmit.
//
// Returns:
//              the byte received
unsigned char ps2pi_t::transmitByte(char tx_byte)  {
	// Data byte received
	unsigned char RXdata = 0;
	clk(true);

	// Bit bang all 8 bits
	for (int i = 0; i < 8; i++)
	{

		// If the bit to be transmitted is 1 then set the command pin to 1
		digitalWrite(commandPin, tx_byte & (1 << i) ? 1 : 0);
		clk(false);	// Clock it out and clock in data

		// If the data pin is now high then save the input.
		if (digitalRead(dataPin))
			SET(RXdata, i);
		clk(true);
	}

	// Done transferring byte, set the command pin back high and wait.
	digitalWrite(commandPin, 1);
	delayMicroseconds(BYTE_DELAY);
	return RXdata;
}


// Transmit command and put responce in PS2data
// Inputs:
//              s       - Pointer to unsigned char array to be transmitted.
//              len     - Number of bytes to be transmitted.
void ps2pi_t::transmitCmdString(unsigned char *s, int len)  {
	// Ensure that the command bit is high before clocking out and dropping att.
	digitalWrite(commandPin, 1);

	// Ensure that the clock is high before dropping attention
	clk(true);

	// Ready to begin transmitting, pull attention low.
	digitalWrite(attnPin, 0);

	// Shift the data out, one byte at a time.
	for (int i = 0; i < len; i++)
	{
		PS2data[i] = transmitByte(s[i]);
	}
	clk(true);

	// Packet finished, release attention line.
	digitalWrite(attnPin, 1);
}
void ps2pi_t::setMajorMode(int mode)
{	unsigned char cmd[9] = { 0x01, 0x44, 0, 1, 3, 0, 0, 0, 0 };
	transmitCmdString(cmd, 9);
}
char ps2pi_t::getLeftX()  {
	static char val = 0x80;

	if (isValid()) {
		val = PS2data[7];
	}
	return val;
}
char ps2pi_t::getLeftY()  {
	static char val = 0x80;
	if (isValid()) {
		val = PS2data[8];
	}
	return val;
}
char ps2pi_t::getRighX()  {
	static char val = 0x80;
	if (isValid()) {
		val = PS2data[5];
	}
	return val;
}
char ps2pi_t::getRighY()  {
	static char val = 0x80;
	if (isValid()) {
		val = PS2data[6];
	}
	return val;
}


// Read the PS2 Controller. Save the returned data to PS2data.
int ps2pi_t::readPS2(void)  {
	static bool busy = false;
	if (busy) {
		return -1;
	}
	busy = true;

	// Ensure that the command bit is high before clocing out and dropping att.
	digitalWrite(commandPin, 1);
	clk(true);

	// Drop attention pin.
	digitalWrite(attnPin, 0);

	// Wait for a while between transmitting bytes so that pins can stabilize.
	delayMicroseconds(BYTE_DELAY);

	// The TX and RX buffer used to read the controller.
	unsigned char TxRx1[21] =
	{	0x01, 0x42, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0
	};

	// Grab the first 9 bits
	int i;
	for (i = 0; i < 9; i++)
	{
		PS2data[i] = transmitByte(TxRx1[i]);
	}

	// If controller is in full data return mode, get the rest of data
	if (PS2data[1] == 0x79)
	{
		while (i < 21)
		{
			PS2data[i] = transmitByte(TxRx1[i]);

			//printf("%d: %d\t->\t%d\n", i, (int)TxRx1[i], (int)PS2data[i]);
			i++;
		}
	}

	// Done reading packet, release attention line.
	digitalWrite(attnPin, 1);
	busy = false;
	return 0;
}


// Re-Initialize the I/O pins and set up the controller for the desired mode.
//      TODO:
//              Currently this function is only coded for either analog mode without all pressures
//              returned or analog mode with all pressures. Need to implement digital.
//      Inputs:
//                      --!! NOTE !!-- wiringPiSetupPhys(), wiringPiSetupGpio() OR wiringPiSetup()
//                              should be called first. The following pins refer to either the gpio or the
//                              physical pins, depending on how wiring pi was set up.
//              _commandPin - The RPi pin that is connected to the COMMAND line of PS2 remote
//      _dataPin        - The RPi pin that is connected to the DATA line of PS2 remote
//      _clkPin         - The RPi pin that is connected to the CLOCK line of PS2 remote
//              _attnPin        - The RPi pin that is connected to the ATTENTION line of PS2 remote
//
// Returns:
//               1                      - Success!
//              -1                      - Invalid mode!
//              -2                      - Failed to get controller into desired mode in less than MAX_INIT_ATTEMPT attempts
int ps2pi_t::reInitializeController(char _controllerMode)
{	transmitCmdString(enterConfigMode, sizeof(enterConfigMode));
	transmitCmdString(set_mode_analog_lock, sizeof(set_mode_analog_lock));
	return 0;
}
int ps2pi_t::reInitializeController()
{	transmitCmdString(enterConfigMode, sizeof(enterConfigMode));
	transmitCmdString(set_mode_analog_lock, sizeof(set_mode_analog_lock));
	return 0;
}
void ps2pi_t::printData()  {
	static bool busy = false;
	if (busy) {
		return;
	}
	if (!isValid()) {
		return;
	}
	busy = true;
	printf("%02x %02x %02x  ", PS2data[0], PS2data[1], PS2data[2]);
	for (int i = 0; i < 8; i++) {
		if (PS2data[3] & (1 << i)) {
			printf(" ");
		}

		else {
			printf("%d", i);
		}
	}
	for (int i = 0; i < 8; i++) {
		if (PS2data[4] & (1 << i)) {
			printf(" ");
		}

		else {
			printf("X");
		}
	}
	for (int i = 5; i < 9; i++) {
		printf("%2x ", PS2data[i]);
	}
	if (PS2data[1] == 0x41) {
		printf("DIGITALMODE");
	}
	if (PS2data[1] == 0x73) {
		printf("ANALOGMODE");
	}
	if (PS2data[1] == 0x79) {
		printf("ALLPRESSUREMODE");
	}
	if (PS2data[1] == 0xF3) {
		printf("DS2NATIVEMODE");
	}
	busy = false;
}


