************************************************************************
**
** Project......: Firmware USB AVR Si570 controler.
**
** Platform.....: ATtiny45
**
** Licence......: This software is freely available for non-commercial 
**                use - i.e. for research and experimentation only!
**                Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
**                Based on ObDev's AVR USB driver by Christian Starkjohann
**
** Programmer...: F.W. Krom, PE0FKO.
**                Thanks to Tom Baier DG8SAQ for the initial program.
**                Thanks to Alex Lee for the command 0x17 description.
** 
** Description..: Control the Si570 Freq. PLL chip over the USB port.
**
** History......: V15.1 02/12/2008: First release of PE0FKO.
**                V15.2 19/12/2008: Change the Si570 code.
**                V15.3 02/01/2009: Add Automatich smooth tune.
**                V15.4 06/01/2009: Add Automatic Band Pass Filter Selection.
**                V15.5 14/01/2009: Add the Smooth tune and band pass filter 
**                                  to the "Set freq by Si570 registers" command.
**                V15.6 17/01/2009: Bug fix, no connection on boot from PC.
**                                  Used a FreqSmooth so the returned freq is
**                                  the real freq and not the smooth center freq.
**                V15.7 22/01/2009: Source change. Upgrade ObDev to 20081022.
**                                  FreqSmoothTune variable removed from eeprom.
**                                  Test errors in i2c code changed. 
**                                  Add cmd 0x00, return firmware version number.
**                                  Add cmd 0x20, Write Si570 register
**                                  Add cmd 0x0F, Reset by Watchdog
**                V15.8 10/02/2009: CalcFreqFromRegSi570() will use the fixed
**                                  xtal freq of 114.285 MHz. Change static 
**                                  variables to make some more free rom space.
**                V15.9 17/02/2009: Disable I/O functions in case the ABPF is enabled.
**               V15.10 18/03/2009: LO frequency subtract and multiply.
**                                  Add cmd 0x31, Write the frequency subtract multiply to the eeprom
**                                  Add cmd 0x39, Return the frequency subtract multiply
**                                  Check if the DCO freq is lower than the Si570 max.
**                                  Include some .c files, it is smaller in size.
**                                  Move some static variables to register, smaller code size.
**                                  Add support for the CW Key_2 in command 0x50 & 0x51.
**                                  CW Key always return open if ABPF is enabled (command 0x50 & 0x51)
**               V15.11 27/07/2009: BUG in the CalcFreqMulAdd() with a negative subtract value.
**                                  Change the SetFreq() so that the filter table is set and 
**                                  after it the subtract multiply is done! (changed in order)
**                                  Changed the SetFreq() so that cmd 0x3a will return the requested
**                                  freq and not the Si570 freq. 
**               V15.12 28/08/2009: Added the IBPF settings. Every band, selected with the Filter
**                                  cross-over table, holds its own offset/multiply and filter number.
**                                  The command 0x41 will return the old I2C address, it only accept
**                                  I2C addresses if Index is zero.
**                                  Change of the USB Serial number is possible for the last char of 
**                                  that string "PE0FKO-0". The "0" can be changed with command 0x43.
**               V15.13 15/06/2010: Bug fix for the filter / band selection in DeviceSi570.c
**                                  Added compiler option for the Si570 chip grade B & C selection.
**                                  Delay 100ms added before the USB enumeration to stabilize the electric part.
**               V15.14 15/12/2010: Added Si570 chip Grade, DCO parameter and correct the si570 
**                                  divider selecttion. Temperature raw value.
**
**************************************************************************

Compiler: WinAVR-20071221
Chip: ATtiny45 (4Kb prom)
V14		3866 bytes (94.4% Full)
V15.1	3856 bytes (94.1% Full)
V15.2	3482 bytes (85.0% Full)
V15.3	3892 bytes (95.0% Full)
V15.4	3918 bytes (95.7% Full)
V15.5	4044 bytes (98.7% Full)
V15.6	4072 bytes (99.4% Full)
V15.7	4090 bytes (99.9% Full)
V15.8	3984 bytes (97.3% Full)
V15.9	3984 bytes (97.3% Full)
V15.10	4018 bytes (98.1% Full)
V15.11	4094 bytes (100.% Full)
V15.12	5112 bytes (62.4% Full), ATtiny85, WinAVR-20090313, vusb-20090822
V15.13	4558 bytes (55.6% Full), ATtiny85, WinAVR-20100110, vusb-20090822
V15.14	4712 bytes (57.5% Full), ATtiny85, WinAVR-20100110, vusb-20100715


