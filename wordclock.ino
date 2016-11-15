#include <SPI.h>

#include <Logging.h>
#include <DCF77.h>
#include <Time.h>
#include <TimeLib.h>
#include <LPD8806.h>


// PINS
const byte PIN_DCF77 = 2;

const byte PIN_LDR = A0;
const byte PIN_LED = 13;

// INTERRUPTS
const byte INTERRUPT_DCF77 = 0;

// LANGUAGES
const byte LANGUAGE_DE_DE = 0;
const byte LANGUAGE_DE_SW = 1;
const byte LANGUAGE_EN = 2;

// CACHE
const byte LDR_NUMBER_OF_LEVELS = 8;
const byte LDR_CACHE_SIZE = 16;
byte ldrCache[LDR_CACHE_SIZE];

// DCF77
//time_t time;
DCF77 dcf77 = DCF77(PIN_DCF77, INTERRUPT_DCF77);
time_t previousTime = 0;
const byte SLEEP_TIME_FROM_HOUR = 0;
const byte SLEEP_TIME_TO_HOUR = 6;

// LEDs
const byte LED_NUMBER_OF_LEDS = 126;
LPD8806 ledStrip = LPD8806(LED_NUMBER_OF_LEDS);

// LED-COLOR (0 == red, 240 == blue)
const word HUE_OFFSET = 240;
uint32_t previousColor = 0;

// Effect on start
const boolean SHOW_RANDOM_EFFECT_ON_STARTUP = true;

/// LED matrix
word matrix[11];

#define DE_X_ESIST       matrix[0] |= 0b1101110000000000
#define DE_X_VOR         matrix[3] |= 0b1110000000000000
#define DE_X_NACH        matrix[3] |= 0b0000000111100000
#define DE_X_UHR         matrix[9] |= 0b0000000011100000

#define DE_M_FUENF       matrix[0] |= 0b0000000111100000
#define DE_M_ZEHN        matrix[1] |= 0b1111000000000000
#define DE_M_VIERTEL     matrix[2] |= 0b0000111111100000
#define DE_M_ZWANZIG     matrix[1] |= 0b0000111111100000
#define DE_M_HALB        matrix[4] |= 0b1111000000000000
#define DE_M_DREIVIERTEL matrix[2] |= 0b1111111111100000

#define DE_H_EIN         matrix[5] |= 0b1110000000000000
#define DE_H_EINS        matrix[5] |= 0b1111000000000000
#define DE_H_ZWEI        matrix[5] |= 0b0000000111100000
#define DE_H_DREI        matrix[6] |= 0b1111000000000000
#define DE_H_VIER        matrix[6] |= 0b0000000111100000
#define DE_H_FUENF       matrix[4] |= 0b0000000111100000
#define DE_H_SECHS       matrix[7] |= 0b1111100000000000
#define DE_H_SIEBEN      matrix[8] |= 0b1111110000000000
#define DE_H_ACHT        matrix[7] |= 0b0000000111100000
#define DE_H_NEUN        matrix[9] |= 0b0001111000000000
#define DE_H_ZEHN        matrix[9] |= 0b1111000000000000
#define DE_H_ELF         matrix[4] |= 0b0000011100000000
#define DE_H_ZWOELF      matrix[8] |= 0b0000001111100000

#define DE_M_1           matrix[10] |= 0b1111000000000000
#define DE_M_2           matrix[10] |= 0b0000111100000000
#define DE_M_3           matrix[10] |= 0b0000000011110000
#define DE_M_4           matrix[10] |= 0b0000000000001111

// SLEEP
boolean shouldSleep = false;

// Set DEBUG mode on/off
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#define LOG_LEVEL LOG_LEVEL_NOOUTPUT

// Set test mode on/off
const boolean IS_TEST_MODE = false;
const boolean IS_LDR_TEST = false;
const boolean IS_DCF77_TEST = false;
const boolean IS_LED_TEST = false;
const boolean IS_TIME_AND_COLOR_TEST = false;
const boolean IS_COLOR_TEST = false;
const boolean IS_HSB2RGB_TEST = false;


