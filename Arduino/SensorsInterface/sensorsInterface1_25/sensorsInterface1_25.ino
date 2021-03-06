/****************************************************************
*              Arduino CMPS10 compass, L3G4200D gyro & KALMAN   *
*                  running I2C mode, MMA7260Q Accelerometer     *
*                   Gps code & XBEE & MATLAB communication      *
*                        by Balestrieri Giovanni                *
*                          AKA UserK, 2013                      *
*****************************************************************/

/*
 *  Features:
 *  
 *  - Bias substraction
 *  - Std filter for measurements
 *  - XBee connectivity
 *  - Matlab-Ready
 *  - Kalman filter
 *  - Complementary Filter
 */
 
 
#include <Wire.h>
#include <CMPS10.h>
#include "Kalman.h"
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

Kalman kalmanX;
Kalman kalmanY;

// angle estimated from gyro
double gyroXAngle = 0;
double gyroYAngle = 0;

// angle estimated from kalman
double xAngle;
double yAngle;

double compAngleX = 180;
double compAngleY = 180;

// Xbee Declaration variables
uint8_t pinRx = 2 , pinTx = 4; // the pin on Arduino
long BaudRate = 57600;
char GotChar, getData;

// Xbee SoftwareSerial initialization
SoftwareSerial xbee(pinRx, pinTx); // RX, TX

// If using hardware serial (e.g. Arduino Mega), comment
// out the above six lines and enable this line instead:
//Adafruit_GPS GPS(&Serial1);
// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO  true

// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;

// Matlab plot
boolean plotting = false;
boolean matlab = false;

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

#define codeVersion 1.60

// Filter parameters
// Gyro filter constant
#define alphaW 0.6
// Accelerometer filter constant
#define alphaA 0.7
#define alpha 0.7
// Magnetometer filter constant
#define alphaAP 0.7

// Accelerometer parameter definition
#define modeG 0.800 // [mV/G]
#define midV 336*0.0049 // [V]

/*
 *  Create compass object
 */                     
CMPS10 my_compass;
int wx;
int wy;
int wz; 
float Wfilter[3];

// W filter count
int wCont;
long wxT;
long wyT;
long wzT; 

// Defining Gyro bias
int biasWx = 10;
int biasWy = -12;
int biasWz = 1;

// Filtering options
int filterAng = 1;

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

// Filter compass params
float APxF=0;
float APyF=0;
float APzF=0;

float accX=0;
float accY=0;
float accZ=0;

double angleXAcc;
double angleYAcc;

int pitch1;
int roll1;
float bearing1;

unsigned long timerMu;
  
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
  
  //  Reset W counters
  wxT=0,wyT=0,wzT=0;  
  wCont=0;
  
  GPS.begin(9600);
  Serial.println(" Testing Gps...");
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data for high update rates!
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // uncomment this line to turn on all the available data - for 9600 baud you'll want 1 Hz rate
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_ALLDATA);
  // Set the update rate:
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
  Serial.println(PMTK_Q_RELEASE);
  
  Serial.print(" Filtering amplitude: ");
  Serial.println((float) alpha);
  setupL3G4200D(2000); // Configure L3G4200  - 250, 500 or 2000 deg/sec
  
  // Initialize xbee serial communication
  xbee.begin( BaudRate );
  //xbee.println("Setup Completed!");
  
  // Initialise Kalman Values
  kalmanX.setAngle(0);
  kalmanY.setAngle(0);
  
  kalmanX.setQbias(0.009); // Process noise variance for gyro bias
  kalmanY.setQbias(0.009); // Process noise variance for gyro bias
  
  kalmanX.setQangle(0.001); // Process noise variance for the accelerometer
  kalmanY.setQangle(0.001); // Process noise variance for the accelerometer
  
  kalmanX.setRmeasure(0.003); // Measurement noise variance 
  kalmanY.setRmeasure(0.003); // Measurement noise variance
  
  
  timerMu = micros();
  //wait for the sensors to be ready 
  delay(2000); 
}

unsigned long timer = millis();
void loop()
{
  getAccValues();
  getGyroValues();  
  getCompassValues();
  estimateAngle();
  gpsRoutine();
  xbeeRoutine();
  sendDataSensorsToMatlab();
}

/* 
   *
   *  When Matlab sends P, arduino starts sending sensors data through Xbee
   *  If Matlab sends L, it stops.
   *
   */