Fuse bit information:
Fuse high byte:
0xdd = 1 1 0 1   1 1 0 1     RSTDISBL disabled (SPI programming can be done)
0x5d = 0 1 0 1   1 1 0 1     RSTDISBL enabled (PB5 can be used as I/O pin)
       ^ ^ ^ ^   ^ \-+-/ 
       | | | |   |   +------ BODLEVEL 2..0 (brownout trigger level -> 2.7V)
       | | | |   +---------- EESAVE (preserve EEPROM on Chip Erase -> not preserved)
       | | | +-------------- WDTON (watchdog timer always on -> disable)
       | | +---------------- SPIEN (enable serial programming -> enabled)
       | +------------------ DWEN (debug wire enable)
       +-------------------- RSTDISBL (disable external reset -> disabled)

Fuse low byte:
0xe1 = 1 1 1 0   0 0 0 1
       ^ ^ \+/   \--+--/
       | |  |       +------- CKSEL 3..0 (clock selection -> HF PLL)
       | |  +--------------- SUT 1..0 (BOD enabled, fast rising power)
       | +------------------ CKOUT (clock output on CKOUT pin -> disabled)
       +-------------------- CKDIV8 (divide clock by 8 -> don't divide) 


Modifications by Fred Krom, PE0FKO at Nov 2008
- Hang on no pull up of SCL line i2c to Si570 (or power down of Si590 in SR-V90)
- Compiler (WinAVR-20071221) optimized the i2c delay loop a way!
- Calculating the Si570 registers from a given frequency, returns a HIGH HS_DIV value
- Source cleanup and split in deferent files.
- Remove many debug USB command calls!
- Version usbdrv-20081022
- Add command 0x31, write only the Si570 registers (change freq max 3500ppm)
- Change the Si570 register calculation and now use the full 38 bits of the chip!
  Is is accurate, fast and small code! It cost only 350us (old 2ms) to calculate the new registers.
- Add command 0x3d, Read the actual used xtal frequency (4 bytes, 24 bits fraction, 8.24 bits)
- Add the "automatic smooth tune" functionality.
- Add the I/O function command 0x15
- Add the commands 0x34, 0x35, 0x3A, 0x3B, 0x3C, 0x3D
- Add the I/O function command 0x16
- Add read / write Filter cross over points 0x17
- Many code optimalization to make the small code.
- Calculation of the freq from the Si570 registers and call 0x32, command 0x30


Implemented functions:
----------------------

V15.10
+----+---+---+---+-----------------------------------------------------
|Cmd |SQA|FKO| IO| Function
+0x--+---+---+---+-----------------------------------------------------
| 00 | * |   | I | Echo value variable
| 00 |   | * | I | Get Firmware version number
| 01 | * | * | I | [DO NOT USE] set port directions
| 02 | * | * | I | [DO NOT USE] read ports
| 03 | * | * | I | [DO NOT USE] read port states 
| 04 | * | * | I | [DO NOT USE] set ports
| 05 | * |   | I | [DO NOT USE] send I2C start sequence
| 06 | * |   | I | [DO NOT USE] send I2C stop sequence
| 07 | * |   | I | [DO NOT USE] send byte to I2C
| 08 | * |   | I | [DO NOT USE] send word to I2C
| 09 | * |   | I | [DO NOT USE] send dword to I2C
| 0A | * |   | I | [DO NOT USE] send word to I2C with start and stop sequence
| 0B | * |   | I | [DO NOT USE] receive word from I2C with start and stop sequence
| 0C | * |   | I | [DO NOT USE] modify I2C clock
| 0D |   |   | I | [DO NOT USE] read OSCCAL to "value"
| 0E |   |   | I | [DO NOT USE] Write "value" to OSCCAL
| 0F | * | * | I | [DO NOT USE] Reset by Watchdog
| 10 | * |   | I | [DO NOT USE] EEPROM write byte value=address, index=data
| 11 | * |   | I | [DO NOT USE] EEPROM read byte "value"=address
| 13 |   |   | I | [DO NOT USE] return usb device address
| 15 |   | * | I | Set IO port with mask and data bytes, and perform cmd 0x16
| 16 |   | * | I | Return the I/O pin value
| 17 |   | * | I | Read the Filter cross over points and set one point
| 18 |   | * | I | Set the RX Band Pass Filter Address for one band: 0..3
| 19 |   | * | I | Read the RX Band Pass Filter Address for one band: 0..3
| 1A |   |   | I | Set the TX Low Pass Filter Address for one band: 0..7 (MOBO)
| 1B |   |   | I | Read the TX Low Pass Filter Address for one band: 0..7 (MOBO)
| 20 | * | * | I | Write byte to Si570 register
| 21 | * |   | I | [DO NOT USE] SI570: read byte to register index (Use command 0x3F)
| 22 | * |   | I | [DO NOT USE] SI570: freeze NCO (Use command 0x20)
| 23 | * |   | I | [DO NOT USE] SI570: unfreeze NCO (Use command 0x20)
| 30 | * | * | O | Set frequency by register and load Si570
| 31 |   | * | O | Write the frequency subtract multiply to the eeprom
| 32 | * | * | O | Set frequency by value and load Si570
| 33 | * | * | O | write new crystal frequency to EEPROM and use it.
| 34 |   | * | O | Write new startup frequency to eeprom
| 35 |   | * | O | Write new smooth tune to eeprom and use it.
| 39 |   | * | I | Return the frequency subtract multiply
| 3A |   | * | I | Return running frequency
| 3B |   | * | I | Return smooth tune ppm value
| 3C |   | * | I | Return the startup frequency
| 3D |   | * | I | Return the XTal frequency
| 3E |   |   | I | [DEBUG] read out calculated frequency control registers
| 3F | * | * | I | Read out frequency control registers
| 40 | * | * | I | Return I2C transmission error status
| 41 | * |   | I | [DO NOT USE] set/reset init freq status
| 41 |   | * | I | Set the new i2c address.
| 42 |   | * | I | CPU Temperaure
| 43 |   | * | I | Change USB SerialNumber ID
| 44 |   | * | I | Change the Si570 chip Grade (A,B,C)
| 50 | * | * | I | Set USR_P1 and get cw-key status
| 51 | * | * | I | Read SDA and CW key level simultaneously


Commands:
---------
All the command are working with the "usb_control_msg" command from the LibUSB open source project.
I'm using "libusb-win32-device-bin-0.1.12.1" from http://sourceforge.net/projects/libusb-win32/

To use the library include the header file ./include/usb.h in your project and add the 
library ./lib/*/libusb.lib for your linker.

Open the device with usb_open(...) with the VID & PID to get a device handle.

  int usb_control_msg(usb_dev_handle *dev, int requesttype, int request,
                      int value, int index, char *bytes, int size,
                      int timeout);

    requesttype:    Data In or OUT command (table IO value)
        I = USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN
        O = USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT

    request: The command number.
    value:   Word parameter 0
    index:   Word parameter 1
    bytes:   Array data send to the device (OUT) or from the device (IN)
    size:    length in bytes of the "bytes" array.


In case of a unknow command the firmware will return a 1 (one) if there is a bytes array specified it 
returns the byte 255.


In the next examples I will use two subroutines that will call the usb_control_msg function, it make's 
the examples more readable:

int usbCtrlMsgIN(int request, int value, int index, char *bytes, int size)
{
  return usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                         request, value, index, bytes, size, 500);
}

int usbCtrlMsgOUT(int request, int value, int index, char *bytes, int size)
{
  return usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
                         request, value, index, bytes, size, 500);
}




Command 0x00:
-------------
This call will return the version number of the firmware. The high byte is the version major and the
low byte the version minor number.

It is a bid tricky for the previous versions because they used a "word echo command" on command 0x00.
If the call will be done with the "value" parameter is set to 0x0E00 it will return version 14.0 for the
original DG8SAQ software. (Also my previous software will return the version 14.0, the owner had to
upgrade or not use it). There is also a other way to check the type of software, use the USB Version 
string for that.

Code sample:
    uint16_t version;
    r = usbCtrlMsgIN(0x00, 0x0E00, 0, &version, sizeof(version));
	// if the return value is 2, the variable version will give the major and minor
	// version number in the high and low byte.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x00
    value:           0x0E00
    index:           0
    bytes:           Version word variable
    size:            2


Command 0x01:
-------------
Set port directions.
Do not use, use I/O function 0x15


Command 0x02:
-------------
Read ports.
Do not use, use I/O function 0x15


Command 0x03:
-------------
Read port states.
Do not use.


Command 0x04:
-------------
Set ports.
In case of the enabled ABPF no change of I/O will be done.
Do not use, use I/O function 0x15


Command 0x0F:
-------------
Restart the board (done by Watchdog timer).

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x0F
    value:           0
    index:           0
    bytes:           NULL
    size:            0


Command 0x15:
-------------
Set the I/O bits of the device. The SoftRock V9 only had two I/O lines, bit0 and bit1.
It also returned the I/O value (like command 0x16).

There are two values for every I/O bit, the data direction and data bits.
+-----+------+----------------------
| DDR | DATA | PIN Function
+-----+------+----------------------
|  0  |   0  | input
|  0  |   1  | input internal pullup
|  1  |   0  | output 0
|  1  |   1  | output 1
+-----+------+----------------------

In case of the enabled ABPF no change of I/O will be done.

Code sample:
    uint16_t INP;
    r = usbCtrlMsgIN(0x15, 0x02, 0x02, &INP, sizeof(INP));
    // Set P2 to output and one!
    // Use P1 as input, no internal pull up R enabled.
    // Read the input in array INP[], only bit0 and bit1 used by this hardware.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x15
    value:           Data Direction Register
    index:           Data register
    bytes:           PIN status (returned)
    size:            2


Command 0x16:
-------------
Get the I/O values in the returned word, the SoftRock V9 only had two I/O lines, bit0 and bit1.

Code sample:
    uint16_t INP;
    r = usbCtrlMsgIN(0x16, 0, 0, &INP, sizeof(INP));
    // Read the input word INP, only bit0 and bit1 used by SoftRock V9 hardware.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x16
    value:           0
    index:           0
    bytes:           PIN status (returned)
    size:            2


Command 0x17:
-------------
Read the Filter cross over points and set one point.

This command can control 2 banks of filters.  Typically the first bank is the Rx and Tx BPF
used in the QSD and QSE stages.  The second bank is the Tx LPF between the PA and the antenna
output.  The # of crossover points of the two banks of filters can be different.  For example,
the first bank is usually 4 bands (160m, 80/40m, 30/17/20m, 15/12/10m) as in Softrock v6.3 and
v9.0.  The second bank is usually 6 bands but can go up to 7, 8, 12 or even 16!

The index is used to specify the particular filter crossover point.  The index of the 1st bank
starts from 0, and ends at the last crossover point.  For example, if there are 4 bands, then
there will be 3 crossover points: 0, 1, and 2.  Index 3 is used as a boolean flag, specifying
whether this filter bank is enabled or disabled (for automatic band pass filter ABPF function).

The index of the 2nd filter bank starts from 256, and ends at the last crossover point.  For
example, if there are 6 LPF's, then there will be 5 crossover points, with index 256, 257, 258,
259, and 260.  index 261 is used as a boolean flag, specifying whether this filter bank is
enabled or disabled (for automatic switching).  If "disabled", it usually means the filter is
set for "all pass" or bypassed.

The first call to this command should be used to find out how many crossover points there are.
Call with an index of 255 for the 1st filter bank, and an index of 256+255 for the 2nd filter
bank.

  filter_number_of_bytes = usbCtrlMsgIN(0x17, 0, 255, FilterCrossOver, sizeof(FilterCrossOver));

If there are 4 filters (3 crossover points), then the filter_number_of_bytes returned will be 8.
(Each crossover point is 2 bytes.  Thus there will be 6 bytes.  Following that another 2 bytes will
be for the boolean flag, making a total of 8).

If ther are 6 filters (5 crossover points), there filter_number_of_bytes returned will be 12.

Subsequent calls to this command can then be used to:

1.  set one of the filter crossover points by specifying the index
2.  enable/disable the filter bank by specifying the last index (for the boolean flag)
3.  read the cross over points only - all of them of one bank at once, by specifying 255 (1st bank)
    or 256+255 (2nd bank).

(Note that in actions 1 and 2 above, the cross over points are also read out after the completion
of the action.)

The data format of the crossover points is a 11.5 bits in MHz, that gives a resolution of 1/32 MHz.
The last data entry is a boolean flag to enable or disable the filter bank.

Code sample:
   uint16_t FilterCrossOver[16];        // allocate enough space for up to 16 filters
   unsigned int filter_number_of_bytes;

  // first find out how may cross over points there are for the 1st bank, use 255 for index
  filter_number_of_bytes = usbCtrlMsgIN(0x17, 0, 255, FilterCrossOver, sizeof(FilterCrossOver));

  // Specify filter cross over point for a softrock that divide the LO by 4!
  // And read the points back from the device in the last call.
  if (filter_number_of_bytes == 8)  // 3 crossover points and one flag, so set them up
  {
	FilterCrossOver[0] = 4.1 * 4.0 * (1<<5);
	FilterCrossOver[1] = 8.0 * 4.0 * (1<<5);
	FilterCrossOver[2] = 16. * 4.0 * (1<<5);
	FilterCrossOver[3] = true;        // Enable

	usbCtrlMsgIN(0x17, FilterCrossOver[0], 0, NULL, 0);
	usbCtrlMsgIN(0x17, FilterCrossOver[1], 1, NULL, 0);
	usbCtrlMsgIN(0x17, FilterCrossOver[2], 2, NULL, 0);
	usbCtrlMsgIN(0x17, FilterCrossOver[3], 3, FilterCrossOver, sizeof(FilterCrossOver));
  }


Parameters: Setting one of the points
   requesttype:    USB_ENDPOINT_IN
   request:         0x17
   value:           FilterCrossOver[i]   i being the index of the particular cross over point
   index:           index of the 'value' filter point.
   bytes:           Array of up to 16 16bits integers for the filter points.
   size:            filter_number_of_bytes

Parameters: Enable / disable the filter
   requesttype:    USB_ENDPOINT_IN
   request:         0x17
   value:           0 (disable) or 1 (enable)
   index:           index of 1 plus the last crossover point
   bytes:           Array of up to 16 16bits integers for the filter points.
   size:            filter_number_of_bytes


Command 0x18:
-------------
Set the RX Band Pass Filter Address for one band: 0..3

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x18
    value:           Filter for the band
    index:           Band number (0..3)
    bytes:           pointer 4 byte array, Band2Filter table
    size:            4


Command 0x19:
-------------
Read the RX Band Pass Filter Address for one band: 0..3

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x19
    value:           0
    index:           0
    bytes:           pointer 4 byte array, Band2Filter table
    size:            4


Command 0x1A:
-------------
Set the TX Low Pass Filter Address for one band: 0..7 (MOBO)
Not implemented


Command 0x1B:
-------------
Read the TX Low Pass Filter Address for one band: 0..7 (MOBO)
Not implemented


Command 0x20:
-------------
Write one byte to a Si570 register. Return value is the i2c error boolean in the buffer array.

Code sample:
	// Si570 RECALL function
	uint8_t i2cError;
    r = usbCtrlMsgIN(0x20, 0x55 | (135<<8), 0x01, &i2cError, 1);
	if (r == 1 && i2cError == 0)
		// OK

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x20
    value:           I2C Address low byte (only for the DG8SAQ firmware)
                     Si570 register high byte
    index:           Register value low byte
    bytes:           NULL
    size:            0


Command 0x30:
-------------
Set the oscillator frequency by Si570 register. The real frequency will be 
calculated by the firmware and the called command 0x32

Default:    None

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x30
    value:           I2C Address (only for the DG8SAQ firmware), 0
    index:           7 (only for the DG8SAQ firmware), 0
    bytes:           pointer 48 bits register
    size:            6


Command 0x31:
-------------
Write the frequency subtract, multiply value's to the eeprom and use it.

The real frequency is the input frequnecy minus the subtract value times the multiply value.
Si570_F = (Finput - subtract) * multiply


Default:    None

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x31
    value:           0
    index:           0
    bytes:           pointer 2 * 32 bits interger
    size:            8

Code sample:
	double sub, mul;
    uint32_t iSM[2];

	sub = 135.0;
	mul = 4.0;

	iSM[0] = (uint32_t)( sub * (1UL << 21) );
	iSM[1] = (uint32_t)( mul * (1UL << 21) );

    r = usbCtrlMsgOUT(0x31, 0, 0, (char *)iSM, sizeof(iSM));
    if (r != sizeof(iSM)) Error



Command 0x32:
-------------
Set the oscillator frequency by value. The frequency is formatted in MHz
as 11.21 bits value. 
The "automatic band pass filter selection", "smooth tune", "one side calibration" and
the "frequency subtract multiply" are all done in this function. (if anabled in the firmware)

Default:    None

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x32
    value:           0
    index:           0
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iFreq;
    double   dFreq;

    dFreq = 30.123456; // MHz
    iFreq = (uint32_t)( dFreq * (1UL << 21) )
    r = usbCtrlMsgOUT(0x32, 0, 0, (char *)&iFreq, sizeof(iFreq));
    if (r < 0) Error


Command 0x33:
-------------
Write new crystal frequency to EEPROM and use it. It can be changed to calibrate the device.
The frequency is formatted in MHz as a 8.24 bits value.

Default:    114.285 MHz

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x33
    value:           0
    index:           0
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iXtalFreq;
    double   dXtalFreq;

    dXtalFreq = 114.281;
    iXtalFreq = (uint32_t)( dXtalFreq * (1UL<<24) )
    r = usbCtrlMsgOUT(0x33, 0, 0, (char *)&iXtalFreq, sizeof(iXtalFreq));
    if (r < 0) Error


Command 0x34:
-------------
Write new startup frequency to eeprom. When the device is started it will output
this frequency until a program set an other frequency.
The frequency is formatted in MHz as a 11.21 bits value.

Default:    4 * 7.050 MHz

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x34
    value:           0
    index:           0
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iXtalFreq;
    double   dXtalFreq;

    dFreq = 4.0 * 3.550; // MHz
    iFreq = (uint32_t)( dFreq * (1UL<<24) )
    r = usbCtrlMsgOUT(0x34, 0, 0, (char *)&iFreq, sizeof(iFreq));
    if (r < 0) Error


Command 0x35:
-------------
Write new smooth tune to eeprom and use it.

Default:    3500 PPM

Parameters:
    requesttype:    USB_ENDPOINT_OUT
    request:         0x35
    value:           0
    index:           0
    bytes:           pointer 16 bits integer
    size:            2

Code sample:
    uint16_t Smooth;
    Smooth = 3400;
    r = usbCtrlMsgOUT(0x35, 0, 0, (char *)&Smooth, sizeof(Smooth));
    if (r < 0) Error


Command 0x39:
-------------
Return the frequency subtract multiply values.
V15.12: The index is tha band for with the values are working.

Default:    subtract = 0.0, multiply = 1.0

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x39
    value:           0
    index:           0	or	Index into band
    bytes:           pointer 2 * 32 bits integer
    size:            8

Code sample:
    uint32_t iSM[2];
	double sub, mul;
	uint8_t iBand;

	iBand = 0;
    r = usbCtrlMsgIN(0x39, 0, iBand, (char *)iSM, sizeof(iSM));
    if (r != sizeof(iSM)) Error

	sub = (double)(int32_t)iSM[0] / (1UL << 21); // Signed value
	mul = (double)         iSM[1] / (1UL << 21);


Command 0x3A:
-------------
Return actual frequency of the device.
The frequency is formatted in MHz as a 11.21 bits value.

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x3A
    value:           0
    index:           0
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iFreq;
    double   dFreq;
    r = usbCtrlMsgIN(0x3A, 0, 0, (char *)&iFreq, sizeof(iFreq));
    if (r == 4)
        dFreq = (double)iFreq / (1UL<<21);


Command 0x3B:
-------------
Return the "Smooth tune" PPM (pulse per MHz) of the device.
The value is default 3500 (from data sheet) and can be changed. I do not know what 
happened with the chip if it is out of range (>3500).
If the value is set to zero it will disable the "Automatic Smooth tune" function.

Default:    3500 PPM

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x3B
    value:           0
    index:           0
    bytes:           pointer 16 bits integer
    size:            2

Code sample:
    uint16_t Smooth;
    r = usbCtrlMsgIN(0x3B, 0, 0, (char *)&Smooth, sizeof(Smooth));
    if (r == 2) ...


Command 0x3C:
-------------
Return device startup frequency.
The frequency is formatted in MHz as a 11.21 bits value.

Default:    4 * 7.050 MHz

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x3C
    value:           0
    index:           0
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iFreq;
    double   dFreq;
    r = usbCtrlMsgIN(0x3C, 0, 0, (char *)&iFreq, sizeof(iFreq));
    if (r == 4)
        dFreq = (double)iFreq / (1UL<<21);


Command 0x3D:
-------------
Return device crystal frequency.
The frequency is formatted in MHz as a 8.24 bits value.

Default:    114.285 MHz

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x3D
    value:           0
    index:           0
    bytes:           pointer 32 bits integer
    size:            4

Code sample:
    uint32_t iFreqXtal;
    double   dFreqXtal;
    r = usbCtrlMsgIN(0x3D, 0, 0, (char *)&iFreqXtal, sizeof(iFreqXtal));
    if (r == 4)
        dFreqXtal = (double)iFreqXtal / (1UL<<24);


Command 0x3F:
-------------
Return the Si570 frequency control registers (reg 7 .. 12). If there are I2C errors
the return length is 0.

Default:    None

Parameters:
    requesttype:     USB_ENDPOINT_IN
    request:         0x3F
    value:           0
    index:           0
    bytes:           pointer 6 byte register array
    size:            6


Command 0x41:
-------------
Set a new I2C address for the Si570 chip and return the old I2C address.
If the value is not zero the I2C address will be writen to eeprom and the old value is always returned, 
The function can also be used to reset the device to "factory default" by writing the
value 255. After a restart the device will initialize to all the default values.

Default:    0x55 (85 decimal)

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x41
    value:           I2C address or 255 [byte]
    index:           0
    bytes:           pointer 1 byte I2C address
    size:            1


Command 0x42:
-------------
Get CPU Temperaure from the attiny[48]5

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x42
    value:           0
    index:           0
    bytes:           pointer 2 bytes ADC temperatur value
    size:            2


Command 0x43:
-------------
Set and return the USB SerialNumber ID. The USB SerialNumber "PE0FKO-0" can be changed only for the
last char "0". If the value is not zero the ID char will be writen to eeprom and the old value is always returned, 

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x43
    value:           New ID char (>0) [byte]
    index:           0
    bytes:           pointer 1 byte ID address
    size:            1


Command 0x44:
-------------
Change and get the Si570 chip Grade and DCO min / max value. The grade can be a number from 0 (no change),
1 (grade A), 2 (grade B), 3 (grade C, default). The grade zero will not change the grade.

The divider restrictions for the 3 Si57x speed grades or frequency grades are as follows
- Grade A covers 10 to 945 MHz, 970 to 1134 MHz, and 1213 to 1417.5 MHz. Speed grade A
  device have no divider restrictions.
- Grade B covers 10 to 810 MHz. Speed grade B devices disable the output in the following
  N1*HS_DIV settings: 1*4, 1*5
- Grade C covers 10 to 280 MHz. Speed grade C devices disable the output in the following
  N1*HS_DIV settings: 1*4, 1*5, 1*6, 1*7, 1*11, 2*4, 2*5, 2*6, 2*7, 2*9, 4*4

Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x44
    value:           Si570 chip grade (0..3) in low byte
    index:           DCO min if high byte value is zero, DCO max if high byte value is not zero
    bytes:           pointer 5 byte grade address
    size:            5


Command 0x50:
-------------
Set PTT (PB4) I/O line and read CW key level from the PB5 (CW Key_1) and PB1 (CW Key_2).
In case of the enabled ABPF no change of PTT I/O line will be done and no read of the CW key's are
done. The command will return (in case of enabled ABPF) for both CW key's a open status (bits are 1).
The returnd bit value is bit 5 (0x20) for CW key_1 and bit 1 (0x02) for CW key_2, the other bits are zero.


Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x50
    value:           Output bool to user output PTT
    index:           0
    bytes:           pointer to 1 byte variable CW Key's
    size:            1


Command 0x51:
-------------
Read CW key level from the PB5 (CW Key_1) and PB1 (CW Key_2).
In case of the enabled ABPF no read of the CW key's are done. The command will return for both CW key's 
a open status (bits are 1).
The returnd bit value is bit 5 (0x20) for CW key_1 and bit 1 (0x02) for CW key_2, the other bits are zero.


Parameters:
    requesttype:    USB_ENDPOINT_IN
    request:         0x51
    value:           0
    index:           0
    bytes:           pointer to 1 byte variable CW Key's
    size:            1


EOF