///
/// Initializes the DCF77, LDR and LEDs.
///
void setup()
{
  // Initialize logging
  Log.Init(LOG_LEVEL, 9600);

  // Define INPUT and OUTPUT PINS  
  pinMode(PIN_DCF77, INPUT);
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_LED, OUTPUT);

  // Activate internal Pull-Up for PIN_DCF77
  digitalWrite(PIN_DCF77, HIGH);

  // Activate cache for LDR
  // TODO: Activate
  // initializeCache();

  // Start up the LED strip
  ledStrip.begin();
  // Update the strip, to start they are all 'off'
  ledStrip.show();

  // Activate DCF77 sensor
  dcf77.Start();
  // Sync time every 30 seconds
  setSyncInterval(30);
  setSyncProvider(getDCF77Time);

  randomSeed(analogRead(A5));

  // Render a fancy matrix effect until we receive the time
  Log.Debug("Waiting for time from DCF77..."CR);
  Log.Debug("It will take at least 2 minutes until a first update can be processed."CR);
  if (IS_TEST_MODE)
  {
    Log.Debug(CR"But not in this test ;)"CR);
    Log.Debug(CR);
  }
  else
  {  
    byte effectIdentifier = random(0, 4);
    unsigned long startMillis = millis();
    // Wait until the SyncProvider sets the time
    while (!isTimeSet()) 
    { 
      if (SHOW_RANDOM_EFFECT_ON_STARTUP)
      {
        unsigned long currentMillis = millis();
        unsigned int elapsedSeconds = (currentMillis - startMillis) / 1000;
        Log.Debug("Time elapsed: %d seconds"CR, elapsedSeconds);
        renderEffect(effectIdentifier);
      }
      else
      {
        Log.Debug(".");
      }
    }
    Log.Debug(CR);
  }
}

///
/// Runs forever. :)
///
void loop()
{
  if (IS_TEST_MODE)
  {
    // LDR
    if (IS_LDR_TEST)
      testLDR();
    // DCF77
    if (IS_DCF77_TEST)
      testDCF77();
    // LEDs
    if (IS_LED_TEST)
      testLEDs();
    // TIME & COLOR
    if (IS_TIME_AND_COLOR_TEST)
      testGetDayOfYear();
    // COLOR
    if (IS_COLOR_TEST)
      testColors();
    // HSB2RGB
    if (IS_HSB2RGB_TEST)
      testHSB2RGB();
  }
  else
  {
    time_t time = now();
    uint32_t color = getColor(time);

    // Sleep from 00:00 to 06:00 o'clock or when the time did not change at all
    if (isSleepTime(time))
    {
      if (!shouldSleep)
      {
        shouldSleep = true;
        fallAsleep();
        // Delay for 30 seconds
        delay(30000);
      }
    }
    else if (hasTimeChanged(time) || hasColorChanged(color))
    {
      shouldSleep = false;
      uint32_t color = getColor(time);
      renderTimeAndColor(time, color);
    }
    else
    {
      Log.Debug("Neither time nor color changed. Nothing to do. :)"CR);
    }
  }
}



// -------------------------
// DCF-77
// -------------------------

unsigned long getDCF77Time()
{ 
  time_t dcf77Time = dcf77.getTime();
  if (IS_TEST_MODE && IS_DCF77_TEST && dcf77Time != 0)
    Log.Debug("Time is updated!"CR);
  return dcf77Time;
}

boolean isTimeSet()
{
  return timeStatus() != timeNotSet;
}

boolean isSleepTime(time_t time)
{
  byte currentHour = hour(time);
  return currentHour >= SLEEP_TIME_FROM_HOUR && currentHour < SLEEP_TIME_TO_HOUR;
}

boolean hasTimeChanged(time_t time)
{
  if (time != previousTime)
  {
    previousTime = time;
    return true;
  }
  if (IS_TEST_MODE && IS_DCF77_TEST && previousTime != 0)
    Log.Debug("Time did not change, nothing to do.");
  return false;
}

void logTimeAndDate()
{
  logDigits(hour());
  Log.Debug(":");
  logDigits(minute());
  Log.Debug(":");
  logDigits(second());
  Log.Debug(" ");
  logDigits(day());
  Log.Debug(".");
  logDigits(month());
  Log.Debug(".");
  logDigits(year());
  Log.Debug(CR);
}

void logDigits(int digits)
{
  if (digits < 10)
    Log.Debug("0");
  Log.Debug("%d", digits);
}



// -------------------------
// LED
// -------------------------

void fallAsleep()
{
  clearBuffer();
  renderColor(0);
  ledStrip.show();
}

void renderTimeAndColor(time_t time, uint32_t color)
{
  clearBuffer();
  setTime(LANGUAGE_DE_SW, hour(time), minute(time));
  renderColor(color);
  ledStrip.show();
}