void xbeeRoutine()
{  
  if (Serial.available()) 
  {
    GotChar = Serial.read();
    xbee.print(GotChar);
    
    if(GotChar == 'P')
    {  	 
      plotting = true;
      Serial.println(" Sending plot data from now on.");
    } 
    if(GotChar == 'L')
    {  	
      plotting = false; 
      Serial.println(" Stop sentind data to Matlab.");
    } 
  }
  
  if(xbee.available())
  {  //is there anything to read
    getData = xbee.read();  //if yes, read it  
    
    Serial.println(getData);
    // Establishing Matlab Communication
    if(getData == 'T')
    {  	 
      xbee.print('K');
      Serial.println(" Sending K to Matlab...");
    } 
    if(getData == 'Y')
    {  	 
      Serial.println(" Matlab communication established. ");
    } 
    if(getData == 'N')
    {  	 
      //xbee.print("Communication problem!");
      Serial.println(" [Warning] Matlab communication problem. ");
    } 
       
    // Plotting command evaluation
    if(getData == 'P')
    {  	 
      plotting = true;
      Serial.println(" Sending plot data from now on.");
    } 
    if(getData == 'L')
    {  	
      plotting = false; 
      Serial.println(" Stop sentind data to Matlab.");
    }
    if (getData == 'M')
    {    
      matlab=true;
    }
    if (getData == 'S')
    {    
      matlab=false;
    }
  }
}

void sendDataSensorsToMatlab()
{
  if (matlab)
    {    
      Serial.println("       M    ");
      // Gathering and Sending Sensors data
      xbee.print('R');
      xbee.print(',');
      xbee.print(roll1);
      xbee.print(',');
      xbee.print('P');
      xbee.print(',');
      xbee.print(pitch1);
      xbee.print(',');
      xbee.print('Y');
      xbee.print(',');
      xbee.print(bearing1);
      // Send Kalman Filer Angular position values
      xbee.print(',');
      xbee.print('KR');
      xbee.print(',');
      xbee.print((int)yAngle);
      xbee.print(',');
      xbee.print('KP');
      xbee.print(',');
      xbee.print((int)xAngle);
      // Send the rest      
      xbee.print('W');
      xbee.print(',');
      //xbee.print(wx);
      xbee.print(Wfilter[0]);
      xbee.print(',');
      xbee.print('W');
      xbee.print(',');
      //xbee.print(wy);
      xbee.print(Wfilter[1]);
      xbee.print(',');
      xbee.print('W');
      xbee.print(',');
      //xbee.print(wz);
      xbee.print(Wfilter[2]);
      xbee.print(',');
      xbee.print('A');
      xbee.print(',');
      xbee.print(accX);
      xbee.print(',');
      xbee.print('A');
      xbee.print(',');
      xbee.print(accY);
      xbee.print(',');
      xbee.print('A');
      xbee.print(',');
      xbee.print(accZ);
      //Plot terminator carriage
      xbee.write(13);
    } 
}

void getAccValues()
{
  int accValX = analogRead(accPinX);
  int accValY = analogRead(accPinY);
  int accValZ = analogRead(accPinZ);
  
  float aX = (float) accValX * 0.0049;
  float aY = (float) accValY * 0.0049;
  float aZ = (float) accValZ * 0.0049;
  
  accX = (aX-midV)/modeG;
  accY = (aY-midV)/modeG;
  accZ = (aZ-midV)/modeG;
  
  //Compute angles from acc values
  angleXAcc = (atan2(-accX,accZ)) * RAD_TO_DEG;
  angleYAcc = (atan2(-accY,accZ)) * RAD_TO_DEG;
  
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
 xF = (1-alphaA)*xF + alphaA*val[0];
 yF = (1-alphaA)*yF + alphaA*val[1];
 zF = (1-alphaA)*zF + alphaA*val[2];
 
 val[0] = xF;
 val[1] = yF;
 val[2] = zF; 
}


void omegaFilterExpMA(float val[])
{
 //ArrayList val = new ArrayList();
 gxF = (1-alphaW)*gxF + alphaW*val[0];
 gyF = (1-alphaW)*gyF + alphaW*val[1];
 gzF = (1-alphaW)*gzF + alphaW*val[2];
 
 val[0] = gxF - biasWx;
 val[1] = gyF - biasWy;
 val[2] = gzF - biasWz; 
}

