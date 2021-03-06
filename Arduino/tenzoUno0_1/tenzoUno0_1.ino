/****************************************************************
*                  Arduino CMPS10 compass, L3G4200D gyro,       *
*                   MMA7260Q Accelerometer & Gps code           *
*                    CMPS10 $ L3G4200D running I2C mode         *
*                        by Balestrieri Giovanni                *
*                          AKA UserK, 2013                      *
*****************************************************************/
#include <Wire.h>
#include <SoftwareSerial.h>
#include <Adafruit_GPS.h>
#if ARDUINO >= 100
 #include <SoftwareSerial.h>
#else
  // Older Arduino IDE requires NewSoftSerial, download from:
  // http://arduiniana.org/libraries/newsoftserial/
// #include <NewSoftSerial.h>
 // DO NOT install NewSoftSerial if using Arduino 1.0 or later!
#endif
// Connect the GPS Power pin to 5V
// Connect the GPS Ground pin to ground
// If using software serial (sketch example default):
//   Connect the GPS TX (transmit) pin to Digital 3
//   Connect the GPS RX (receive) pin to Digital 2
// If using hardware serial (e.g. Arduino Mega):
//   Connect the GPS TX (transmit) pin to Arduino RX1, RX2 or RX3
//   Connect the GPS RX (receive) pin to matching TX1, TX2 or TX3

// If using software serial, keep these lines enabled
// (you can change the pin numbers to match your wiring):

#if ARDUINO >= 100
  SoftwareSerial mySerial(3, 2);
#else
  NewSoftSerial mySerial(3, 2);
#endif
Adafruit_GPS GPS(&mySerial);

// If using hardware serial (e.g. Arduino Mega), comment
// out the above six lines and enable this line instead:
//Adafruit_GPS GPS(&Serial1);
// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO  true

// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;
void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy

// I2C adddress of Cmps10
#define cmpAddress 0x60  
//I2C address of the L3G4200D
#define gyroAddress 105 

// Gyro registers definition
#define CTRL_REG1 0x20
#define CTRL_REG2 0x21
#define CTRL_REG3 0x22
#define CTRL_REG4 0x23
#define CTRL_REG5 0x24

#define num 3

#define codeVersion 1.6
  // Filter parameters
  //ArrayList Wfilter = new ArrayList();
  //int Wfilter[num];
#define alpha 0.7

// Accelerometer parameter definition
#define modeG 0.800 // [mV/G]
#define midV 336*0.0049 // [V]

int wx;
int wy;
int wz; 

// Defining pin layout
int accPinX = A0;
int accPinY = A1;
int accPinZ = A2;
// Filter omega param
float gxF=0;
float gyF=0;
float gzF=0;
// Filter acc params
float xF=0;
float yF=0;
float zF=0;

float accX=0;
float accY=0;
float accZ=0;
  
void setup()
{
  Serial.begin(9600);
  // Initializes I2C
  Wire.begin();    
  Serial.print(" QuadSensors ");
  Serial.print(codeVersion);
  Serial.println(" Testing Compass... ");
  int a = soft_ver();  
  if(a==14)
  {
    Serial.print(" Cmps10 correctly connected with address: ");
    Serial.println(a);
  }
  else
  {
    Serial.println("Device not connected");
  }
  // Gps test
  /*
  GPS.begin(9600);
  Serial.println(" Testing Gps...");
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data for high update rates!
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // uncomment this line to turn on all the available data - for 9600 baud you'll want 1 Hz rate
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_ALLDATA);
  // Set the update rate
  // 1 Hz update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  // 5 Hz update rate- for 9600 baud you'll have to set the output to RMC or RMCGGA only (see above)
  //GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);
  // 10 Hz update rate - for 9600 baud you'll have to set the output to RMC only (see above)
  //GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ);

  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  // the nice thing about this code is you can have a timer0 interrupt go off
  // every 1 millisecond, and read data from the GPS for you. that makes the
  // loop code a heck of a lot easier!
  useInterrupt(true);
  
  // Ask for firmware version
  Serial1.println(PMTK_Q_RELEASE);
  */
  
  
  Serial.print(" Filtering amplitude: ");
  Serial.println((float) alpha);
  setupL3G4200D(2000); // Configure L3G4200  - 250, 500 or 2000 deg/sec
  //wait for the sensors to be ready 
  delay(2000); 
}

uint32_t timer = millis();
void loop()
{
  // updates x, y, and z with new values
  getAccValues();
  getGyroValues();  
  getCompassValues();
  gpsRoutine();
}

