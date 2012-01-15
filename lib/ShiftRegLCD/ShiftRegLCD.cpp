// ShiftRegLCD  - Shiftregister-based LCD library for Arduino
//
// Connects an LCD using 2 or 3 pins from the Arduino, via an 8-bit ShiftRegister (SR from now on).
//
// Acknowledgements:
//
// Based very much on the "official" LiquidCrystal library for the Arduino:
//     ttp://arduino.cc/en/Reference/Libraries
// and also the improved version (with examples CustomChar1 and SerialDisplay) from LadyAda:
//     http://web.alfredstate.edu/weimandn/arduino/LiquidCrystal_library/LiquidCrystal_index.html
// and also inspired by this schematics from an unknown author (thanks to mircho on the arduino playground forum!):
//     http://www.scienceprog.com/interfacing-lcd-to-atmega-using-two-wires/
//
// Modified to work serially with the shiftOut() function, an 8-bit shiftregister (SR) and an LCD in 4-bit mode.
//
// Shiftregister connection description (NEW as of 2009.07.27)
//
// Bit  #0 - N/C - not connected, used to hold a zero
// Bits #1 - N/C
// Bit  #2 - connects to RS (Register Select) on the LCD
// Bits #3 - #6 from SR connects to LCD data inputs D4 - D7.
// Bit  #7 - is used to enabling the enable-puls (via a diode-resistor AND "gate")
//
// 2 or 3 Pins required from the Arduino for Data, Clock, and Enable (optional). If not using Enable,
// the Data pin is used for the enable signal by defining the same pin for Enable as for Data.
// Data and Clock outputs/pins goes to the shiftregister.
// LCD RW-pin hardwired to LOW (only writing to LCD). Busy Flag (BF, data bit D7) is not read.
//
// Any shift register should do. I used 74LS164, for the reason that's what I had at hand.
//
//       Project homepage: http://code.google.com/p/arduinoshiftreglcd/
//
// History
// 2009.05.23  raron - but; based mostly (as in almost verbatim) on the "official" LiquidCrystal library.
// 2009.07.23  Incorporated some proper initialization routines
//               inspired (lets say copy-paste-tweaked) from LiquidCrystal library improvements from LadyAda
// 2009.07.25  raron - Fixed comments. I really messed up the comments before posting this, so I had to fix it.
//               Also renamed a function, but no improvements or functional changes.
// 2009.07.27  Thanks to an excellent suggestion from mircho at the Arduiono playgrond forum,
//               the number of wires now required is only two!
// 2009.07.28  Mircho / raron - a new modification to the schematics, and a more streamlined interface
// 2009.07.30  raron - minor corrections to the comments. Fixed keyword highlights. Fixed timing to datasheet safe.
// 2011.07.02  Fixed a minor flaw in setCursor function. No functional change, just a bit more memory efficient.
//               Thanks to CapnBry (from google code and github) who noticed it. URL to his version of shiftregLCD:
//               https://github.com/CapnBry/HeaterMeter/commit/c6beba1b46b092ab0b33bcbd0a30a201fd1f28c1

#include "ShiftRegLCD.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "Arduino.h"

// Assuming 1 line 8 pixel high font
ShiftRegLCD::ShiftRegLCD(uint8_t srdata, uint8_t srclock, uint8_t enable) {
	init(srdata, srclock, enable, 1, 0);
}
// Set nr. of lines, assume 8 pixel high font
ShiftRegLCD::ShiftRegLCD(uint8_t srdata, uint8_t srclock, uint8_t enable, uint8_t lines) {
	init(srdata, srclock, enable, lines, 0);
}
// Set nr. of lines and font
ShiftRegLCD::ShiftRegLCD(uint8_t srdata, uint8_t srclock, uint8_t enable, uint8_t lines, uint8_t font) {
	init(srdata, srclock, enable, lines, font);
}


void ShiftRegLCD::init(uint8_t srdata, uint8_t srclock, uint8_t enable, uint8_t lines, uint8_t font)
{
  _two_wire = 0;
  _srdata_pin = srdata; _srclock_pin = srclock; _enable_pin = enable;
  if (enable == TWO_WIRE)
  {
	_enable_pin = _srdata_pin;
	_two_wire = 1;
  }
  pinMode(_srclock_pin, OUTPUT);
  pinMode(_srdata_pin, OUTPUT);
  pinMode(_enable_pin, OUTPUT);

  if (lines>1)
  	_numlines = LCD_2LINE;
  else
  	_numlines = LCD_1LINE;

  _displayfunction = LCD_4BITMODE | _numlines;

  // For some displays you can select a 10 pixel high font
  // (Just in case I removed the neccesity to have 1-line display for this)
  if (font != 0)
    _displayfunction |= LCD_5x10DOTS;
  else
    _displayfunction |= LCD_5x8DOTS;

  // At this time this is for 4-bit mode only, as described above.
  // Page 47-ish of this (HD44780 LCD) datasheet:
  //    http://www.datasheetarchive.com/pdf-datasheets/Datasheets-13/DSA-247674.pdf
  // According to datasheet, we need at least 40ms after power rises above 2.7V
  // before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
  delayMicroseconds(50000);
  init4bits(LCD_FUNCTIONSET | LCD_8BITMODE);
  delayMicroseconds(4500);  // wait more than 4.1ms
  // Second try
  init4bits(LCD_FUNCTIONSET | LCD_8BITMODE);
  delayMicroseconds(150);
  // Third go
  init4bits(LCD_FUNCTIONSET | LCD_8BITMODE);

  // And finally, set to 4-bit interface
  init4bits(LCD_FUNCTIONSET | LCD_4BITMODE);

  // Set # lines, font size, etc.
  command(LCD_FUNCTIONSET | _displayfunction);
  // Turn the display on with no cursor or blinking default
  _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
  display();
  // Clear it off
  clear();
  // Initialize to default text direction (for romance languages)
  _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  // set the entry mode
  command(LCD_ENTRYMODESET | _displaymode);
  home();
}