void getGyroValues()
{
  wCont++;
  byte xMSB = readRegister(gyroAddress, 0x29);
  byte xLSB = readRegister(gyroAddress, 0x28);
  wx = ((xMSB << 8) | xLSB);
  gyroXAngle += wx * ((double)(micros() - timerMu) / 1000000);
  wxT = wxT + wx;
  
  
  byte yMSB = readRegister(gyroAddress, 0x2B);
  byte yLSB = readRegister(gyroAddress, 0x2A);
  wy = ((yMSB << 8) | yLSB);
  gyroYAngle += wy * ((double)(micros() - timerMu) / 1000000);
  wyT = wyT + wy;
  
  byte zMSB = readRegister(gyroAddress, 0x2D);
  byte zLSB = readRegister(gyroAddress, 0x2C);
  wz = ((zMSB << 8) | zLSB);
  wzT = wzT + wz;
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
  
  Wfilter[0] = wx;
  Wfilter[1] = wy;
  Wfilter[2] = wz;
  
  omegaFilterExpMA(Wfilter);
  /*
  Serial.print(" [WxF,WyF,WzF]");
  Serial.print(" = [ ");
  Serial.print( (float) Wfilter[0]);

  Serial.print(" , ");
  Serial.print( (float) Wfilter[1]);

  Serial.print(" , ");
  Serial.print( (float) Wfilter[2]);
  Serial.print(" ]");
 */
  /*
   * To determine bias, uncomment what follows, leave
   * the board on the support and wait for the limit of 
   * the value wxT/wCont,wyT/wCont,wzT/wCont. Use it as a bias
   */
   
  //Serial.println();
  //Serial.println(wxT/wCont);
  //Serial.println(wyT/wCont);
  //Serial.println(wzT/wCont);
  
}

void estimateAngle()
{
 /* 
 compAngleX = (0.93 * (compAngleX + (wx * (double)(micros() - timerMu) / 1000000))) + (0.07 * angleXAcc); 
 compAngleY = (0.93 * (compAngleY + (-wy * (double)(micros() - timerMu) / 1000000))) + (0.07 * angleYAcc); 
 
 Serial.println();
 Serial.print("  C:( ");
 Serial.print(compAngleX);
 Serial.print(" ; ");
 Serial.print(compAngleY);
 Serial.print(")  "); 
 */
 
 xAngle = kalmanX.getAngle(angleXAcc,Wfilter[0],(double)(micros() - timerMu))-5;
 yAngle = kalmanY.getAngle(angleYAcc,Wfilter[1],(double)(micros() - timerMu));
 yAngle = -yAngle;
 
 Serial.print("K:( ");
 Serial.print((int)yAngle);
 Serial.print(" ; ");
 Serial.print((int)xAngle);
 Serial.print(")");
 Serial.println();
 
 timerMu = micros();
}

void getCompassValues()
{
   bearing1 =  my_compass.bearing();
   pitch1 = my_compass.pitch();
   roll1 = my_compass.roll();
   
   /*  Create float array of angular positions
    *  Roll  --  X
    *  Pitch --  Y
    *  Yaw   --  Z
    */
   float angPosFilter[3] = {roll1,pitch1,bearing1};
   //  Filter data  
   if (filterAng == 1)
   {
     angPosFilterExpMA(angPosFilter); 
     displayValues(angPosFilter[2],angPosFilter[1],angPosFilter[0]);
   }
   else if (filterAng == 0)
   {
     displayValues(bearing1,pitch1,roll1);
   }
   //delay(100); 
}

void angPosFilterExpMA(float val[])
{
 //ArrayList val = new ArrayList();
 APxF = (1-alphaAP)*APxF + alphaAP*val[0];
 APyF = (1-alphaAP)*APyF + alphaAP*val[1];
 APzF = (1-alphaAP)*APzF + alphaAP*val[2];
 /*
 val.add((float) gxF);
 val.add((float) gyF);
 val.add((float) gzF);
 */
 val[0] = APxF;
 val[1] = APyF;
 val[2] = APzF; 
}

void displayValues(int b, int p, int r)
{    
   Serial.print(" Compass: [R, P, Y] = [");
   Serial.print(r);  
   Serial.print(" , ");
   Serial.print(p);
   Serial.print(" , ");
   Serial.print(b);
   Serial.print(" ]");
   //Serial.println("");
}

void display_data(int b, int p, int r)
{   
  Serial.print("Bearing = "); // Display the full bearing and fine bearing seperated by a decimal poin on the LCD03
  Serial.print(b);       
  Serial.print("  ");

  delay(5);

  Serial.print("Pitch = ");
  Serial.print(p);
  Serial.print(" ");

  delay(5);
  
  Serial.print("Roll = ");
  Serial.print(r);
  Serial.print(" ");
  Serial.println();
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