void renderColor(uint32_t color)
{
  for (byte i = 0; i < 11; i++)
    renderRow(i, matrix[i], color);
}

void renderRow(byte rowIdx, word data, uint32_t color)
{
  // Row 10 contains special bitmasks for the edge LEDs
  if (rowIdx == 10) 
  {  
    renderPixel(123, data & 0b1111000000000000, color);
    renderPixel(121, data & 0b0000111100000000, color);
    renderPixel(119, data & 0b0000000011110000, color);
    renderPixel(125, data & 0b0000000000001111, color);
  }
  else
  {
    // Unfortunately I failed when building the initial layout, which is why the first two rows consist of 11 LEDs
    // while all other rows consist of 12 LEDs. This must be fixed here, otherwise the positions of LEDs to activate
    // cannot be determined correctly.
    byte offset = 2;
    if (rowIdx == 0)
      offset = 0;

    byte ledIdx = 0;
    byte idx = 0;

    // Even or odd row? This is important for the determination of LEDs to activate, since even rows are treated from
    // left to right whereas odd rows are treated right to left.
    if (rowIdx % 2 == 0) 
    {
      // Hop over our bitmask from MSB -> LSB
      for (word bitMask = 32768; bitMask > 16; bitMask >>= 1)  
      {
        word value = data & bitMask;
        ledIdx = (rowIdx * 12 - offset) + idx++;
        renderPixel(ledIdx, value, color);
      }
    }
    else
    {
      // Hop over our bitmask from LSB -> MSB
      for (word bitMask = 16; bitMask > 0 && bitMask <= 32768; bitMask <<= 1) 
      {
        word value = data & bitMask;
        ledIdx = (rowIdx * 12 - offset) + idx++;
        // Evil hack
        if (rowIdx == 1 && ledIdx == 10)
          continue;
        renderPixel(ledIdx, value, color);
      }
    }
  }
}

void renderPixel(byte pixelIdx, word value, uint32_t color)
{
  if (pixelIdx < 0 || pixelIdx > LED_NUMBER_OF_LEDS - 1)
  {
    Log.Error("Pixel-Index %d is invalid. Valid values are between 0 and %d"CR, pixelIdx, LED_NUMBER_OF_LEDS - 1);
    return;
  }
  if (value){
    //Log.Debug("Activate %d"CR, pixelIdx);
    ledStrip.setPixelColor(pixelIdx, color);
  }
  else{
    //Log.Debug("Deactivate %d"CR, pixelIdx);
    ledStrip.setPixelColor(pixelIdx, 0);
  }
}

void clearBuffer()
{
  // The matrix contains 11 rows (0 to 10)
  for (byte i = 0; i < 11; i++)
    matrix[i] = 0;
}

boolean isPixelToIgnore(int pixelIndex)
{
  return pixelIndex == 33 || pixelIndex == 34 ||
    pixelIndex == 57 || pixelIndex == 58 || 
    pixelIndex == 81 || pixelIndex == 82 || 
    pixelIndex == 105 || pixelIndex == 106 || 
    pixelIndex > 117;
}

void renderRandomEffect()
{
  renderEffect((byte)random(0, 4));
}

void renderEffect(byte effectIdentifier)
{
  Log.Debug("Effect identifier is %d: ", effectIdentifier);
  switch (effectIdentifier)
  {
  case 0:
    Log.Debug("Rendering color chase"CR);
    renderColorChase(ledStrip.Color(127, 0, 0), 50);
    break;
  case 1:
    Log.Debug("Rendering rainbow"CR);
    renderRainbow(10);
    break;
  case 2:
    Log.Debug("Rendering rainbow cycle"CR);
    renderRainbowCycle(10);
    break;
  case 3:
    Log.Debug("Rendering color wipe"CR);
    renderColorWipe(ledStrip.Color(127, 0, 0), 50);
    break;
  default:
    Log.Debug("Rendering color chase (default)"CR);
    renderColorChase(ledStrip.Color(127, 0, 0), 50);
    break;

  }
}

void renderMatrixEffect() 
{
  for (byte i = 0; i < 11; i++) {

  }
}