void getAccValues()
{
  int accValX = analogRead(accPinX);
  int accValY = analogRead(accPinY);
  int accValZ = analogRead(accPinZ);
  
  float aX = (float) accValX * 0.0049;
  float aY = (float) accValY * 0.0049;
  float aZ = (float) accValZ * 0.0049;
  /*
  Serial.println("");
   Serial.print(aX);
   Serial.print(" ,  ");
   Serial.print(aY);
   Serial.print(" ,  ");
   Serial.println(aZ);
   */
  float accX = (aX-midV)/modeG;
  float accY = (aY-midV)/modeG;
  float accZ = (aZ-midV)/modeG;
  /*
  Serial.println("");
   Serial.print(accX);
   Serial.print(" ,  ");
   Serial.print(accY);
   Serial.print(" ,  ");
   Serial.println(accZ);
   */
   
   float accfilter[] = {accX,accY,accZ};
  accFilterExpMA(accfilter);
  Serial.print("Acc = [");
  Serial.print(accX);
  Serial.print(",");
  Serial.print(accY);
  Serial.print(",");
  Serial.print(accZ);
  Serial.print("]    ");
  
  Serial.print("  [axF,ayF,azF] = [");
  Serial.print(accfilter[0]);
  Serial.print(",");
  Serial.print(accfilter[1]);
  Serial.print(",");
  Serial.print(accfilter[2]);
  Serial.print("]    ");
}  
  
void gpsRoutine()
{
  if (! usingInterrupt) {
    // read data from the GPS in the 'main loop'
    char c = GPS.read();
    // if you want to debug, this is a good time to do it!
    //if (GPSECHO)
      //if (c) Serial.print(c);
  }
  
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
  
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }
  
  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  timer = millis();
  
  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000 && GPS.fix) { 
    timer = millis(); // reset the timer
    
    Serial.print("\nTime: ");
    Serial.print(GPS.hour, DEC); Serial.print(':');
    Serial.print(GPS.minute, DEC); Serial.print(':');
    Serial.print(GPS.seconds, DEC); Serial.print('.');
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC); Serial.print('/');
    Serial.print(GPS.month, DEC); Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: "); Serial.print((int)GPS.fix);
    Serial.print(" quality: ");  Serial.println((int)GPS.fixquality); 
    if (GPS.fix) {
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(", "); 
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
      
      Serial.print("Speed (knots): "); Serial.println(GPS.speed);
      Serial.print("Angle: "); Serial.println(GPS.angle);
      Serial.print("Altitude: "); Serial.println(GPS.altitude);
      Serial.println("Satellites: "); Serial.println((int)GPS.satellites);
    }
  }
}

void accFilterExpMA(float val[])
{
 //ArrayList val = new ArrayList();
 xF = (1-alpha)*xF + alpha*val[0];
 yF = (1-alpha)*yF + alpha*val[1];
 zF = (1-alpha)*zF + alpha*val[2];
 
 val[0] = xF;
 val[1] = yF;
 val[2] = zF; 
}


void filterExpMA(float val[])
{
 //ArrayList val = new ArrayList();
 gxF = (1-alpha)*gxF + alpha*val[0];
 gyF = (1-alpha)*gyF + alpha*val[1];
 gzF = (1-alpha)*gzF + alpha*val[2];
 /*
 val.add((float) gxF);
 val.add((float) gyF);
 val.add((float) gzF);
 */
 val[0] = gxF;
 val[1] = gyF;
 val[2] = gzF; 
}

void getGyroValues()
{
  byte xMSB = readRegister(gyroAddress, 0x29);
  byte xLSB = readRegister(gyroAddress, 0x28);
  float wx = ((xMSB << 8) | xLSB);

  byte yMSB = readRegister(gyroAddress, 0x2B);
  byte yLSB = readRegister(gyroAddress, 0x2A);
  float wy = ((yMSB << 8) | yLSB);

  byte zMSB = readRegister(gyroAddress, 0x2D);
  byte zLSB = readRegister(gyroAddress, 0x2C);
  float wz = ((zMSB << 8) | zLSB);
  /*
  Serial.print("   Gyroscope [Wx,Wy,Wz]");
  Serial.print(" = [ ");
  Serial.print(wx);

  Serial.print(" , ");
  Serial.print(wy);

  Serial.print(" , ");
  Serial.print(wz);
  Serial.print(" ]");
  */
  
  float Wfilter[] = {wx,wy,wz};
  filterExpMA(Wfilter);
  Serial.print(" [WxF,WyF,WzF]");
  Serial.print(" = [ ");
  Serial.print( (float) Wfilter[0]);

  Serial.print(" , ");
  Serial.print( (float) Wfilter[1]);

  Serial.print(" , ");
  Serial.print( (float) Wfilter[2]);
  Serial.print(" ]");
  
}