// ********** high level commands, for the user! **********
void ShiftRegLCD::clear()
{
  command(LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
  delayMicroseconds(2000);    // this command takes a long time!
}

void ShiftRegLCD::home()
{
  command(LCD_RETURNHOME);  // set cursor position to zero
  delayMicroseconds(2000);  // this command takes a long time!
}

void ShiftRegLCD::setCursor(uint8_t col, uint8_t row)
{
  const uint8_t row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
  //int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
  if ( row > _numlines )
    row = _numlines-1;    // we count rows starting w/0
  command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void ShiftRegLCD::noDisplay() {
  _displaycontrol &= ~LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void ShiftRegLCD::display() {
  _displaycontrol |= LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns the underline cursor on/off
void ShiftRegLCD::noCursor() {
  _displaycontrol &= ~LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void ShiftRegLCD::cursor() {
  _displaycontrol |= LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turn on and off the blinking cursor
void ShiftRegLCD::noBlink() {
  _displaycontrol &= ~LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void ShiftRegLCD::blink() {
  _displaycontrol |= LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// These commands scroll the display without changing the RAM
void ShiftRegLCD::scrollDisplayLeft(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void ShiftRegLCD::scrollDisplayRight(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void ShiftRegLCD::shiftLeft(void) {
  _displaymode |= LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This is for text that flows Right to Left
void ShiftRegLCD::shiftRight(void) {
  _displaymode &= ~LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'right justify' text from the cursor
void ShiftRegLCD::shiftIncrement(void) {
  _displaymode |= LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'left justify' text from the cursor
void ShiftRegLCD::shiftDecrement(void) {
  _displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// Allows us to fill the first 8 CGRAM locations with custom characters
void ShiftRegLCD::createChar(uint8_t location, uint8_t charmap[]) {
  location &= 0x7; // we only have 8 locations 0-7
  command(LCD_SETCGRAMADDR | location << 3);
  for (int i=0; i<8; i++) {
    write(charmap[i]);
  }
  command(LCD_SETDDRAMADDR); // Reset display to display text (from pos. 0)
}

// ********************

void ShiftRegLCD::command(uint8_t value) {
  send(value, LOW);
}

size_t ShiftRegLCD::write(uint8_t value) {
  send(value, HIGH);
  return 0;
}

// For sending data via the shiftregister
void ShiftRegLCD::send(uint8_t value, uint8_t mode) {
  uint8_t val1, val2;
  if ( _two_wire ) shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, 0x00 ); // clear shiftregister
  digitalWrite( _enable_pin, LOW );
  mode = mode ? SR_RS_BIT : 0; // RS bit; LOW: command.  HIGH: character.
  val1 = mode | SR_EN_BIT | ((value >> 1) & 0x78); // upper nibble
  val2 = mode | SR_EN_BIT | ((value << 3) & 0x78); // lower nibble
  shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, val1 );
  digitalWrite( _enable_pin, HIGH );
  delayMicroseconds(1);                 // enable pulse must be >450ns
  digitalWrite( _enable_pin, LOW );
  if ( _two_wire ) shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, 0x00 ); // clear shiftregister
  shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, val2 );
  digitalWrite( _enable_pin, HIGH );
  delayMicroseconds(1);                 // enable pulse must be >450ns
  digitalWrite( _enable_pin, LOW );
  delayMicroseconds(40);               // commands need > 37us to settle
}

// For sending data when initializing the display to 4-bit
void ShiftRegLCD::init4bits(uint8_t value) {
  uint8_t val1;
  if ( _two_wire ) shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, 0x00 ); // clear shiftregister
  digitalWrite( _enable_pin, LOW );
  val1 = SR_EN_BIT | ((value >> 1) & 0x78);
  shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, val1 );
  digitalWrite( _enable_pin, HIGH );
  delayMicroseconds(1);                 // enable pulse must be >450ns
  digitalWrite( _enable_pin, LOW );
  delayMicroseconds(40);               // commands need > 37us to settle
}