// Chase one dot down the full strip.  Good for testing purposes.
void renderColorChase(uint32_t c, uint8_t wait) {
  // Start by turning all pixels off:
  for (int i = 0; i < ledStrip.numPixels(); i++)
    ledStrip.setPixelColor(i, 0);

  // Then display one pixel at a time
  for (int i = 0; i < ledStrip.numPixels(); i++) 
  {
    // Ignore non-visible LEDs and corner LEDs
    if (isPixelToIgnore(i))
      continue;
    ledStrip.setPixelColor(i, c);
    ledStrip.show();             
    ledStrip.setPixelColor(i, 0);
    delay(wait);
  }
  ledStrip.show();
}

void renderRainbow(uint8_t wait)
{   
  // 3 cycles of all 384 colors in the wheel
  for (int i = 0; i < 384; i++)
  {
    for (int j = 0; j < ledStrip.numPixels(); j++)
    {
      if (isTimeSet())
        return;
      // Ignore non-visible LEDs and corner LEDs
      if (isPixelToIgnore(j))
        continue;
      ledStrip.setPixelColor(j, Wheel((i + j) % 384));
    }
    ledStrip.show();
    delay(wait);
  }
}

// Slightly different, this one makes the rainbow wheel equally distributed 
// along the chain
void renderRainbowCycle(uint8_t wait)
{
  // 5 cycles of all 384 colors in the wheel
  for (int i = 0; i < 384 * 5; i++)
  {
    for (int j = 0; j < ledStrip.numPixels(); j++)
    {
      if (isTimeSet())
        return;
      // Ignore non-visible LEDs and corner LEDs
      if (isPixelToIgnore(j))
        continue;
      ledStrip.setPixelColor(j, Wheel((i + j) % 384));
      // tricky math! we use each pixel as a fraction of the full 384-color wheel
      // (thats the i / strip.numPixels() part)
      // Then add in j which makes the colors go around per pixel
      // the % 384 is to make the wheel cycle around
      ledStrip.setPixelColor(j, Wheel(((j * 384 / ledStrip.numPixels()) + i) % 384));
    }  
    ledStrip.show();
    delay(wait);
  }
}

// Fill the dots progressively along the strip.
void renderColorWipe(uint32_t c, uint8_t wait) 
{
  int numberOfLEDs = ledStrip.numPixels();
  for (int i = 0; i < numberOfLEDs; i++)
  {
    if (isTimeSet())
      return;
    // Ignore non-visible LEDs and corner LEDs
    if (isPixelToIgnore(i))
      continue;
    ledStrip.setPixelColor(i, c);
    ledStrip.show();
    delay(wait);
  }
  for (int i = numberOfLEDs - 1; i >= 0; i--)
  {
    if (isTimeSet())
      return;
    // Ignore non-visible LEDs and corner LEDs
    if (isPixelToIgnore(i))
      continue;
    ledStrip.setPixelColor(i, 0);
    ledStrip.show();
    delay(wait);
  }
}

//Input a value 0 to 384 to get a color value.
//The colours are a transition r - g -b - back to r
uint32_t Wheel(uint16_t wheelPos)
{
  byte r, g, b;
  switch (wheelPos / 128)
  {
  case 0:
    r = 127 - wheelPos % 128;  // Red down
    g = wheelPos % 128;        // Green up
    b = 0;                     // Blue off
    break; 
  case 1:
    g = 127 - wheelPos % 128;  // Green down
    b = wheelPos % 128;        // Blue up
    r = 0;                     // Red off
    break; 
  case 2:
    b = 127 - wheelPos % 128;  // Blue down 
    r = wheelPos % 128;        // Red up
    g = 0;                     // Green off
    break; 
  }
  return ledStrip.Color(r, g, b);
}

/**
 * Sets the time received by the DCF77 to the LED matrix.
 */
void setTime(byte language, byte hours, byte minutes)
{
  boolean isFullHour = minutes < 5;
  boolean isHourAdditionRequired = minutes >= 25 || (language == LANGUAGE_DE_SW && minutes >= 15 && minutes <= 19);
  setHours(language, hours, isHourAdditionRequired, isFullHour);
  setMinutes(language, minutes);
  setCorners(minutes);
}

/**
 * Sets the hours to the LED matrix. In case it is a full hour
 * (minute is 0) we have to write the word "O'CLOCK" in addition.
 */