void getCompassValues()
{
   byte highByte, lowByte, fine;              // highByte and lowByte store high and low bytes of the bearing and fine stores decimal place of bearing
   int pitch, pitch2, roll, roll2;           // Stores pitch and roll values of CMPS10, chars are used because they support signed value
   int bearing, Tz;                               // Stores full bearing
   
   Wire.beginTransmission(cmpAddress);         //starts communication with CMPS10
   Wire.write(2);                              //Sends the register we wish to start reading from
   Wire.endTransmission();

   Wire.requestFrom(cmpAddress, 4);           // Request 4 bytes from CMPS10
   while(Wire.available() < 4);               // Wait for bytes to become available
   highByte = Wire.read();           
   lowByte = Wire.read();            
   pitch = Wire.read();        //0-255 ????????      
   roll = Wire.read();         // 0- 255 ???      
   pitch2 = map(pitch,0,255,0,360);
   roll2 = map(roll,0,255,0,360);
    
   bearing = ((highByte<<8)+lowByte)/10;      // Calculate full bearing
   fine = ((highByte<<8)+lowByte)%10;         // Calculate decimal place of bearing
   
   
   Serial.print(" Compass: [ThetaX, ThetaY, ThetaZ] = [");
   Serial.print(roll2);  
   Serial.print(" , ");
   Serial.print(pitch2);
   Serial.print(" , ");
   Serial.print(bearing);
   Serial.print(".");
   Serial.print(fine);
   Serial.println(" ]");
   Serial.println("");/*  ... (");
   
   Serial.print(roll2);
   Serial.print(" , ");
   Serial.print(pitch2);
   Serial.print(" , ");
   Serial.print(Tz);
   Serial.println(")");*/
   delay(100); 
}

int setupL3G4200D(int scale){
  //From  Jim Lindblom of Sparkfun's code

  // Enable x, y, z and turn off power down:
  writeRegister(gyroAddress, CTRL_REG1, 0b00001111);

  // If you'd like to adjust/use the HPF, you can edit the line below to configure CTRL_REG2:
  writeRegister(gyroAddress, CTRL_REG2, 0b00001000);

  // Configure CTRL_REG3 to generate data ready interrupt on INT2
  // No interrupts used on INT1, if you'd like to configure INT1
  // or INT2 otherwise, consult the datasheet:
  writeRegister(gyroAddress, CTRL_REG3, 0b00001000);

  // CTRL_REG4 controls the full-scale range, among other things:

  if(scale == 250){
    writeRegister(gyroAddress, CTRL_REG4, 0b00100000);
  }else if(scale == 500){
    writeRegister(gyroAddress, CTRL_REG4, 0b00010000);
  }else{
    writeRegister(gyroAddress, CTRL_REG4, 0b00110000);
  }

  // CTRL_REG5 controls high-pass filtering of outputs, use it
  // if you'd like:
  writeRegister(gyroAddress, CTRL_REG5, 0b00010000);
}

void writeRegister(int deviceAddress, byte address, byte val) {
    Wire.beginTransmission(deviceAddress); // start transmission to device 
    Wire.write(address);       // send register address
    Wire.write(val);         // send value to write
    Wire.endTransmission();     // end transmission
}

int readRegister(int deviceAddress, byte address){

    int v;
    Wire.beginTransmission(deviceAddress);
    Wire.write(address); // register to read
    Wire.endTransmission();

    Wire.requestFrom(deviceAddress, 1); // read a byte

    while(!Wire.available()) {
        // waiting
    }

    v = Wire.read();
    return v;
}

int soft_ver(){
   int data;       // Software version of  CMPS10 is read into data and then returned
   
   Wire.beginTransmission(cmpAddress);
   // Values of 0 being sent with write need to be masked as a byte so they are not misinterpreted as NULL this is a bug in arduino 1.0
   Wire.write((byte)0);   // Sends the register we wish to start reading from
   Wire.endTransmission();

   Wire.requestFrom(cmpAddress, 1);                  // Request byte from CMPS10
   while(Wire.available() < 1);
   data = Wire.read();           
   Serial.print("  ");
   Serial.print(data);
   return(data);
}

// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
#ifdef UDR0
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
#endif
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}