void setHours(byte language, byte hours, boolean isHourAdditionRequired, boolean isFullHour)
{
  if (isFullHour)
    DE_X_UHR;
  if (isHourAdditionRequired)
    hours += 1;

  switch (hours) {
  case 0:
  case 12:
  case 24:
    DE_H_ZWOELF;
    break;
  case 1:
  case 13:
    if (isFullHour)
      DE_H_EIN;
    else
      DE_H_EINS;
    break;
  case 2:
  case 14:
    DE_H_ZWEI;
    break;
  case 3:
  case 15:
    DE_H_DREI;
    break;
  case 4:
  case 16:
    DE_H_VIER;
    break;
  case 5:
  case 17:
    DE_H_FUENF;
    break;
  case 6:
  case 18:
    DE_H_SECHS;
    break;
  case 7:
  case 19:
    DE_H_SIEBEN;
    break;
  case 8:
  case 20:
    DE_H_ACHT;
    break;
  case 9:
  case 21:
    DE_H_NEUN;
    break;
  case 10:
  case 22:
    DE_H_ZEHN;
    break;
  case 11:
  case 23:
    DE_H_ELF;
    break;
  }
}

void setMinutes(byte language, byte minutes)
{
  // Common German
  switch (language) {
  case LANGUAGE_DE_DE:
    DE_X_ESIST;

    switch (minutes / 5) {
    case 1:
      // 5 nach
      DE_H_FUENF;
      DE_X_NACH;
      break;
    case 2:
      // 10 nach
      DE_H_ZEHN;
      DE_X_NACH;
      break;
    case 3:
      // viertel nach
      DE_M_VIERTEL;
      DE_X_NACH;
      break;
    case 4:
      // 20 nach
      DE_M_ZWANZIG;
      DE_X_NACH;
      break;
    case 5:
      // 5 vor halb
      DE_M_FUENF;
      DE_X_VOR;
      DE_M_HALB;
      break;
    case 6:
      // halb
      DE_M_HALB;
      break;
    case 7:
      // 5 nach halb
      DE_M_FUENF;
      DE_X_NACH;
      DE_M_HALB;
      break;
    case 8:
      // 20 vor
      DE_M_ZWANZIG;
      DE_X_VOR;
      break;
    case 9:
      // viertel vor
      DE_M_VIERTEL;
      DE_X_VOR;
      break;
    case 10:
      // 10 vor
      DE_M_ZEHN;
      DE_X_VOR;
      break;
    case 11:
      // 5 vor
      DE_M_FUENF;
      DE_X_VOR;
      break;
    default:
      break;
    }
    break;
    //
    // Deutsch: Schwaebisch
    //
  case LANGUAGE_DE_SW:
    DE_X_ESIST;
    switch (minutes / 5) {
    case 1:
      // 5 nach
      DE_M_FUENF;
      DE_X_NACH;
      break;
    case 2:
      // 10 nach
      DE_M_ZEHN;
      DE_X_NACH;
      break;
    case 3:
      // viertel
      DE_M_VIERTEL;
      break;
    case 4:
      // 20 nach
      DE_M_ZWANZIG;
      DE_X_NACH;
      break;
    case 5:
      // 5 vor halb
      DE_M_FUENF;
      DE_X_VOR;
      DE_M_HALB;
      break;
    case 6:
      // halb
      DE_M_HALB;
      break;
    case 7:
      // 5 nach halb
      DE_M_FUENF;
      DE_X_NACH;
      DE_M_HALB;
      break;
    case 8:
      // 20 vor
      DE_M_ZWANZIG;
      DE_X_VOR;
      break;
    case 9:
      // dreiviertel
      DE_M_DREIVIERTEL;
      break;
    case 10:
      // 10 vor
      DE_M_ZEHN;
      DE_X_VOR;
      break;
    case 11:
      // 5 vor
      DE_M_FUENF;
      DE_X_VOR;
      break;
    default:
      break;
    }
    break;
  }
}

/// <summary>
/// Sets the corner LEDs indicating the minutes.
/// </summary>
void setCorners(byte minutes) 
{
  Log.Debug("%d minutes"CR, minutes % 5);
  switch (minutes % 5)
  {
  case 0:
    break;
  case 1:
    DE_M_1;
    break;
  case 2:
    DE_M_1;
    DE_M_2;
    break;
  case 3:
    DE_M_1;
    DE_M_2;
    DE_M_3;
    break;
  case 4:
    DE_M_1;
    DE_M_2;
    DE_M_3;
    DE_M_4;
    break;
  default:
    break;
  }
}



// -------------------------
// LDR
// -------------------------

byte getDensityFromLDR()
{
  // Read the value from the LDR
  int sensorValue = analogRead(PIN_DCF77);
  byte mappedSensorValue = map(sensorValue, 0, 1023, 0, LDR_NUMBER_OF_LEVELS);
  // Push it to the stack
  addToCache(mappedSensorValue);
  // Get the average density
  return getAverageDensityValue();
}

void initializeCache()
{
  byte defaultValue = LDR_NUMBER_OF_LEVELS / 2;
  for (byte i = 0; i < LDR_CACHE_SIZE; i++)
    ldrCache[i] = defaultValue;
}

void addToCache(byte value)
{
  for(byte i = LDR_CACHE_SIZE - 1; i >= 0; i--)
  {
    if (ldrCache[i] < 0)
    {
      if (i > 0 && ldrCache[i - 1] < 0)
        continue;
      ldrCache[i] = value;
      return;
    }
    for (byte j = 1; j < LDR_CACHE_SIZE; j++)
      ldrCache[j - 1] = ldrCache[j];
    ldrCache[LDR_CACHE_SIZE - 1] = value;
    return;
  }
}

byte getAverageDensityValue()
{
  unsigned long sum = 0;
  for (byte i = 0; i < LDR_CACHE_SIZE; i++)
  {
    if (ldrCache[i] < 0)
      continue;
    sum += ldrCache[i];
  }
  return (byte) (sum / LDR_CACHE_SIZE);
}

/*
 * Gets the day of the year (supports leap years).
 */
word getDayOfYear(word year, word month, word day)
{  
  // Number of days at the beginning of the month in a non-leap year
  word days[] = 
  { 
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 
  };    
  // The conditions to have a leap year
  if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
    return days[month - 1] + day + 1;
  else
    return days[month - 1] + day;  
}

uint8_t getR(uint32_t color)
{
  return ((uint8_t) (color >> 8) | 0x80);  
}

uint8_t getG(uint32_t color)
{
  return ((uint8_t) (color >> 16) | 0x80);  
}

uint8_t getB(uint32_t color)
{
  return ((uint8_t) (color | 0x80));  
}

uint32_t getRGB(uint32_t r, uint32_t g, uint32_t b)
{
  return ((uint32_t) (r | 0x80) << 8) | ((uint32_t) (g | 0x80) << 16) | (b | 0x80);
}

boolean hasColorChanged(uint32_t color)
{
  if (color != previousColor)
  {
    previousColor = color;
    return true;
  }
  Log.Debug("Color did not change, nothing to do.");
  return false;
}

uint32_t getColor(time_t currentTime)
{
  byte densityValue = getDensityFromLDR();
  int currentYear = year(currentTime);  
  int currentMonth = month(currentTime);
  int currentDay = day(currentTime);
  word dayOfYear = getDayOfYear(currentYear, currentMonth, currentDay);

  word hue = map(dayOfYear, 1, 366, 0, 360);
  float saturation = 1.0;
  float brightness = (float) map(densityValue, 0, 10, 0, LDR_NUMBER_OF_LEVELS)/10;

  return HSB2RGB(hue, saturation, brightness);
}

uint32_t HSB2RGB(word hue, float saturation, float brightness)
{ 
  // Apply the offset. 
  // If set to 0, the first color is red. 
  // If set to 240, the first color is blue.
  hue = (hue + HUE_OFFSET) % 360;
  saturation = constrain(saturation, 0.0, 1.0);
  brightness = constrain(brightness, 0.0, 1.0);

  byte h =  (byte)floor(hue / 60.0);
  float f = hue / 60.0 - h;
  float p = brightness * (1.0 - saturation);
  float q = brightness * (1.0 - saturation * f);
  float t = brightness * (1.0 - saturation * (1.0 - f));

  float r, g, b;

  if (h == 0 || h == 6)
  {
    r = brightness;
    g = t;
    b = p;
  }
  else if (h == 1)
  {
    r = q;
    g = brightness;
    b = p;
  }
  else if (h == 2)
  {
    r = p;
    g = brightness;
    b = t;
  }
  else if (h == 3)
  {
    r = p;
    g = q;
    b = brightness;
  }
  else if (h == 4)
  {
    r = t;
    g = p;
    b = brightness;
  }
  else if (h == 5)
  {
    r = brightness;
    g = p;
    b = q;    
  }
  else
  {
    r = 1.0;
    g = 0.0;
    b = 0.0;
  }

  return getRGB(r*255, g*255, b*255);
}



// -------------------------
// Tests
// -------------------------

void testDCF77()
{
  time_t time = now();
  if (time == previousTime)
    return;
  previousTime = time;
  logTimeAndDate();
}

void testLEDs()
{
  int h = 0;
  for (int m = 0; m < 60; m++)
  {
    clearBuffer();
    setTime(LANGUAGE_DE_SW, h, m);
    for (byte i = 0; i < 11; i++)
      renderRow(i, matrix[i], 32);
    ledStrip.show();
    Log.Debug("%d:%d"CR, h, m); 
    delay(250);
    if (m == 59)
    {
      m = -1;
      if (h == 23)
        h = 0;
      else
        h++;
    }  
  }

  /*
  clearBuffer();
   setTime(LANGUAGE_DE_SW, 10, 9);
   render();
   ledStrip.show();
   delay(3000);
   clearBuffer();
   
   setTime(LANGUAGE_DE_SW, 7, 5);
   render();
   ledStrip.show();
   delay(3000);  
   clearBuffer();
   
   setTime(LANGUAGE_DE_SW, 7, 10);
   render();
   ledStrip.show();
   delay(3000);  
   clearBuffer();
   
   setTime(LANGUAGE_DE_SW, 7, 20);
   render();
   ledStrip.show();
   delay(3000);
   clearBuffer();
   */
}

void testLDR()
{
  Log.Debug(CR);
  // Read the value from the LDR
  byte sensorValue = map((int)random(0, 1023), 0, 1024, 0, LDR_NUMBER_OF_LEVELS);
  Log.Debug("Sensor value: %d"CR, sensorValue);
  delay(100);

  // Push it to the stack
  addToCache(sensorValue);
  delay(100);

  Log.Debug("Cache content: ");
  for (word i = 0; i < LDR_CACHE_SIZE; i++)
    Log.Debug(" %d", ldrCache[i]);
  Log.Debug(CR);

  // Get the average density
  byte averageDensityValue = getAverageDensityValue();
  Log.Debug("Average density value: %d"CR, averageDensityValue);

  delay(100);
}

void testColors()
{
  float saturation = 1.0;
  float brightness = 1.0;
  for (word hue = 0; hue < 360; hue+=10)
  {
    uint32_t rgb = HSB2RGB(hue, saturation, brightness);
    Log.Debug("RGB color -> R-G-B (RGB): -> %d-%d-%d (%l)"CR, getR(rgb), getG(rgb), getB(rgb), (unsigned long)rgb);
    renderColorWipe(rgb, 5);
  }  
  clearBuffer();
}

void testGetDayOfYear()
{
  for (word y = 2014; y < 2017; y++)
  {
    for (byte m = 1; m <= 12; m++)
    {      
      if ((m % 2 == 1 && m < 8) || (m % 2 == 0 && m >= 8))
      {
        for (byte d = 1; d <= 31; d++)
        {          
          Log.Debug("Date: %d.%d.%d"CR, d, m, y);
          Log.Debug("Day of year: %d"CR, getDayOfYear(y, m, d));
          delay(50);
        }
      }      
      else if ((m % 2 == 0 && m < 8) || (m % 2 == 1 && m >= 8))
      {
        for (byte d = 1; d <= 30; d++)
        {          
          Log.Debug("Date: %d.%d.%d"CR, d, m, y);
          Log.Debug("Day of year: %d"CR, getDayOfYear(y, m, d));
          delay(50);
        }
      }      
      else
      {
        for (byte d = 1; d <= 28; d++)
        {          
          Log.Debug("Date: %d.%d.%d"CR, d, m, y);
          Log.Debug("Day of year: %d"CR, getDayOfYear(y, m, d));
          delay(50);
        } 
      }
    }
  }
}

void testHSB2RGB()
{
  // We want full saturation all the time.
  float saturation = 1.0;
  // Simulate all hue values
  for (word hue = 0; hue < 360; hue++)
  {
    // Simulate all brightness values
    for (float brightness = 0.0; brightness < 1.0; brightness+=0.1)
    {
      uint32_t rgb = HSB2RGB(hue, saturation, brightness);
      Log.Debug("RGB color -> R-G-B (RGB): -> %d-%d-%d (%l)"CR, getR(rgb), getG(rgb), getB(rgb), (long)rgb);
      delay(50);
    };
  }
}





