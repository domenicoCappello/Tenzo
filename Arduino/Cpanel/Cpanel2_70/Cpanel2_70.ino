/**********************************************************************
 *             Arduino & L3G4200D gyro & KALMAN & 3 threshold PID     *
 *                running I2C mode, MMA7260Q Accelerometer            *
 *                   Gps & XBEE & MATLAB communication                *
 *                        by Balestrieri Giovanni                     *
 *                          AKA UserK, 2013                           *
 **********************************************************************/

/**
 *  Features:
 *  
 *  - Bias substraction
 *  - Std filter for measurements
 *  - XBee connectivity
 *  - Matlab connection 
 *  - Kalman filter
 *  - Complementary Filter
 *  - Motors control
 *  - FreeIMU Ready
 **/

#include <Servo.h>
#include <Wire.h>
#include <CMPS10.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include "Kalman.h"
#include "FreeIMU.h"
#include <SoftwareSerial.h>
#include <Adafruit_GPS.h>
#if ARDUINO >= 100
#include <SoftwareSerial.h>
#endif
#include <PID_v1.h>

/**
 * Print sensor's value
 */
boolean printSetupInfo = true;
boolean printOrientationFreeIMU = false; 
boolean printRawAcc = true; 
boolean printAcc = true; 
boolean printRawGyro= false; 
boolean printGyro = false; 
boolean printRawCompass= false; 
boolean printCompass = false; 
boolean printRawKalman= false; 
boolean printKalman = true; 
boolean printPIDVals = false;
boolean printMotorsVals = false;
// Timers
boolean printTimers = false;
boolean printLandProtocolTimers = false;
boolean printGeneralTimers = true;
boolean printAccTimers = true;

boolean printThrottle = false;
boolean printAckCommands = false;
boolean printVerboseSerial = true;

// Take Off settings
int rampTill = 60;
int motorRampDelayFast = 150;
int motorRampDelayMedium = 350;
int motorRampDelaySlow = 550;
int motorRampDelayVerySlow = 850;

// Safe Mode: after timeToLand ms tenzo will land automatically
unsigned long timeToLand = 20000;
boolean autoLand = false;
boolean landing = false;
int landSpeed = 1;
/**
 * Pid Controller 
 */
boolean autoEnablePid = true;
boolean enablePid = false;
boolean enableRollPid = true;
boolean enablePitchPid = true;
boolean enableYawPid = false;
boolean enableAltitudePid = false;

// Define IO and setpoint for control
double SetpointRoll = 0, InputRoll, errorRoll, OutputRoll;
double SetpointPitch = 0, InputPitch, errorPitch, OutputPitch;
double SetpointYaw = 180, InputYaw, errorYaw, OutputYaw;
double SetpointAltitude = 1, InputAltitude, errorAltitude, OutputAltitude;

// Define the aggressive and conservative Tuning Parameters
// Roll
double aggKpRoll=0.10, aggKiRoll=0.06, aggKdRoll=0.04;
double consKpRoll=0.22, consKiRoll=0.09, consKdRoll=0.02;
double farKpRoll=0.05, farKiRoll=0.9, farKdRoll=0.03;


// Pitch
double aggKpPitch=0.07, aggKiPitch=0.06, aggKdPitch=0.04;
double consKpPitch=0.24, consKiPitch=0.09, consKdPitch=0.06;
double farKpPitch=0.02, farKiPitch=0.09,  farKdPitch=0.10;

// Yaw
double aggKpYaw=0.3, aggKiYaw=0.0, aggKdYaw=0.1;
double consKpYaw=0.3, consKiYaw=0, consKdYaw=0.0;

// Altitude  ->> *** Add it, judst created
double aggKpAltitude=0.2, aggKiAltitude=0.0, aggKdAltitude=0.1;
double consKpAltitude=0.1, consKiAltitude=0, consKdAltitude=0.1;

//Specify the links and initial tuning parameters
PID myRollPID(&InputRoll, &OutputRoll, &SetpointRoll, consKpRoll, consKiRoll, consKdRoll, DIRECT);
PID myPitchPID(&InputPitch, &OutputPitch, &SetpointPitch, consKpPitch, consKiPitch, consKdPitch, DIRECT);
PID myYawPID(&InputYaw, &OutputYaw, &SetpointYaw, consKpYaw, consKiYaw, consKdYaw, DIRECT);
PID myAltitudePID(&InputAltitude, &OutputAltitude, &SetpointAltitude, consKpAltitude, consKiAltitude, consKdAltitude, DIRECT);

// Threshold
int thresholdRoll = 7;
int thresholdFarRoll = 20;
int thresholdPitch = 7; 
int thresholdFarPitch = 25;
int thresholdYaw = 15;
int thresholdAlt = 20;

// initialize pid outputs
int rollPID = 0;
int pitchPID = 0;
int yawPID = 0;

// initial conditions
int w0X = 90;
int w0Y = 90;

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

//#if ARDUINO >= 100
SoftwareSerial mySerial(10, 11);
//#else
//NewSoftSerial mySerial(10, 11);
//#endif
Adafruit_GPS GPS(&mySerial);
unsigned long timer = millis();

// Create compass object
CMPS10 my_compass;

/**
 * FreeImu library variables
 */
boolean enableIMUfiltering = false;
boolean filterIMUData = true;
// store yaw pitch roll
float ypr[3];
float iq0, iq1, iq2, iq3;
float exInt, eyInt, ezInt;  // scaled integral error
volatile float twoKp;      // 2 * proportional gain (Kp)
volatile float twoKi;      // 2 * integral gain (Ki)
volatile float q0, q1, q2, q3; // quaternion of sensor frame relative to auxiliary frame
volatile float integralFBx,  integralFBy, integralFBz;
unsigned long lastUpdate, now; // sample period expressed in milliseconds
float sampleFreq;

//FreeIMU imu;
Kalman kalmanX;
Kalman kalmanY;
Kalman kalmanZ;

// angle estimated from gyro
double gyroXAngle = 0;
double gyroYAngle = 0;

// angle estimated from kalman
double xAngle;
double yAngle;
double zAngle;

//Offset Kalman
int kalmanXOffset = -0;
int kalmanYOffset = 0;
int kalmanZOffset = 0;

double compAngleX = 0;
double compAngleY = 0;

float complementaryConstant = 0.03; 

// If using hardware serial (e.g. Arduino Mega), comment
// out the above six lines and enable this line instead:
//Adafruit_GPS GPS(&Serial1);
// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO  true

/*
 * Boolean variables initialization
 */
// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;

//Initialize condition, true if 'i' sent via serial/xbee
boolean initializing = false;
boolean initialized = false;

// Matlab plot
boolean plotting = false;
boolean matlab = false;
boolean pid = false;
boolean manual = false;

Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

void useInterrupt(boolean); 

//I2C address of the L3G4200D
#define gyroAddress 105 

// Gyro registers definition
#define CTRL_REG1 0x20
#define CTRL_REG2 0x21
#define CTRL_REG3 0x22
#define CTRL_REG4 0x23
#define CTRL_REG5 0x24

#define num 3

#define codeVersion 2.50

/*
 * Filter parameters
 * 
 */
//
//// Gyro filter constant 60 HZ
//float alphaW = 0.02; // Exp(-2PI*Fcutoff/SamplingTime) 

// Gyro filter constant 30 HZ
float alphaW = 0.9; // Exp(-2PI*Fcutoff/SamplingTime)
//
//// Gyro filter constant 25 HZ
//float alphaW = 0.20; // Exp(-2PI*Fcutoff/SamplingTime)
//
//// Accelerometer filter constant 60 HZ
//float alphaA_xy = 0.34; // Sampling freq 350HZ
//float alphaA_z  = 0.08; // Sampling freq 150HZ

// Accelerometer filter constant 30  Hz
//float alphaA_xy = 0.28;
//float alphaA_z  = 0.58;

// Accelerometer filter constant good
float alphaA_xy = 0.04;  //use 'a' to increse and 'z' to decrease
float alphaA_z  = 0.04;  //use 'h' to increase 'n' to decrease

//// Accelerometer filter constant 25 HZ
//float alphaA_xy = 0.35;
//float alphaA_z  = 0.638;

// Euler Angles filter costant
float alphaAK = 0.3;
// Magnetometer filter constant
#define alphaAP 0.2

// threshold gyro 
#define gyrothresholdLow 800
#define gyrothresholdMid 1500

#define gyrothresholdHigh 4000




// Accelerometer parameter definition
#define modeG 0.800 // [mV/G]
#define midV 336*0.0049 // [V]
#define gX0 362
#define gY0 363
#define gZ0 364

const float ZOUT_1G = 850;
const int NADJ  = 50;

float XError,YError,ZError;
float xd,yd,zd,z,x,y;

float AccelAdjust(int axis)
{
  float acc = 0;

  for (int j=0;j<NADJ;j++)
  {
    float lettura=analogRead(axis);
    acc = acc + ((lettura*5000)/1023.0);
    delay(11); 
  }
  return acc/NADJ;
}

//  Create gyro object              
int wx;
int wxCandidate;
int wx_past = 0;
int wy;
int wyCandidate;
int wy_past = 0;
int wz; 
int wzCandidate;
int wz_past = 0;
float Wfilter[3];
float accfilter[3];
float angK[3];
float angPosFilter[3];

// W filter count
int wCont;
long wxT;
long wyT;
long wzT; 

// Defining Gyro bias
int biasWx = 10;
int biasWy = 0;//-12;
int biasWz = 1;

// Filtering options
int filterAng = 0;

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
float xF_m1=0;
float yF_m1=0;
float zF_m1=0;
float xF_m2=0;
float yF_m2=0;
float zF_m2=0;
float xFbuff[3] = {0,0,0};
float yFbuff[3] = {0,0,0};
float zFbuff[3] = {0,0,0};
  
unsigned char bufferAcc[694];  // 13 + (n*17)
int sizeBuffAcc = 1000;
float buffXAccToSend[1000];
float buffYAccToSend[1000];
float buffZAccToSend[1000];
float buffTimeToSend[1000];
int contBuffAcc = 1;

// Filter compass params
float APxF=0;
float APyF=0;
float APzF=0;

// Filter Kalman angles
float AKxF=0;
float AKyF=0;
float AKzF=0;

float accX=0;
float accY=0;
float accZ=0;

double angleXAcc;
double angleYAcc;
double angleZAcc;

// Pressure Params
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
float seaLevelPressure = 1021;
int pitchK;
int rollK;
int yaw;
int pitch1;
int roll1;
float bearing1;

// Motor constant
int thresholdUp=255, thresholdDown=1;
// Motor speed;
int throttle=0;
int omega=0;

/** 
 * Timers
 **/

// General 
unsigned long generalTimer;
unsigned long lastGeneralTimer;
// Acc
unsigned long accTimer;
unsigned long lastAccTimer;
unsigned long filteredAccTimer;
unsigned long lastFiltAccTimer;
// Gyro
unsigned long gyroTimer;
unsigned long lastGyroTimer;
unsigned long filteredGyroTimer;
unsigned long lastFilteredGyroTimer;
// Kalman
unsigned long kalTimer;
unsigned long lastKalTimer;
unsigned long filteredKalTimer;
unsigned long lastFilteredKalTimer;

unsigned long timerMu;

/** 
 *  Xbee - Matlab pid values communication 
 *  Expected command: 
 *  X,[0-9],[0-9],[0-9],[1.2f],X
 **/

byte byteRead;
double num1, num2;
double complNum,answer,counter;
int numOfDec,optCount=0,letterCount=0;
boolean mySwitch=false;
boolean cmplx = false;
int opt1,opt2,opt3;
char* options1[4] = {
  "Roll","pitch","Yaw","Altitude"};
char* options2[2]  = {
  "Conservative","Aggressive"};
char* options3[3]  = {
  "proportional","Derivative","Integral"};

// Landing protocol variables
unsigned long timerStart;
unsigned long checkpoint;

/**
 *  Serial Communication Protocol
 **/

// Serial
int BaudRateSerial = 9600;
// Gps
int BaudRateGps = 4800;
// Xbee 
uint8_t pinRx = 12 , pinTx = 13; // the pin on Arduino
int BaudRateXbee = 19200;
char GotChar;
byte getData;
// Xbee SoftwareSerial initialization
SoftwareSerial xbee(pinRx, pinTx);

byte loBytew1, hiBytew1,loBytew2, hiBytew2;
int loWord,hiWord;

// Serial Protocol
int versionArduinoProtocol = 5;
int mode = 0;
long cmd1;
long cmd2;
long cmd3;
long cmd4;
int numCommandToSend = 0;
int numFilterValuesToSend = 0;

int inputBuffSize = 30;
int outputBuffSize = 30;
byte bufferBytes[30];

// Serial IDs
int arduinoAdd = 1;
int MatlabAdd = 2;

int motorsID = 1;
int accID = 2;
int gyroID = 3;
int magnID = 4;
int estID = 5;
int sonicID = 6;
int gpsID = 7;
int baroID = 8;
int rollConsID = 9;
int pitchConsID = 10;
int yawConsID = 11;
int altitudeConsID = 12;
int rollAggID = 13;
int pitchAggID = 14;
int yawAggID = 15;
int altitudeAggID = 16;
int takeOffID = 17;
int iHoverID = 18;
int landID = 19;
int enableMotorsID = 20;
int sendConsPidRollID = 21;
int sendConsPidPitchID = 22;
int sendConsPidYawID = 23;
int sendConsPidAltID = 24;
int sendAggPidRollID = 25;
int sendAggPidPitchID = 26;
int sendAggPidYawID = 27;
int sendAggPidAltID = 28;
int tenzoStateID = 30;
int connID = 31;
int accValuesID = 32;


int cmdLength = 17;
int shortCmdLength = 9;
int headerLength = 13;

typedef struct mcTag {  // 13 bytes
  unsigned char srcAddr;
  unsigned char dstAddr;
  unsigned long versionX;
  unsigned char numCmds;
  unsigned char hdrLength;
  unsigned char cmdLength;
  unsigned short totalLen;
  unsigned short crc; 
} 
MyControlHdr;

typedef struct ctrTag { // 17 bytes
  unsigned char cmd;
  long param1;
  long param2;
  long param3;
  long param4;   
} 
MyCommand;

typedef struct ctrTagShort { // 17 bytes
  unsigned char cmd;
  float param1;
  float param2;
  float param3;
  float param4; 
} 
MyShortCommand;

unsigned char buffer[47];  // or worst case message size 70, 47: 2 mess

MyControlHdr * pCtrlHdr = (MyControlHdr *)(&buffer[0]);
MyControlHdr * pCtrlHdrAcc = (MyControlHdr *)(&bufferAcc[0]);

// Control Sensors data transmission
boolean sendAccToMatlab = false;
boolean sendGyroToMatlab = false;
boolean sendEstToMatlab = false;
boolean sendMagnToMatlab = false;
boolean sendMotorsToMatlab = false;
boolean sendRollToMatlab = false;
boolean sendPitchToMatlab = false;
boolean sendYawToMatlab = false;
boolean sendAltToMatlab = false;
boolean sendPidState = false;
boolean sendLandAck = false;
boolean sendTakeOffAck = false;
boolean sendHoverState = false;
boolean sendTenzoState = false;
boolean sendConnAck = false;
boolean sendDisconnAck = false;
boolean storeAccData = false;
boolean accDataReady = false;

void setup()
{
  Serial.begin(BaudRateSerial);
  // Initializes I2C
  Wire.begin();    
  if (printSetupInfo)
  {
    Serial.print(" CPanel ");
    Serial.print(codeVersion);
  
    Serial.println("Pressure Sensor Test");
    Serial.println("");
  }
  /* Initialise the sensor */
  if(!bmp.begin() && printSetupInfo)
  {
    /* There was a problem detecting the BMP085 ... check your connections */
    Serial.print("Ooops, no BMP085 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }

  //  Reset W counters
  wxT=0,wyT=0,wzT=0;  
  wCont=0;

  GPS.begin(BaudRateGps);
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
  if (printSetupInfo)
  {
    // Ask for firmware version
    Serial.println(PMTK_Q_RELEASE);  
    Serial.print(" Accelerometer Filtering amplitude: X,Y Axis:");
    Serial.println((float) alphaA_xy);  
    Serial.print(" Z Axis: ");
    Serial.println((float) alphaA_z);
    Serial.print(" Gyroscope Filtering amplitude: ");
    Serial.println((float) alphaW);
  }


  XError =  AccelAdjust(0);
  YError =  AccelAdjust(1);
  ZError =  AccelAdjust(2);
  ZError = ZError - ZOUT_1G;
  /*
  Serial.println("Ajustado acelerometro eje X");
   Serial.print("Error de X= ");
   Serial.println(XError);
   
   Serial.println("Ajustado acelerometro eje Y");
   Serial.print("Error de Y= ");
   Serial.println(YError);
   
   Serial.println("Ajustado acelerometro eje Z");
   Serial.print("Error de Z= ");
   Serial.println(ZError);
   */
  setupL3G4200D(250); // Configure L3G4200  - 250, 500 or 2000 deg/sec

  // Initialize xbee serial communication
  xbee.begin( BaudRateXbee );
  //xbee.println("Setup Completed!");

  // Initialise Kalman Values
  kalmanX.setAngle(0);
  kalmanY.setAngle(0);
  kalmanZ.setAngle(0);

  kalmanX.setQbias(2); // Process noise variance for gyro bias
  kalmanY.setQbias(6); // Process noise variance for gyro bias
  kalmanZ.setQbias(6); // Process noise variance for gyro bias

  kalmanX.setQangle(0.05); // Process noise variance for the accelerometer
  kalmanY.setQangle(0.05); // Process noise variance for the accelerometer
  kalmanZ.setQangle(0.05); // Process noise variance for the accelerometer

  // KalmanX.setRmeasure(0.003); // Measurement noise variance

  kalmanX.setRmeasure(0.5); // Measurement noise variance  
  kalmanY.setRmeasure(0.5); // Measurement noise variance
  kalmanZ.setRmeasure(0.5); // Measurement noise variance


  timerMu = micros();

  // Motors initialization
  servo1.attach(3);  
  servo2.attach(5);    
  servo3.attach(6);   
  servo4.attach(9);

  servo1.write(0);  
  servo2.write(0); 
  servo3.write(0); 
  servo4.write(0);
  
  if (printSetupInfo)
  {
    getBmpValues();
  }
  // Xbee communication variables
  num1=0;
  num2=0;
  complNum=0;
  counter=1;
  numOfDec=0;  

  //wait for the sensors to be ready 
  delay(2000); 
  timerStart = 0;
}

void loop()
{
  updateTimers();
  getAccValues(); 
  getCompassValues(); 
  getGyroValues(); 
  estimateAngle();
  //gpsRoutine();
  xbeeRoutine();
  detOrientation();
  control();
  sendDataSensors(matlab);
  protocol1();
  timers();
}

void updateTimers()
{
  // General timers
  lastGeneralTimer = millis();  
}

void timers()
{ 
  generalTimer = millis() - lastGeneralTimer;
  if (printTimers)
  {
    if (printLandProtocolTimers)
    {
      Serial.println();
      Serial.print("                      checkpoint: ");
      Serial.print(checkpoint);
      Serial.print("      timer: ");
      Serial.print(timerStart);
      Serial.println();
    }
    if (printGeneralTimers)
    {
      Serial.println();
      Serial.print("    lastGenTimer: ");
      Serial.print(lastGeneralTimer);
      Serial.print("    GenTimer: ");
      Serial.print(generalTimer);
      Serial.print(" ms ");
      Serial.println();
    }
  }
}

void control()
{
  if (enablePid)
  {
    // Roll PID
    if (enableRollPid)
    {
      InputRoll = rollK;
      errorRoll = abs(SetpointRoll - rollK); //distance away from setpoint
      if(errorRoll<thresholdRoll)
      {  //we're close to setpoint, use conservative tuning parameters
        myRollPID.SetTunings(consKpRoll, consKiRoll, consKdRoll);
      }
      else if(errorRoll>=thresholdRoll && errorRoll<thresholdFarRoll)
      {
        //we're far from setpoint, use aggressive tuning parameters
        myRollPID.SetTunings(aggKpRoll, aggKiRoll, aggKdRoll);
      }
      else if(errorRoll>thresholdFarRoll)
      {
        //we're far from setpoint, use aggressive tuning parameters
        myRollPID.SetTunings(farKpRoll, farKiRoll, farKdRoll);
      }

      myRollPID.Compute(); // Computes outputRoll

      if (printPIDVals)
      {
        Serial.print("ErrorRoll:");
        Serial.print(errorRoll);
        Serial.print("Roll PID: ");
        Serial.print(OutputRoll);
        Serial.println();
      }
    }
    else
    {
      OutputRoll = 0;
    }

    // Pitch PID
    if (enablePitchPid)
    {
      InputPitch = pitchK;
      errorPitch = abs(SetpointPitch - pitchK); //distance away from setpoint

      if (errorPitch<thresholdPitch)
      {  //we're close to setpoint, use conservative tuning parameters
        myPitchPID.SetTunings(consKpPitch, consKiPitch, consKdPitch);
      }
      else if (errorPitch>=thresholdPitch && errorPitch<thresholdFarPitch)
      {
        //we're far from setpoint, use aggressive tuning parameters
        myPitchPID.SetTunings(aggKpPitch, aggKiPitch, aggKdPitch);
      }
      else if (errorPitch>=thresholdFarPitch)
      {
        //we're really far from setpoint, use other tuning parameters
        myPitchPID.SetTunings(farKpPitch, farKiPitch, farKdPitch);
      }

      myPitchPID.Compute(); // Computes outputPitch

      if (printPIDVals)
      { 
        Serial.print("  Error Pitch: ");
        Serial.print(errorPitch);
        Serial.print("  pidAction: ");
        Serial.print(OutputPitch);
        Serial.println();
      }
    }  
    else
    {
      OutputPitch = 0;
    }

    // Yaw PID
    if (enableYawPid)
    {
      if (filterAng == 1)
      {
        InputYaw = angPosFilter[2];
        errorYaw = abs(SetpointYaw - angPosFilter[2]); //distance away from setpoint
      }
      else if (filterAng == 0)
      {
        InputYaw = bearing1;
        errorYaw = abs(SetpointYaw - bearing1); 
      }

      if(errorYaw<thresholdYaw)
      {
        //we're close to setpoint, use conservative tuning parameters
        myYawPID.SetTunings(consKpYaw, consKiYaw, consKdYaw);
      }
      else
      {
        //we're far from setpoint, use aggressive tuning parameters
        myYawPID.SetTunings(aggKpYaw, aggKiYaw, aggKdYaw);
      }    
      myYawPID.Compute(); // Resturns outputYaw 

      if (printPIDVals)
      {
        Serial.print(" ErrorYaw: ");
        Serial.print(errorYaw);
        Serial.print(" Yawpid: ");
        Serial.print(OutputYaw);
        Serial.println();
      }     
    }
    else
    {
      OutputYaw=0;
    }
    if (printPIDVals)
    {
      Serial.println();      
    }
  }
  else
  {
    OutputRoll = 0;
    OutputPitch = 0;    
    OutputYaw = 0;
  }
}


// FreeIMU orientation - TODO ** More work needed **
void detOrientation()
{
  if (enableIMUfiltering)
  {
    getYawPitchRoll(ypr); 
    if (printOrientationFreeIMU)
    {
      Serial.println();
      Serial.print("Yaw: ");
      Serial.print(ypr[0]);
      Serial.print(" Pitch: ");
      Serial.print(ypr[1]);
      Serial.print(" Roll: ");
      Serial.print(ypr[2]);
      Serial.println("");
    }
  }
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
  angPosFilter[0] = roll1;
  angPosFilter[1] = pitch1;
  angPosFilter[2] = bearing1;
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

void xbeeRoutine()
{
  if (Serial.available()) 
  {
    GotChar = Serial.read();

    if (GotChar == 'X')
    {
      if (!cmplx)
      {
        // begin of the string
        cmplx =true;
      }
      else
      {
        // end of the string - reset values
        cmplx=false;
        optCount=0;
        /* Create the double from num1 and num2 */
        complNum=num1+(num2/(counter));
        /* Reset the variables for the next round */
        Serial.print("     opt1: ");
        Serial.print(options1[opt1]); 

        Serial.print("     opt2: ");
        Serial.print(options2[opt2]); 

        Serial.print("     opt3: ");
        Serial.print(options3[opt3]); 

        Serial.print("    Value: ");
        Serial.print(complNum);

        // Roll 
        if (opt1==0 && opt2==0 && opt3 ==0)
          consKpRoll = complNum;
        else if (opt1==0 && opt2==0 && opt3 ==1)
          consKdRoll = complNum;
        else if (opt1==0 && opt2==0 && opt3 ==2)
          consKiRoll = complNum;
        else if (opt1==0 && opt2==1 && opt3 ==0)
          aggKpRoll = complNum;
        else if (opt1==0 && opt2==1 && opt3 ==1)
          aggKdRoll = complNum;
        else if (opt1==0 && opt2==1 && opt3 ==2)
          aggKiRoll = complNum;
        // Pitch
        else if (opt1==1 && opt2==0 && opt3 ==0)
          consKpPitch = complNum;
        else if (opt1==1 && opt2==0 && opt3 ==1)
          consKdPitch = complNum;
        else if (opt1==1 && opt2==0 && opt3 ==2)
          consKiPitch = complNum;
        else if (opt1==1 && opt2==1 && opt3 ==0)
          aggKpPitch = complNum;
        else if (opt1==1 && opt2==1 && opt3 ==1)
          aggKdPitch = complNum;
        else if (opt1==1 && opt2==1 && opt3 ==2)
          aggKiPitch = complNum;  

        // Yaw
        else if (opt1==2 && opt2==0 && opt3 ==0)
          consKpYaw = complNum;
        else if (opt1==2 && opt2==0 && opt3 ==1)
          consKdYaw = complNum;
        else if (opt1==2 && opt2==0 && opt3 ==2)
          consKiYaw = complNum;
        else if (opt1==2 && opt2==1 && opt3 ==0)
          aggKpYaw = complNum;
        else if (opt1==2 && opt2==1 && opt3 ==1)
          aggKdYaw = complNum;
        else if (opt1==2 && opt2==1 && opt3 ==2)
          aggKiYaw = complNum;   

        // Altitude
        else if (opt1==3 && opt2==0 && opt3 ==0)
          consKpAltitude = complNum;
        else if (opt1==3 && opt2==0 && opt3 ==1)
          consKdAltitude = complNum;
        else if (opt1==3 && opt2==0 && opt3 ==2)
          consKiAltitude = complNum;
        else if (opt1==3 && opt2==1 && opt3 ==0)
          aggKpAltitude = complNum;
        else if (opt1==3 && opt2==1 && opt3 ==1)
          aggKdAltitude = complNum;
        else if (opt1==3 && opt2==1 && opt3 ==2)
          aggKiAltitude = complNum;   

        num1=0;
        num2=0;
        complNum=0;
        counter=1;
        mySwitch=false;
        numOfDec=0;
      }
    }

    if (GotChar==44)
    {
      // Comma
      optCount++;
      Serial.println();
      Serial.print("virgola numero: ");
      Serial.println(optCount);
      letterCount = 0;
    }

    //listen for numbers between 0-9
    if(GotChar>47 && GotChar<58)
    {
      //number found
      if (cmplx)
      {
        if (optCount == 1)
        {
          opt1 = GotChar - 48; 
          Serial.print(opt1);
        }
        else if (optCount == 2)
        {
          opt2 = GotChar - 48; 
          Serial.print(opt2);
        }
        else if (optCount == 3)
        {
          opt3 = GotChar - 48; 
          Serial.print(opt3);
        }
        if (optCount == 4)
        {
          /* If mySwitch is true, then populate the num1 variable
           otherwise populate the num2 variable*/
          if(!mySwitch)
          {
            num1=(num1*10)+(GotChar-48);
          }
          else
          {
            num2=(num2*10)+(GotChar-48);           
            /* These counters are important */
            counter=counter*10;
            numOfDec++;
          }
        }
      }
    }      
    else if(GotChar == 'A')
    {  	
      storeAccData = true;
    }          
    else if(GotChar == 'J')
    {  	
      storeAccData = false;
    }  
    else if (GotChar==46)
    {
      mySwitch=true;
    }
    else if(GotChar == '8')
    {  	 
      plotting = true;
      Serial.println(" Sending plot data from now on.");
    } 
    else if(GotChar == '9')
    {  	
      plotting = false; 
      Serial.println(" Stop sentind data to Matlab.");
    } 
    else if(GotChar == 'i')
    {  	
      initialize();
    } 
    else if(GotChar == 'M')
    {  	
      matlab = true;
    }
    else if(GotChar == 'N')
    {  	
      matlab = false;
    }
    else if(GotChar == 'B')
    {  	
      getBmpValues();
    }
    else if(GotChar == 'G')
    {  	
      enablePid = false;
    }
    else if(GotChar == 'T')
    {  	
      enablePid = true;
    }  
    else if(GotChar == 'r')
    {  	
      resetMotorsPidOff();
    } 
    else if (GotChar == 'P')
    {
      dispActualAllPidVals();
    }
    else if(GotChar == 'z')
    { 
      if (alphaA_xy>0.01)
      {  	
        alphaA_xy = alphaA_xy - 0.01;
        Serial.println();
        Serial.print(alphaA_xy);
        Serial.println();
      }
    } 
    else if(GotChar == 'a')
    { 
      if (alphaA_xy<1)
      { 	
        alphaA_xy = alphaA_xy + 0.01;
        Serial.println();
        Serial.print(alphaA_xy);
        Serial.println();
      }
    } 
    else if(GotChar == 'o')
    { 
      if (alphaW<1)
      {  	
        alphaW = alphaW + 0.01;
        Serial.println();
        Serial.print(alphaW);
        Serial.println();
      }
    } 
    else if(GotChar == 'l')
    { 
      if (alphaW>0.01)
      {  	
        alphaW = alphaW - 0.01;
        Serial.println();
        Serial.print(alphaW);
        Serial.println();
      }
    }
    else if(GotChar == 'h')
    { 
      if (alphaA_z<1)
      {  	
        alphaA_z = alphaA_z + 0.01;
        Serial.println();
        Serial.print(alphaA_z);
        Serial.println();
      }
    } 
    else if(GotChar == 'n')
    { 
      if (alphaA_z>0.01)
      {  	
        alphaA_z = alphaA_z - 0.01;
        Serial.println();
        Serial.print(alphaA_z);
        Serial.println();
      }
    }
    else if(GotChar == 'j')
    { 
      if (alphaAK<1)
      {  	
        alphaAK = alphaAK + 0.01;
        Serial.println();
        Serial.print(alphaAK);
        Serial.println();
      }
    } 
    else if(GotChar == 'm')
    { 
      if (alphaAK>0.01)
      {  	
        alphaAK = alphaAK - 0.01;
        Serial.println();
        Serial.print(alphaAK);
        Serial.println();
      }
    }
    // Complementary filter Constant
    else if(GotChar == 'c')
    { 
      if (complementaryConstant>0.01)
      {  	
        complementaryConstant = complementaryConstant - 0.01;
        Serial.println();
        Serial.print(complementaryConstant);
        Serial.println();
      }
    }
    else if(GotChar == 'v')
    { 
      if (complementaryConstant<1)
      {  	
        complementaryConstant = complementaryConstant + 0.01;
        Serial.println();
        Serial.print(complementaryConstant);
        Serial.println();
      }
    }  
    if (GotChar == 'w')
    {
      if (throttle<thresholdUp)
      {
        throttle = throttle + 5;
      }
    }
    else if (GotChar == 's')
    {
      if (throttle>thresholdDown)
      {
        throttle = throttle - 5;
      }
    }

    if (GotChar == 'e')
    {
      if (throttle<thresholdUp)
      {
        throttle = throttle + 1;
      }
    }
    else if (GotChar == 'd')
    {
      if (throttle>thresholdDown)
      {
        throttle = throttle - 1;
      }
    }

    if (GotChar== '1')
    {
      Serial.println("M1");
      testMotor(1);
    }
    else if (GotChar== '2')
    {
      testMotor(2);
    }
    else if (GotChar== '3')
    {
      testMotor(3);
    }
    else if (GotChar== '4')
    {
      testMotor(4);
    }
    else if(GotChar == 'L')
    {
      land();
    }
  }

  if(xbee.available()>0)
  { 
    if (xbee.available() >= inputBuffSize)
    {     
      // Store byte in buffer
      for (int j=0;j<=inputBuffSize;j++)
      {
        bufferBytes[j]=xbee.read();
        Serial.print("Received: ");
        Serial.print(bufferBytes[j]);
        Serial.println();
      }      
      if (bufferBytes[0] == 2 && bufferBytes[1]==1)
      {
        /**
         * Message has been correctly sent by MATLAB and delivered to the Arduino 
         * Decoding Header
         **/

        // Assembling VERSION long skip first two bytes

        //hiBytew1 = bufferBytes[2];
        //Serial.println(hiBytew1,BIN);
        //loBytew1 = bufferBytes[3];
        //Serial.println(loBytew1,BIN);
        //loWord = word(hiBytew1, loBytew1);
        //Serial.println(loWord,BIN);
        hiBytew2 = bufferBytes[4];
        //Serial.println(hiBytew2,BIN);
        loBytew2 = bufferBytes[5];
        //Serial.println(loBytew2,BIN);
        hiWord = word(hiBytew2, loBytew2);
        //versionProtocol = makeLong( hiWord, loWord);
        int versionProtocol = hiWord;
        if (printVerboseSerial)
        {
          Serial.println();
          Serial.print("Version");
          Serial.print(versionProtocol);
        }
        if (versionProtocol != versionArduinoProtocol)
          Serial.println("Warning Sync Repos, different serial protocols");

        //  number cmd
        int numCmd = bufferBytes[6];
        int headL = bufferBytes[7];
        int cmdL = bufferBytes[8];
        //  total Length
        hiBytew2 = bufferBytes[9];
        //Serial.println(hiBytew2,BIN);
        loBytew2 = bufferBytes[10];
        //Serial.println(loBytew2,BIN);
        short totL = word(hiBytew2, loBytew2);
        if (printVerboseSerial)
        {
          Serial.println();
          Serial.print("Tot Len");
          Serial.println(totL);
        }
        // CRC
        hiBytew2 = bufferBytes[11];
        //Serial.println(hiBytew2,BIN);
        loBytew2 = bufferBytes[12];
        //Serial.println(loBytew2,BIN);
        short crc = word(hiBytew2, loBytew2);
        if (printVerboseSerial)
        {
          Serial.println();
          Serial.print("Crc");
          Serial.println(crc);
        }

        /**
         * Decoding Command
         **/
        int type = bufferBytes[13];

        if (type == connID)
        {
          // Connection channel
          hiBytew2 = bufferBytes[16];
          loBytew2 = bufferBytes[17];
          int conAck = word(hiBytew2, loBytew2); 

          if (conAck == 1)
          {
            if (!matlab)
            {
              sendConnAck = true;
              sendTenzoState = true;
            }
            else 
            {
              sendConnAck = true;
              sendTenzoState = true;
              // Connection requested but already connected
              Serial.println();
              Serial.println(" Warning!! Something wrong during CONnection sync.");
              Serial.print("Connection requested but already connected");
              Serial.println();
            }
          }
          else if (conAck == 3)
          {
            // Communication problems
            if (!matlab)
            {
              sendConnAck = true;
              sendTenzoState = true;
              Serial.println();
              Serial.println(" Warning!! Communication problems. Sending again.");
              Serial.println();
            }
            else 
            {
              sendConnAck = true;
              sendTenzoState = true;
              // Impossible: Connection requested but already connected
              Serial.println();
              Serial.println(" Warning!! Something wrong during CONnection sync.");
              Serial.print("Connection requested but already connected");
              Serial.println();
            }
          }
          else if (conAck == 0)
          {
            if (matlab)
            {
              sendDisconnAck = true;
            }
            else 
            {
              sendDisconnAck = true;
              // Impossible: Disconnection requested but already disconnected
              Serial.println();
              Serial.println(" Warning!! Something wrong during DISconnection sync.");
              Serial.print("Disconnection requested but already disconnected");
              Serial.println();
            }
          }
          else
          {
            //  Shouldn't get here - Wrong mess received
            Serial.println();
            Serial.println(" Warning!! Third Condition.");
            Serial.println();
          }
        }
        else if (type == tenzoStateID)
        {
          hiBytew2 = bufferBytes[16];
          loBytew2 = bufferBytes[17];
          int remoteTakeOffTemp = word(hiBytew2, loBytew2);

          hiBytew2 = bufferBytes[20];
          loBytew2 = bufferBytes[21];
          int remoteHoverTemp = word(hiBytew2, loBytew2);

          hiBytew2 = bufferBytes[24];
          loBytew2 = bufferBytes[25];
          int remoteLandTemp = word(hiBytew2, loBytew2);

          if (printAckCommands) 
          {
            // Impossible: Disconnection requested but already disconnected
            Serial.println();
            Serial.println(" | Check state requested |");
            Serial.println();
          }

          checkLocalRemoteStates(remoteTakeOffTemp,remoteHoverTemp,remoteLandTemp);

        }
        else if (type == motorsID)
        {
          // Motors : same cmd   

          stopSendingToMatlab();

          hiBytew2 = bufferBytes[16];
          loBytew2 = bufferBytes[17];
          int motorSpeedChange = word(hiBytew2, loBytew2);
          motorSpeedChange = motorSpeedChange - 100;

          hiBytew2 = bufferBytes[20];
          loBytew2 = bufferBytes[21];
          int m2 = word(hiBytew2, loBytew2);
          m2 = m2 - 100;

          hiBytew2 = bufferBytes[24];
          loBytew2 = bufferBytes[25];
          int m3 = word(hiBytew2, loBytew2);
          m3 = m3 -100;

          hiBytew2 = bufferBytes[28];
          loBytew2 = bufferBytes[29];
          int m4 = word(hiBytew2, loBytew2);
          m4 = m4 -100;

          if (motorSpeedChange == 10)
          {
            Serial.println("M1");
            testMotor(1);
          }
          else if (m2 == 10)
          {
            Serial.println("M2");
            testMotor(2);
          }
          else if (m3 == 10)
          {
            Serial.println("M3");
            testMotor(3);  
          }
          else if (m4 == 10)
          { 
            Serial.println("M4");
            testMotor(4);
          }
          else if (motorSpeedChange > 1)
          {
            if (throttle<thresholdUp)
            {
              throttle = throttle + motorSpeedChange;
              if (printVerboseSerial)
              {
                Serial.println();
                Serial.print("new Speed: ");
                Serial.print(motorSpeedChange);
                Serial.println(); 
              }
              sendMotorsToMatlab = true;
            }
          }
          else if (motorSpeedChange < -1)
          {
            if (throttle>thresholdDown)
            {
              throttle = throttle + motorSpeedChange;
              if (printVerboseSerial)
              {
                Serial.println();
                Serial.print("new Speed: ");
                Serial.print(motorSpeedChange);
                Serial.println(); 
              }
              sendMotorsToMatlab = true;
            } 
          }
        }
        else if (type == accID)
        {
          // Send Acc Values
          stopSendingToMatlab();
          sendAccToMatlab = true;
        }
        else if (type == gyroID)
        {
          // Send Gyro Values
          stopSendingToMatlab();
          sendGyroToMatlab = true;
        }
        else if (type == magnID || type == estID)
        {
          // Send Est + Magn Values
          stopSendingToMatlab();
          sendMagnToMatlab = true;
          sendEstToMatlab = true;
        }
        else if (type == takeOffID)
        {
          // Channel 17: Take Off
          stopSendingToMatlab();

          hiBytew2 = bufferBytes[16];
          loBytew2 = bufferBytes[17];
          int altitudeRef = word(hiBytew2, loBytew2);
          if (printVerboseSerial)
          {
            Serial.println();
            Serial.print("Channel: 17   altitudeRef: ");
            Serial.print(altitudeRef);
            Serial.println();
          }
          // TODO set altitude reference when initiate
          if (!initialized)
          {
            initialize();
          }
          else
          {
            Serial.println(" Already out in space");
          }
        }
        else if (type == landID)
        {
          // Channel 19: Land
          stopSendingToMatlab();

          hiBytew2 = bufferBytes[16];
          loBytew2 = bufferBytes[17];
          landSpeed = word(hiBytew2, loBytew2);
          if (printVerboseSerial)
          {
            Serial.println();
            Serial.print("Channel: 19   landSpeed: ");
            Serial.print(landSpeed);
            Serial.println();
          }
          // TODO: Aggressive or kind stop
          if (initialized)
          {
            land();
          }
          else
          {
            Serial.println();
            Serial.print("Received Land command but Tenzo is not hovering!! WARNING!!");
            Serial.println();
          }
        }
        else if (type == iHoverID)
        {
          // Channel 18: Hovering
          if (initialized)
          {
            // Hover with PID
            stopSendingToMatlab();

            hiBytew2 = bufferBytes[16];
            loBytew2 = bufferBytes[17];
            int enab = word(hiBytew2, loBytew2);             
            if (printVerboseSerial)
            {
              Serial.println();
              Serial.print("Channel: 18   enab Pid: ");
              Serial.print(enab);
              Serial.println();
            }
            if (enab==0 && enablePid)
            {
              enablePid = false;
              sendHoverState = true;
              numCommandToSend++;
            }
            else if (enab==0 && !enablePid)
            {
              Serial.println();
              Serial.print("Received Pid 'switch off' request but has already been disabled! WARNING!!");
              Serial.println();
            }
            else if (enab == 1 && enablePid)
            {
              Serial.println();
              Serial.print("Received Pid activation request but already enable! WARNING!!");
              Serial.println();
            }
            else if (enab == 1 && !enablePid)
            {
              enablePid = true;
              sendHoverState = true;
              numCommandToSend++;               
            }
          }
          else
          {
            Serial.println();
            Serial.print("Received Hovering command but Tenzo is not flying!! WARNING!!");
            Serial.println();             
          }
        }
        else if (type == enableMotorsID) // Deprecated
        {
          // Request Motors Data  type: 20  
          stopSendingToMatlab();

          hiBytew2 = bufferBytes[16];
          loBytew2 = bufferBytes[17];
          int enab = word(hiBytew2, loBytew2);
          if (enab==0)
            sendMotorsToMatlab = false;
          else if (enab == 1)
            sendMotorsToMatlab = true;
        }
        else if (type == sendConsPidRollID || type == sendAggPidRollID)
        {
          // Request Motors Data   type: 21      || 25  
          stopSendingToMatlab();
          sendPidState = true;
          sendRollToMatlab = true;          
          if (printVerboseSerial)
          {
            Serial.println();
            Serial.print("Channel: 21||25   roll Pid? ");
            Serial.println();
          }
        }
        else if (type == sendConsPidPitchID || type == sendAggPidPitchID)
        {
          // Request Motors Data   type: 22      || 26 
          stopSendingToMatlab();
          sendPidState = true;
          sendPitchToMatlab = true;         
          if (printVerboseSerial)
          {
            Serial.println();
            Serial.print("Channel: 22||26   pitch Pid? ");
            Serial.println();
          }
        }
        else if (type == sendConsPidYawID || type == sendAggPidYawID)
        {
          // Request Motors Data   type: 23      || 27
          stopSendingToMatlab();
          sendPidState = true;
          sendYawToMatlab = true;         
          if (printVerboseSerial)
          {
            Serial.println();
            Serial.print("Channel: 23||27   yaw Pid? ");
            Serial.println();
          }
        }
        else if (type == sendConsPidAltID || type == sendAggPidAltID)
        {
          // Request Motors Data   type: 24      || 28
          stopSendingToMatlab();
          sendPidState = true;
          sendAltToMatlab = true;         
          if (printVerboseSerial)
          {
            Serial.println();
            Serial.print("Channel: 24||28   alt Pid? ");
            Serial.println();
          }
        }
        else 
        {
          stopSendingToMatlab();

          hiBytew2 = bufferBytes[16];
          loBytew2 = bufferBytes[17];
          cmd1 = word(hiBytew2, loBytew2);

          hiBytew2 = bufferBytes[20];
          loBytew2 = bufferBytes[21];
          cmd2 = word(hiBytew2, loBytew2);

          hiBytew2 = bufferBytes[24];
          loBytew2 = bufferBytes[25];
          cmd3 = word(hiBytew2, loBytew2);

          hiBytew2 = bufferBytes[28];
          loBytew2 = bufferBytes[29];
          cmd4 = word(hiBytew2, loBytew2);

          if (type == 9)
          {
            // Pid Cons ROLL 
            consKpRoll = (double) cmd1/1000;
            consKdRoll = (double) cmd2/1000;
            consKiRoll = (double) cmd3/1000;
            SetpointRoll = cmd4;
            sendPidState = true;
            changePidValues();
            sendRollToMatlab = true;        
            if (printVerboseSerial)
            {
              Serial.println();
              Serial.println("AA Cons Roll Pid ");
              Serial.println(consKpRoll);
              Serial.println(consKdRoll);
              Serial.println(consKiRoll);
              Serial.println(SetpointRoll);
              Serial.println();
            }
          }
          else if (type == 13)
          {
            // Pid AGG ROLL 
            aggKpRoll = (double) cmd1/1000;
            aggKdRoll = (double) cmd2/1000;
            aggKiRoll = (double) cmd3/1000;
            SetpointRoll = cmd4;
            sendPidState = true;
            changePidValues();
            sendRollToMatlab = true;      
            if (printVerboseSerial)
            {
              Serial.println();
              Serial.println("Agg Roll Pid ");
              Serial.println(aggKpRoll);
              Serial.println(aggKdRoll);
              Serial.println(aggKiRoll);
              Serial.println(SetpointRoll);
              Serial.println();
            }
          }
          else if (type == 10)
          {
            // Pid Cons PITCH 
            consKpPitch = (double) cmd1/1000;
            consKdPitch = (double) cmd2/1000;
            consKiPitch = (double) cmd3/1000;
            SetpointPitch = cmd4;
            sendPidState = true;
            changePidValues();
            sendPitchToMatlab = true;
            if (printVerboseSerial)
            {
              Serial.println();
              Serial.println("cons pitch Pid ");
              Serial.println(consKpPitch);
              Serial.println(consKdPitch);
              Serial.println(consKiPitch);
              Serial.println(SetpointRoll);
              Serial.println();
            }
          }
          else if (type == 14)
          {
            // Pid AGG PITCH 
            aggKpPitch = (double) cmd1/1000;
            aggKdPitch = (double) cmd2/1000;
            aggKiPitch = (double) cmd3/1000;
            SetpointPitch = cmd4;
            sendPidState = true;
            changePidValues();
            sendPitchToMatlab = true;
            if (printVerboseSerial)
            {
              Serial.println();
              Serial.println("Agg Roll Pid ");
              Serial.println(aggKpPitch);
              Serial.println(aggKdPitch);
              Serial.println(aggKiPitch);
              Serial.println(SetpointRoll);
              Serial.println();
            }
          }
          else if (type == 11)
          {
            // Pid Cons YAW 
            consKpYaw = (double) cmd1/1000;
            consKdYaw = (double) cmd2/1000;
            consKiYaw = (double) cmd3/1000;
            SetpointYaw = cmd4;
            sendPidState = true;
            changePidValues();
            sendYawToMatlab = true;
          }
          else if (type == 15)
          {
            // Pid AGG YAW 
            aggKpYaw = (double) cmd1/1000;
            aggKdYaw = (double) cmd2/1000;
            aggKiYaw = (double) cmd3/1000;
            SetpointYaw = cmd4;
            sendPidState = true;
            changePidValues();
            sendYawToMatlab = true;
          }
          else if (type == 12)
          {
            // Pid Cons ALTITUDE 
            consKpAltitude = (double) cmd1/1000;
            consKdAltitude = (double) cmd2/1000;
            consKiAltitude = (double) cmd3/1000;
            SetpointAltitude = cmd4;
            sendPidState = true;
            changePidValues();
            sendAltToMatlab = true;
          }
          else if (type == 16)
          {
            // Pid AGG ALTITUDE 
            aggKpAltitude = (double) cmd1/1000;
            aggKdAltitude = (double) cmd2/1000;
            aggKiAltitude = (double) cmd3/1000;
            SetpointAltitude = cmd4;
            sendPidState = true;
            changePidValues();
            sendAltToMatlab = true;
          }           
        }
      } // Message to Arduino from Matlab
    }
    else if (xbee.available() > 0)
    { 
      getData = xbee.read();  

      Serial.println();
      Serial.print("Received: ");
      Serial.print(getData);
      Serial.println();

      // Establishing Matlab Communication  
      if(getData == 16)
      {  	 
        byte message[] = {
          0x11        };
        xbee.write(message, sizeof(message));

        Serial.println();
        Serial.print(" Connection requested. Sending to Matlab:  ");
        Serial.println();
        Serial.print(message[0]);
        sendTenzoState = true;
      } 
      else if (getData == 18)
      {
        Serial.println(" Matlab communication established. ");
        matlab = true;
      }
      else if(getData == 19)
      {  	 
        Serial.println(" [Warning] Matlab communication problem. ");
      }  
      else if (getData == 20)
      {  	 
        byte message[] = {
          0x15        };
        xbee.write(message, sizeof(message));

        Serial.println();
        Serial.print(" Sending to Matlab:  ");
        Serial.println(message[0]);  
        Serial.println(" Disconnected. ");
        Serial.println(); 

        matlab = false;
      }
    }
  }
  if (initialized||matlab)
  {
    //motorSpeed(throttle);
    motorSpeedPID(throttle,OutputPitch ,OutputRoll, OutputYaw);
    if (printThrottle)
    {
      Serial.println(throttle);
    }
  }
}

void changePidValues()
{
  if (enablePid)
  {
    // Roll PID
    if (enableRollPid)
    {
      if(errorRoll<thresholdRoll)
      {  //we're close to setpoint, use conservative tuning parameters
        myRollPID.SetTunings(consKpRoll, consKiRoll, consKdRoll);
      }
      else
      {
        //we're far from setpoint, use aggressive tuning parameters
        myRollPID.SetTunings(aggKpRoll, aggKiRoll, aggKdRoll);
      }
    }

    // Pitch PID
    if (enablePitchPid)
    {
      if(errorPitch<thresholdPitch)
      {  //we're close to setpoint, use conservative tuning parameters
        myPitchPID.SetTunings(consKpPitch, consKiPitch, consKdPitch);
      }
      else
      {
        //we're far from setpoint, use aggressive tuning parameters
        myPitchPID.SetTunings(aggKpPitch, aggKiPitch, aggKdPitch);
      }
    }

    // Yaw PID
    if (enableYawPid)
    {
      if(errorYaw<thresholdYaw)
      {
        //we're close to setpoint, use conservative tuning parameters
        myYawPID.SetTunings(consKpYaw, consKiYaw, consKdYaw);
      }
      else
      {
        //we're far from setpoint, use aggressive tuning parameters
        myYawPID.SetTunings(aggKpYaw, aggKiYaw, aggKdYaw);
      } 
    }
    if (printPIDVals)
    {
      dispActualAllPidVals();     
    }
  }
}

void checkLocalRemoteStates(int to, int h, int l)
{
  boolean toCond = false, hCond = false, lCond = false;
  // Checks whether states have been correctly synced
  if (initialized && to == 1)
  {
    toCond = true;
  } 
  else if (!initialized && to == 0)
  {
    toCond = true;
  } 

  if (enablePid && h==1)
  {
    hCond = true;
  }
  else if (!enablePid && h==0)
  {   
    hCond = true;
  }

  if (initialized && l==0)
  {
    lCond = true;
  }
  else if (!initialized && l==1)
  {   
    lCond = true;
  } 

  if (lCond && hCond && toCond)
  {
    matlab = true;
    if (printAckCommands)
    {
      Serial.println();
      Serial.print("[Sync] Tutto a porno");
      Serial.println();
    }
  }
  else
  {
    sendTenzoState = true;
    if (printAckCommands)
    {
      Serial.println();
      Serial.print("[Sync] state problem between Arduino (local) and Matlab (remote). Sending states... ");
      Serial.println();
    }
  }
}

void stopSendingToMatlab()
{  
  sendAccToMatlab = false;
  sendGyroToMatlab = false;
  sendMagnToMatlab = false;
  sendEstToMatlab = false;
  sendRollToMatlab = false;
  sendMotorsToMatlab = false;
  sendPitchToMatlab = false;
  sendYawToMatlab = false;
  sendAltToMatlab = false;
}

void resetMotorsPidOff()
{
  throttle = 0;
  motorSpeed(0);
  // Sets timerStart to 0
  timerStart = 0;
  checkpoint = 0;
  // Disable Pid when motors are off
  if (enablePid)
  {
    // Change only if PID is enabled 
    // Tenzo has landed, no need of controls
    changePidState(false);
  }
}

void resetMotors()
{
  throttle = 0;
  motorSpeed(0);
  // Sets timerStart to 0
  timerStart = 0;
  checkpoint = 0;
  // Disable Pid when motors are off
}

void protocol1()
{  
  if (autoLand)
  {
    // If motors are on updates timer
    if (initialized)
      timerStart = millis() - checkpoint;
    // if Tenzo has already been initialized for a timeToLand period then land
    if (initialized && timerStart>=timeToLand)
    {
      land();
    }
  }
}
void land()
{
  if (initialized)
  {
    landing = true;
    Serial.println();
    Serial.print("Landing protocol started...");
    ;
    Serial.print(throttle);
    Serial.print(" ");
    for (int j=throttle; j>40 ;j--)
    {
      motorSpeed(j);
      Serial.println(j);
      // Kind or brutal land
      if (landSpeed == 1)
        delay(motorRampDelayFast);
      else if (landSpeed == 2)
        delay(motorRampDelayMedium);
      else if (landSpeed == 3)
        delay(motorRampDelaySlow);
      else
        delay(motorRampDelayVerySlow);
    }
    resetMotorsPidOff();
    initialized = false;
    // Notify Matlab after landing
    if (matlab)
    {
      sendLandAck = true;
      numCommandToSend++;
    }
    //Serial.print("   finished");
    landing = false;
  }
  else
  {
    Serial.println();
    Serial.print("Land command received but Tenzo is not Flying   !! WARNING !!");
    Serial.println();
  }

}

void initialize()
{
  if (!initialized)
  {
    initializing = true;
    if (!autoEnablePid)
    {
      resetMotorsPidOff();
    }
    else
    {
      resetMotors();
      enablePid = true;
    }
    delay(500);
    for (int j=0; j<rampTill;j++)
    {
      motorSpeed(j);
      Serial.println(j);
      delay(motorRampDelayFast); 
    }
    checkpoint = millis();
    throttle=rampTill;
    initialized = true;
    // Notify Matlab after Take Off  
    if (matlab)
    {
      sendTakeOffAck =true;
      numCommandToSend++;
    }
    if (enablePid)
    {  
      changePidState(true);
    }
    else
    {
      changePidState(false);
    }
    initializing = false;
  }
  else
  {
    Serial.println();
    Serial.print("First Land ortherwise Tenzo will crash");
    Serial.println();
  }
}

void changePidState(boolean cond)
{
  if (matlab)
  {
    // If pid state is modified, notify Matlab
    /*
    if (enablePid == cond && matlab)
     {
     sendHoverState = false;
     }
     */
    /*
     * AEP + TO
     **/
    if (enablePid && cond && autoEnablePid) 
    {
      Serial.println();
      Serial.print("   eP + cond + AEP ");
      Serial.println();

      sendHoverState = true;
      numCommandToSend++;
    }
    else if (!enablePid && !cond && autoEnablePid)
    {    
      Serial.println();
      Serial.print("   !eP + !cond + AEP ");
      Serial.println();

      sendHoverState = true;
      numCommandToSend++;
    }
    /*
     * !AEP + TO
     **/
    else if (!enablePid && !cond && !autoEnablePid)
    {
      Serial.println();
      Serial.print("   !eP + !cond + !AEP ");
      Serial.println();

      sendHoverState = false;
    }
    /** 
     *  AEP + Land
     **/
    else if (enablePid && !cond && landing)
    {
      Serial.println();
      Serial.print("   eP + !cond + AEP + Land ");
      Serial.println();

      sendHoverState = true;
      numCommandToSend++;
    }
    else if (enablePid && !cond)
    {
      sendHoverState = true;
      numCommandToSend++;
    }
    else if (enablePid != cond )
    {
      sendHoverState = true;
      numCommandToSend++;
    }
  }
  if (cond)
  {
    // Enable Pid Actions
    myRollPID.SetMode(AUTOMATIC);
    //tell the PID to range between 0 and the full throttle
    //SetpointRoll = 0;
    myRollPID.SetOutputLimits(-500, 500);

    // Pitch
    myPitchPID.SetMode(AUTOMATIC);
    //SetpointPitch = 0;
    //tell the PID to range between 0 and the full throttle
    myPitchPID.SetOutputLimits(-500, 500);

    // Yaw
    myYawPID.SetMode(AUTOMATIC);
    //SetpointYaw=0;
    //tell the PID to range between 0 and the full throttle
    myYawPID.SetOutputLimits(-500, 500);
    enablePid = true;
  }
  else
  { 
    myRollPID.SetMode(MANUAL);
    myPitchPID.SetMode(MANUAL);
    myYawPID.SetMode(MANUAL);
    enablePid = false;
  }
}

void motorSpeedPID(int thr, int rollpid, int pitchpid, int yawpid)
{
  int motorA, motorB, motorC, motorD;

  // compute motor inputs
  motorA = thr + rollpid - yawpid;
  motorB = thr + pitchpid + yawpid;
  motorC = thr - rollpid - yawpid;
  motorD = thr - pitchpid + yawpid; 
  if (printMotorsVals)
  {
    Serial.println();
    Serial.print(" Motors:[  ");
    Serial.print(motorA);
    Serial.print("   ;   ");
    Serial.print(motorB);
    Serial.print("   ;   ");
    Serial.print(motorC);
    Serial.print("   ;   ");
    Serial.print(motorD);
    Serial.print("   ]");  
    Serial.print("        PID:[ROLL  ");
    Serial.print(rollpid);
    Serial.print("   ;PITCH   ");
    Serial.print(pitchpid);
    Serial.print("   ;YAW   ");
    Serial.print(yawpid);
    Serial.print("   ; THROTTLE    ");
    Serial.print(thr);
    Serial.print("   ]");
    Serial.println();
  }
  // send input to motors
  servo1.write(motorA);
  servo2.write(motorB);
  servo3.write(motorC);
  servo4.write(motorD);
}

void motorSpeed(int x)
{    
  servo1.write(x);      
  servo2.write(x); 
  servo3.write(x); 
  servo4.write(x); 
}

void sendDataSensors(boolean device)
{
  if (!device)
  {
    // Not yet connected. 
    if (sendConnAck)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
      numCommandToSend++;

      // Channel 31: Connection
      pMyCmd->cmd = connID;
      pMyCmd->param1 = 2; 
      sendConnAck = false;
      sendCommandToMatlab(); 
      if (printAckCommands)
      {
        Serial.println();
        Serial.println("Sending Connection Ack to Matlab: ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      }      
    } 
  }
  else if (device)
  {    
    long cmdTmp1;
    long cmdTmp2;
    long cmdTmp3;
    long cmdTmp4;

    if (sendConnAck)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
      numCommandToSend++;

      // Channel 31: Connection
      pMyCmd->cmd = connID;
      pMyCmd->param1 = 2; 
      sendConnAck = false;
      sendCommandToMatlab(); 
      if (printAckCommands)
      {
        Serial.println();
        Serial.println("Sending Connection Ack to Matlab: ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      }      
    } 
    if (sendDisconnAck)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
      numCommandToSend++;

      // Channel 31: Disconnection
      pMyCmd->cmd = connID;
      pMyCmd->param1 = 10; 
      sendDisconnAck = false;
      matlab = false;
      if (printAckCommands)
      {
        Serial.println();
        Serial.println("Sending Disonnection Ack to Matlab: ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      }      
      sendCommandToMatlab();      
    }
    else if (sendTenzoState)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);

      numCommandToSend++;
      pMyCmd->cmd = tenzoStateID;

      if (initialized)   
      {   
        pMyCmd->param1 = 1;
        pMyCmd->param3 = 0;     
      } 
      else
      {
        pMyCmd->param1 = 0;       
        pMyCmd->param3 = 1; 
      }

      if (enablePid)      
        pMyCmd->param2 = 1; 
      else
        pMyCmd->param2 = 0;   

      if (printAckCommands)
      {
        Serial.println();
        Serial.println("Sending Tenzo state to Matlab");
        Serial.print(" Take Off: ");
        Serial.println(pMyCmd->param1);
        Serial.print(" Hover:  ");
        Serial.println(pMyCmd->param2);
        Serial.print(" Land: ");
        Serial.print(pMyCmd->param3);
        Serial.println(); 
      } 

      sendCommandToMatlab();  
      sendTenzoState = false;
    }
    else if (storeAccData && accDataReady)
    {
      //////////////////////////// Send values to Matlab
    }
    else if (sendHoverState && sendTakeOffAck)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);

      pMyCmd->cmd = takeOffID;

      if (initialized)      
        pMyCmd->param1 = 1; 
      else
        pMyCmd->param1 = 0;   

      sendTakeOffAck = false;
      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab. TakeOffState:  ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      } 

      pMyCmd++;           // Second Message

      // Channel 18: Hovering
      pMyCmd->cmd = iHoverID;
      if (enablePid)      
        pMyCmd->param1 = 1; 
      else
        pMyCmd->param1 = 0; 
      sendHoverState = false;

      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab: Hover: ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      } 

      sendCommandToMatlab();  
    }
    else if (sendTakeOffAck)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);

      pMyCmd->cmd = takeOffID;

      if (initialized)      
        pMyCmd->param1 = 1; 
      else
        pMyCmd->param1 = 0;   

      sendTakeOffAck = false;
      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab. TakeOffState:  ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      } 
      sendCommandToMatlab();  
    }
    else if (sendHoverState && sendLandAck)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
      // Channel 18: Hovering
      pMyCmd->cmd = iHoverID;
      if (enablePid)      
        pMyCmd->param1 = 1; 
      else
        pMyCmd->param1 = 0; 
      sendHoverState = false;

      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab: Hover: ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      } 

      pMyCmd++;           // Second Message
      // Channel 19: Landing
      pMyCmd->cmd = landID;

      if (initialized)      
        pMyCmd->param1 = 0; 
      else
        pMyCmd->param1 = 1;   
      sendLandAck = false;

      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab. Land state: 2  ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      }      
      // Send two commands to Matlab. numCommandsToSend = 2;
      sendCommandToMatlab();  
    }   
    else if (sendHoverState)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
      // Channel 18: Hovering
      pMyCmd->cmd = 18;
      if (enablePid)      
        pMyCmd->param1 = 1; 
      else
        pMyCmd->param1 = 0; 
      sendHoverState = false;
      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab: Hover: ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      }      
      sendCommandToMatlab();  
    }
    else if (sendLandAck)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
      // Channel 19: Landing
      pMyCmd->cmd = landID;

      if (initialized)      
        pMyCmd->param1 = 0; 
      else
        pMyCmd->param1 = 1;   
      sendLandAck = false;
      sendCommandToMatlab();
      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab. Land state:  ");
        Serial.print(pMyCmd->param1);
        Serial.println(); 
      } 

    }
    else if (sendAccToMatlab && matlab)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
      pMyCmd->cmd = accID;

      pMyCmd->param1 = accX*100;   
      pMyCmd->param2 = accY*100;
      pMyCmd->param3 = accZ*100; 

      numCommandToSend++;

      sendCommandToMatlab();  

      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab. [ax ay az]");
        Serial.println(pMyCmd->param1);
        Serial.println(pMyCmd->param2);
        Serial.println(pMyCmd->param3);
        Serial.println(); 
      } 
      sendAccToMatlab = false;
    }
    else if (sendGyroToMatlab  && matlab)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
      pMyCmd->cmd = gyroID;

      pMyCmd->param1 = Wfilter[0]*100;   
      pMyCmd->param2 = Wfilter[1]*100;
      pMyCmd->param3 = Wfilter[2]*100; 
      numCommandToSend++;

      sendCommandToMatlab();  
      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab. [wx wy wz]");
        Serial.println(pMyCmd->param1);
        Serial.println(pMyCmd->param2);
        Serial.println(pMyCmd->param3);
        Serial.println(); 
      } 
      sendGyroToMatlab = false;
    }
    else if (sendEstToMatlab && sendMagnToMatlab && matlab)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);

      numCommandToSend++;
      pMyCmd->cmd = magnID;

      pMyCmd->param1 = roll1;   
      pMyCmd->param2 = pitch1;
      pMyCmd->param3 = bearing1*100;
      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab. [xM yM zM]");
        Serial.println(pMyCmd->param1);
        Serial.println(pMyCmd->param2);
        Serial.println(pMyCmd->param3);
        Serial.println(); 
      }

      pMyCmd++;           // moves pointer to next command position in message
      numCommandToSend++;
      pMyCmd->cmd = estID;

      pMyCmd->param1 = (int)yAngle;   
      pMyCmd->param2 = (int)xAngle;
      pMyCmd->param3 = (int)zAngle*100;

      sendCommandToMatlab(); 

      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending Ack to Matlab. [xEst yEst zEst]");
        Serial.println(pMyCmd->param1);
        Serial.println(pMyCmd->param2);
        Serial.println(pMyCmd->param3);
        Serial.println(); 
      } 
      sendEstToMatlab = false;
      sendMagnToMatlab = false;
    }
    else if (sendMotorsToMatlab && matlab)
    {
      MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
      pMyCmd->cmd = enableMotorsID;
      numCommandToSend++;

      pMyCmd->param1 = throttle; 
      if (printAckCommands)
      {
        Serial.println();
        Serial.print("Sending throttle to Matlab: ");
        Serial.println(pMyCmd->param1);
        Serial.println(); 
      } 
      sendCommandToMatlab();  
      sendMotorsToMatlab = false;
    }
    else if (sendPidState)
    {
      if (sendAltToMatlab)
      {
        MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
        pMyCmd->cmd = 12;
        numCommandToSend++;

        pMyCmd->param1 = consKpAltitude*1000;   
        pMyCmd->param2 = consKdAltitude*1000;
        pMyCmd->param3 = consKiAltitude*1000;
        pMyCmd->param4 = SetpointAltitude;
        if (printAckCommands)
        {
          Serial.println();
          Serial.print("Sending pid vals to Matlab. Alt cons");
          Serial.println(pMyCmd->param1);
          Serial.println(pMyCmd->param2);
          Serial.println(pMyCmd->param3);
          Serial.println(pMyCmd->param4);
          Serial.println(); 
        } 

        pMyCmd++;           // moves pointer to next command position in message

        pMyCmd->cmd = 16;
        numCommandToSend++;      

        pMyCmd->param1 = aggKpAltitude*1000;   
        pMyCmd->param2 = aggKdAltitude*1000;
        pMyCmd->param3 = aggKiAltitude*1000;

        pMyCmd->param4 = SetpointAltitude;
        if (printAckCommands)
        {
          Serial.println();
          Serial.print("Sending pid vals to Matlab. Alt agg");
          Serial.println(pMyCmd->param1);
          Serial.println(pMyCmd->param2);
          Serial.println(pMyCmd->param3);
          Serial.println(pMyCmd->param4);
          Serial.println(); 
        } 

        sendAltToMatlab = false;
      }
      else if (sendRollToMatlab)
      {
        MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
        pMyCmd->cmd = 9;
        numCommandToSend++;

        pMyCmd->param1 = consKpRoll*1000;   
        pMyCmd->param2 = consKdRoll*1000;
        pMyCmd->param3 = consKiRoll*1000;
        pMyCmd->param4 = SetpointRoll;
        if (printAckCommands)
        {
          Serial.println();
          Serial.print("Sending pid vals to Matlab. roll cons: ");
          Serial.println(pMyCmd->param1);
          Serial.println(pMyCmd->param2);
          Serial.println(pMyCmd->param3);
          Serial.println(pMyCmd->param4);
          Serial.println(); 
        } 

        pMyCmd++;           // moves pointer to next command position in message

        pMyCmd->cmd = 13;
        numCommandToSend++;

        pMyCmd->param1 = aggKpRoll*1000;   
        pMyCmd->param2 = aggKdRoll*1000;
        pMyCmd->param3 = aggKiRoll*1000;
        pMyCmd->param4 = SetpointRoll;
        if (printAckCommands)
        {
          Serial.println();
          Serial.print("Sending pid vals to Matlab. roll agg");
          Serial.println(pMyCmd->param1);
          Serial.println(pMyCmd->param2);
          Serial.println(pMyCmd->param3);
          Serial.println(pMyCmd->param4);
          Serial.println(); 
        }

        sendRollToMatlab = false;
      }
      else if (sendPitchToMatlab)
      {
        MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
        pMyCmd->cmd = 10;
        numCommandToSend++;

        pMyCmd->param1 = consKpPitch*1000;   
        pMyCmd->param2 = consKdPitch*1000;
        pMyCmd->param3 = consKiPitch*1000;
        pMyCmd->param4 = SetpointPitch;
        if (printAckCommands)
        {
          Serial.println();
          Serial.print("Sending pid vals to Matlab. pitch cons");
          Serial.println(pMyCmd->param1);
          Serial.println(pMyCmd->param2);
          Serial.println(pMyCmd->param3);
          Serial.println(pMyCmd->param4);
          Serial.println(); 
        } 

        pMyCmd++;           // moves pointer to next command position in message

        pMyCmd->cmd = 14;
        numCommandToSend++;

        pMyCmd->param1 = aggKpPitch*1000;   
        pMyCmd->param2 = aggKdPitch*1000;
        pMyCmd->param3 = aggKiPitch*1000;
        pMyCmd->param4 = SetpointPitch;
        if (printAckCommands)
        {
          Serial.println();
          Serial.print("Sending pid vals to Matlab. pitch agg");
          Serial.println(pMyCmd->param1);
          Serial.println(pMyCmd->param2);
          Serial.println(pMyCmd->param3);
          Serial.println(pMyCmd->param4);
          Serial.println(); 
        } 

        sendPitchToMatlab = false;
      }
      else if (sendYawToMatlab)
      {
        MyCommand * pMyCmd = (MyCommand *)(&buffer[sizeof(MyControlHdr)]);
        pMyCmd->cmd = 11;
        numCommandToSend++;

        pMyCmd->param1 = consKpYaw*1000;   
        pMyCmd->param2 = consKiYaw*1000;
        pMyCmd->param3 = consKiYaw*1000;
        pMyCmd->param4 = SetpointYaw;
        if (printAckCommands)
        {
          Serial.println();
          Serial.print("Sending pid vals to Matlab. Yaw cons");
          Serial.println(pMyCmd->param1);
          Serial.println(pMyCmd->param2);
          Serial.println(pMyCmd->param3);
          Serial.println(pMyCmd->param4);
          Serial.println(); 
        } 

        pMyCmd++;           // moves pointer to next command position in message

        pMyCmd->cmd = 15;
        numCommandToSend++;

        pMyCmd->param1 = aggKpYaw*1000;   
        pMyCmd->param2 = aggKdYaw*1000;
        pMyCmd->param3 = aggKiYaw*1000;
        pMyCmd->param4 = SetpointYaw;
        if (printAckCommands)
        {
          Serial.println();
          Serial.print("Sending pid vals to Matlab. yaw agg");
          Serial.println(pMyCmd->param1);
          Serial.println(pMyCmd->param2);
          Serial.println(pMyCmd->param3);
          Serial.println(pMyCmd->param4);
          Serial.println(); 
        } 

        sendYawToMatlab = false;
      }
      sendPidState = false;
      sendCommandToMatlab(); 
    }  
  } 
}

void sendCommandToMatlabFilter()
{
  // Build Header
  pCtrlHdrAcc->srcAddr = 1;
  pCtrlHdrAcc->dstAddr = 2;     
  pCtrlHdrAcc->versionX = versionArduinoProtocol;    // possible way to let a receiver know a code version
  pCtrlHdrAcc->numCmds = numFilterValuesToSend;    // how many commands will be in the message
  if (printVerboseSerial)
  {
    Serial.println();
    Serial.print("Sending # commands:");
    Serial.print(numFilterValuesToSend);
    Serial.println();
  }
  pCtrlHdrAcc->hdrLength = sizeof(MyControlHdr );  // tells receiver where commands start
  pCtrlHdrAcc->cmdLength = sizeof(MyShortCommand );     // tells receiver size of each command 
  // include total length of entire message
  pCtrlHdrAcc->totalLen= sizeof(MyControlHdr ) + (sizeof(MyShortCommand) * pCtrlHdrAcc->numCmds);
  if (printVerboseSerial)
  {
    Serial.println();
    Serial.print("Total length: ");
    Serial.print(pCtrlHdrAcc->totalLen);
    Serial.println();
    Serial.print("Command length: ");
    Serial.print(pCtrlHdrAcc->cmdLength);
    Serial.println();
    Serial.print("Header length: ");
    Serial.print(pCtrlHdrAcc->hdrLength);
    Serial.println();
  }
  pCtrlHdrAcc->crc = 21;  // dummy temp value
  
  for (int v=0;v<=sizeof(bufferAcc);v++)
  {
    
    //Serial.print(v);
    //Serial.print(" Val: ");
    //Serial.print(bufferAcc[v]);
    //Serial.print(" (byte) | ");    
    Serial.write(bufferAcc[v]);  
    //Serial.println(" | ");
  }
  //Serial.println();
  numFilterValuesToSend = 0;
  
  //storeAccData = false;
}


void sendCommandToMatlab()
{
  // Build Header
  pCtrlHdr->srcAddr = 1;
  pCtrlHdr->dstAddr = 2;    // maybe you'll make 2555 a broadcast address? 
  pCtrlHdr->versionX = versionArduinoProtocol;    // possible way to let a receiver know a code version
  pCtrlHdr->numCmds = numCommandToSend;    // how many commands will be in the message
  if (printVerboseSerial)
  {
    Serial.println();
    Serial.print("Sending # commands:");
    Serial.print(numCommandToSend);
    Serial.println();
  }
  pCtrlHdr->hdrLength = sizeof(MyControlHdr );  // tells receiver where commands start
  pCtrlHdr->cmdLength = sizeof(MyCommand );     // tells receiver size of each command 
  // include total length of entire message
  pCtrlHdr->totalLen= sizeof(MyControlHdr ) + (sizeof(MyCommand) * pCtrlHdr->numCmds);
  if (printVerboseSerial)
  {
    Serial.println();
    Serial.print("Total length: ");
    Serial.print(pCtrlHdr->totalLen);
    Serial.println();
  }
  pCtrlHdr->crc = 21;   // dummy temp value

  for (int v=0;v<=sizeof(buffer);v++)
  {
    xbee.write(buffer[v]);  
  }
  numCommandToSend = 0;
}


void testMotor(int x)
{   
  //Serial.print(" Testing motor: ");
  Serial.println(x);
  if (x==1)
  {
    Serial.println('M1');
    //Serial.println(" Increasing angular velocity");
    servo1.write(40);
    delay(500);
    servo1.write(60);
    delay(500);
    servo1.write(100);
    //Serial.println(" Decreasing angular velocity");
    delay(500);
    servo1.write(40);
    delay(500);
    servo1.write(0);
  }
  else if (x==2)
  {
    Serial.println('M2');
    //Serial.println(" Increasing angular velocity");
    servo2.write(40);
    delay(500);
    servo2.write(60);
    delay(500);
    servo2.write(100);
    //Serial.println(" Decreasing angular velocity");
    delay(500);
    servo2.write(40);
    delay(500);
    servo2.write(0);
  }
  else if (x==3)
  {
    Serial.println('M3');
    //Serial.println(" Increasing angular velocity");
    servo3.write(40);
    delay(500);
    servo3.write(60);
    delay(500);
    servo3.write(100);
    //Serial.println(" Decreasing angular velocity");
    delay(500);
    servo3.write(40);
    delay(500);
    servo3.write(0);
  }
  else if (x==4)
  {
    Serial.println('M4');
    //Serial.println(" Increasing angular velocity");
    servo4.write(0);
    delay(500);
    servo4.write(20);
    delay(500);
    servo4.write(40);
    delay(500);
    servo4.write(60);
    delay(500);
    servo4.write(80);
    delay(500);
    servo4.write(100);
    delay(500);
    servo4.write(120);
    //Serial.println(" Decreasing angular velocity");
    delay(100);
    servo4.write(60);
    delay(500);
    servo4.write(40);
    delay(500);
    servo4.write(0);
  }
}

void getAccValues()
{
  int accValX = analogRead(accPinX);
  int accValY = analogRead(accPinY);
  int accValZ = analogRead(accPinZ);

  accX = (((accValX*5000.0)/1023.0)-XError)/800;
  accY = (((accValY*5000.0)/1023.0)-YError)/800;
  accZ = (((accValZ*5000.0)/1023.0)-ZError)/800;

  // gets the value sample time
  accTimer = millis() - lastAccTimer;
  // updates last reading timer
  lastAccTimer = millis(); 

  accfilter[0] = accX;
  accfilter[1] = accY;
  accfilter[2] = accZ;

  accFilterExpMA(accfilter);

  // gets the time between each filtered sample
  filteredAccTimer = millis() - lastFiltAccTimer;
  // updates last reading timer
  lastFiltAccTimer = millis(); 
  
  
  // Fills the buffers to send
  if (contBuffAcc%(sizeBuffAcc+1) == 0)
  {
    sendBuffAcc();
    accDataReady=true;
  }
  else if (storeAccData)
  {
    //Serial.println(" Storing  ...");
    /*
    Serial.print(" Filling Buff  ");
     Serial.print(accX);
     Serial.print("   ");
     Serial.print(accY);    
     Serial.print("   ");
     Serial.print(accZ);
     Serial.print("   ");
     Serial.print(lastAccTimer);
     */
    buffXAccToSend[contBuffAcc-1] = accX;
    /*
    Serial.println("   ");
     for (int i=0;i<10;i++)    
     {  
     Serial.print("val: ");
     Serial.print(i);
     Serial.print("    ");
     Serial.println(buffXAccToSend[i]);
     }
     Serial.println();
     */
    buffYAccToSend[contBuffAcc-1] = accY;
    /*
    Serial.println("   ");
     for (int i=0;i<10;i++)    
     {  
     Serial.print("val: ");
     Serial.print(i);
     Serial.print("    ");
     Serial.println(buffYAccToSend[i]);
     }
     Serial.println();
     */
    buffZAccToSend[contBuffAcc-1] = accZ;
    /*
    Serial.println("   ");
     for (int i=0;i<10;i++)    
     {  
     Serial.print("val: ");
     Serial.print(i);
     Serial.print("    ");
     Serial.println(buffZAccToSend[i]);
     }
     Serial.println();
     */
    buffTimeToSend[contBuffAcc-1] = lastAccTimer;
    /*
    for (int i=0;i<10;i++)    
     {  
     Serial.print("val: ");
     Serial.print(i);
     Serial.print("    ");
     Serial.println(buffTimeToSend[i]);
     }
     Serial.println();
     */
     contBuffAcc++;
  }

  if (printTimers && printAccTimers)
  {
    Serial.println();
    Serial.print(" millis ");
    Serial.print(millis());
    Serial.println();
    Serial.print("    lastAccTimer: ");
    Serial.print(lastAccTimer);
    Serial.print("    AccTimer: ");
    Serial.print(accTimer);
    Serial.print(" ms");
    Serial.println();
    Serial.print("    lastFilteredAccTimer: ");
    Serial.print(lastFiltAccTimer);
    Serial.print("    filterAccTimer: ");
    Serial.print(filteredAccTimer);
    Serial.print(" ms ");
    Serial.println();  
  }

  //Compute angles from acc values
  angleXAcc = (atan2(-accfilter[0],accfilter[2])) * RAD_TO_DEG;
  angleYAcc = (atan2(accfilter[1],accfilter[2])) * RAD_TO_DEG;
  //angleZAcc = (atan2(accfilter[0],accfilter[1])) * RAD_TO_DEG;

  if (printRawAcc)
  {
    Serial.println();
    Serial.print("Acc = [");
    Serial.print(accX);
    Serial.print(",");
    Serial.print(accY);
    Serial.print(",");
    Serial.print(accZ);
    Serial.print("]    ");
    Serial.println();
  }
  if (printAcc)
  {
    Serial.println();
    Serial.print("  [axF,ayF,azF] = [");
    Serial.print(accfilter[0]);
    Serial.print(",");
    Serial.print(accfilter[1]);
    Serial.print(",");
    Serial.print(accfilter[2]);
    Serial.print("]    ");
    Serial.println();
  }  
}  

void sendBuffAcc()
{
  if (printAckCommands)
  {
    Serial.println();
    Serial.print("Size of array:");
    Serial.print(sizeof(buffXAccToSend));
    Serial.println();
  }

  MyShortCommand * pMyCmdShort = (MyShortCommand *)(&bufferAcc[sizeof(MyControlHdr)]);

  for (int i=0;i<sizeBuffAcc;i++)    
  {
    Serial.println(i);
  /*  
    Serial.println(buffXAccToSend[i]);
    Serial.println(buffYAccToSend[i]);
    Serial.println(buffZAccToSend[i]);
    Serial.println(buffTimeToSend[i]);
      
    
    Serial.write((byte)buffXAccToSend[i]);
    Serial.write((byte)buffYAccToSend[i]);
    Serial.write((byte)buffZAccToSend[i]);
    Serial.write((byte)buffTimeToSend[i]);
    */
    numFilterValuesToSend++;    
    
    pMyCmdShort->cmd = accValuesID;  
    pMyCmdShort->param1 = buffXAccToSend[i];   
    pMyCmdShort->param2 = buffYAccToSend[i];
    pMyCmdShort->param3 = buffZAccToSend[i];
    pMyCmdShort->param4 = buffTimeToSend[i];
    
    if (printAckCommands)
    {
      Serial.println("                              NEW");
      //Serial.println("Inserting values into CMD");
      Serial.println(pMyCmdShort->param1);
      Serial.println(pMyCmdShort->param2);
      Serial.println(pMyCmdShort->param3);
      Serial.println(pMyCmdShort->param4);
      Serial.println(); 
    }     
    pMyCmdShort++;
  }

  contBuffAcc=1;    
  //sendCommandToMatlabFilter();
}

void gpsRoutine()
{
  if (! usingInterrupt) 
  {
    // read data from the GPS in the 'main loop'
    char c = GPS.read();
    // if you want to debug, this is a good time to do it!
    //if (GPSECHO)
    //if (c) Serial.print(c);
  }

  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) 
  {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false

    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }

  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  
    timer = millis();

  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000 && GPS.fix) 
  { 
    timer = millis(); // reset the timer

    Serial.print("\nTime: ");
    Serial.print(GPS.hour, DEC); 
    Serial.print(':');
    Serial.print(GPS.minute, DEC); 
    Serial.print(':');
    Serial.print(GPS.seconds, DEC); 
    Serial.print('.');
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC); 
    Serial.print('/');
    Serial.print(GPS.month, DEC); 
    Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: "); 
    Serial.print((int)GPS.fix);
    Serial.print(" quality: ");  
    Serial.println((int)GPS.fixquality); 
    if (GPS.fix) {
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4); 
      Serial.print(GPS.lat);
      Serial.print(", "); 
      xbee.print(" Altitude :");
      xbee.print(GPS.latitude);
      Serial.print(GPS.longitude, 4);
      Serial.println(GPS.lon);

      xbee.print(";  Longitude: ");
      xbee.print(GPS.longitude);
      Serial.print("Speed (knots): "); 
      Serial.println(GPS.speed);
      Serial.print("Angle: "); 
      Serial.println(GPS.angle);
      Serial.print("Altitude: ");
      Serial.println(GPS.altitude);

      xbee.print("; Altitude");
      xbee.print(GPS.altitude);
      Serial.println("Satellites: "); 
      Serial.println((int)GPS.satellites);
    }
  }
}

// Low pass filter
void accFilterExpMA(float val[])
{
  //ArrayList val = new ArrayList();
  xF = (1-alphaA_xy)*xF + alphaA_xy*val[0];
  yF = (1-alphaA_xy)*yF + alphaA_xy*val[1];
  zF = (1-alphaA_z)*zF + alphaA_z*val[2];

  val[0] = xF;
  val[1] = yF;
  val[2] = zF; 
  xF_m1 = xF;
  yF_m1 = yF;
  zF_m1 = zF;

  for (int i=0 ; i<3;i++)
  {
    xFbuff[i]=xFbuff[i+1];
    yFbuff[i]=yFbuff[i+1];
    zFbuff[i]=zFbuff[i+1];    
  }
  xFbuff[2] = val[0];
  yFbuff[2] = val[1];
  zFbuff[2] = val[2];
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
  //wx = ((xMSB << 8) | xLSB);  
  wxCandidate = ((xMSB << 8) | xLSB);  
  if (abs(wxCandidate-wx_past) <= gyrothresholdHigh)
  {
    wx=wxCandidate;
  }
  wx_past  =  wx;
  gyroXAngle += wx * ((double)(micros() - timerMu) / 1000000);
  wxT = wxT + wx;


  byte yMSB = readRegister(gyroAddress, 0x2B);
  byte yLSB = readRegister(gyroAddress, 0x2A);
  wyCandidate = ((yMSB << 8) | yLSB);
  if (abs(wyCandidate-wy_past) <= gyrothresholdHigh)
  {
    wy=wyCandidate;
    wy_past  =  wy;
  }
  wy_past  =  wy;
  gyroYAngle += wy * ((double)(micros() - timerMu) / 1000000);
  wyT = wyT + wy;

  byte zMSB = readRegister(gyroAddress, 0x2D);
  byte zLSB = readRegister(gyroAddress, 0x2C);
  wzCandidate = ((zMSB << 8) | zLSB);
  if (abs(wzCandidate-wz_past) <= gyrothresholdHigh)
  {
    wz=wzCandidate;
  }

  wz_past  =  wz;
  wzT = wzT + wz;

  Wfilter[0] = wx;
  Wfilter[1] = wy;
  Wfilter[2] = wz;

  omegaFilterExpMA(Wfilter);
  if (printRawGyro)
  {
    Serial.println();
    Serial.print(" [wx,wy,wz]");
    Serial.print(" = [ ");
    Serial.print( (float) wx);

    Serial.print(" , ");
    Serial.print( (float) wy);

    Serial.print(" , ");
    Serial.print( (float) wz);
    Serial.print(" ]");
    Serial.println();
  }
  if (printGyro)
  {
    Serial.println();
    Serial.print(" [WxF,WyF,WzF]");
    Serial.print(" = [ ");
    Serial.print( (float) Wfilter[0]);

    Serial.print(" , ");
    Serial.print( (float) Wfilter[1]);

    Serial.print(" , ");
    Serial.print( (float) Wfilter[2]);
    Serial.print(" ]");
    Serial.println();
  }
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

  // compAngleX = ((1-complementaryConstant) * (compAngleX + (wx * (double)(micros() - timerMu) / 1000000))) + (complementaryConstant * angleXAcc); 
  // compAngleY = ((1-complementaryConstant) * (compAngleY + (-wy * (double)(micros() - timerMu) / 1000000))) + (complementaryConstant * angleYAcc); 
  // 
  // Serial.print("  C:( ");
  // Serial.print(compAngleX);
  // Serial.print(" ; ");
  // Serial.print(compAngleY);
  // Serial.print(")  "); 

  xAngle = kalmanX.getAngle(angleXAcc,Wfilter[0],(double)(micros() - timerMu)) + kalmanXOffset;
  yAngle = kalmanY.getAngle(angleYAcc,Wfilter[1],(double)(micros() - timerMu)) + kalmanYOffset;
  yAngle = -yAngle;
  if (filterAng == 1)
  {
    zAngle = kalmanZ.getAngle(angPosFilter[2],Wfilter[2],(double)(micros() - timerMu)) + kalmanZOffset;
  }
  else if (filterAng == 0)
  {
    zAngle = kalmanZ.getAngle(bearing1,Wfilter[2],(double)(micros() - timerMu)) + kalmanZOffset;
  }

  angK[0] = xAngle;
  angK[1] = yAngle;
  angK[2] = zAngle;
  angEstKalmanFilter(angK);
  if (printRawKalman)
  {
    Serial.println();
    Serial.print("K:( ");
    Serial.print((int)yAngle);
    Serial.print(" ; ");
    Serial.print((int)xAngle);
    Serial.print(" ; ");
    Serial.print((int)zAngle);
    Serial.print(")     ");
    Serial.println();
  }
  if (printKalman)
  {
    Serial.println();
    Serial.print("KFilt:( ");
    Serial.print((int)  angK[1]);
    Serial.print(" ; ");
    Serial.print((int) angK[0]);
    Serial.print(" ; ");
    Serial.print((int) angK[2]);
    Serial.print(")");
    Serial.println();
  }

  pitchK = angK[0];
  rollK = angK[1];
  yaw = angK[2];

  timerMu = micros();
}

void getBmpValues()
{
  /* Get a new sensor event */
  sensors_event_t event;
  bmp.getEvent(&event);
  /* Display the results (barometric pressure is measure in hPa) */
  if (event.pressure)
  {
    Serial.println("Station data: ");
    /* Display atmospheric pressue in hPa */
    Serial.print("Pressure: ");
    Serial.print(event.pressure);
    Serial.println(" hPa");
    /* Calculating altitude with reasonable accuracy requires pressure *
     * sea level pressure for your position at the moment the data is *
     * converted, as well as the ambient temperature in degress *
     * celcius. If you don't have these values, a 'generic' value of *
     * 1013.25 hPa can be used (defined as SENSORS_PRESSURE_SEALEVELHPA *
     * in sensors.h), but this isn't ideal and will give variable *
     * results from one day to the next. *
     * *
     * You can usually find the current SLP value by looking at weather *
     * websites or from environmental information centers near any major *
     * airport. *
     * *
     * For example, for Paris, France you can check the current mean *
     * pressure and sea level at: http://bit.ly/16Au8ol */
    /* First we get the current temperature from the BMP085 */
    float temperature;
    bmp.getTemperature(&temperature);
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    /* Then convert the atmospheric pressure, SLP and temp to altitude */
    /* Update this next line with the current SLP for better results */
    //float seaLevelPressure = 1008.6;
    //float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;
    Serial.print("Altitude: ");
    Serial.print(bmp.pressureToAltitude(seaLevelPressure,event.pressure,temperature));
    Serial.print(" m");
    Serial.println("");
  }
  else
  {
    Serial.println("Sensor error");
  }
}

void angEstKalmanFilter(float val[])
{
  //ArrayList val = new ArrayList();
  AKxF = (1-alphaAK)*AKxF + alphaAK*val[0];
  AKyF = (1-alphaAK)*AKyF + alphaAK*val[1];
  //AKzF = (1-alphaAK)*AKzF + alphaAK*val[2];

  val[0] = AKxF;
  val[1] = AKyF;
  //val[2] = AKzF; 
}

void angPosFilterExpMA(float val[])
{
  APxF = (1-alphaAP)*APxF + alphaAP*val[0];
  APyF = (1-alphaAP)*APyF + alphaAP*val[1];
  APzF = (1-alphaAP)*APzF + alphaAP*val[2];

  val[0] = APxF;
  val[1] = APyF;
  val[2] = APzF; 
}

void displayValues(int b, int p, int r)
{    
  if (printRawCompass)
  {
    Serial.println();
    Serial.print(" Compass: [R, P, Y] = [");
    Serial.print(r);  
    Serial.print(" , ");
    Serial.print(p);
    Serial.print(" , ");
    Serial.print(b);
    Serial.print(" ]");
    Serial.println();
  }
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
  }
  else if(scale == 500){
    writeRegister(gyroAddress, CTRL_REG4, 0b00010000);
  }
  else{
    writeRegister(gyroAddress, CTRL_REG4, 0b00110000);
  }

  // CTRL_REG5 controls high-pass filtering of outputs, use it
  // if you'd like:
  //writeRegister(gyroAddress, CTRL_REG5, 0b00010000);
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
  } 
  else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

/**
 * FreeIMU implementation
 */

void getValues(float val[])
{
  if (filterIMUData)
  {
    val[0] = Wfilter[0];
    val[1] = Wfilter[1];
    val[2] = Wfilter[2];
    val[3] = accfilter[0];
    val[4] = accfilter[1];
    val[5] = accfilter[2];
    val[6] = angPosFilter[0];
    val[7] = angPosFilter[1];
    val[8] = angPosFilter[2];
  } 
  else
  {
    val[0] = wx;
    val[1] = wy;
    val[2] = wz;
    val[3] = accX;
    val[4] = accY;
    val[5] = accZ;
    val[6] = roll1;
    val[7] = pitch1;
    val[8] = bearing1;
  }
}

/**
 * Populates array q with a quaternion representing the IMU orientation with respect to the Earth
 * 
 * @param q the quaternion to populate
 */
void getQ(float * q) {
  float val[9];
  getValues(val);

  now = micros();
  sampleFreq = 1.0 / ((now - lastUpdate) / 1000000.0);

  lastUpdate = now;
  // gyro values are expressed in deg/sec, the * M_PI/180 will convert it to radians/sec

  //AHRSupdate(val[3] * M_PI/180, val[4] * M_PI/180, val[5] * M_PI/180, val[0], val[1], val[2], val[6], val[7], val[8]);
  AHRSupdate(val[0] * M_PI/180, val[1] * M_PI/180, val[2] * M_PI/180, val[3], val[4], val[5], val[6], val[7], val[8]);

  //  AHRSupdate(val[3] * M_PI/180, val[4] * M_PI/180, val[5] * M_PI/180, val[0], val[1], val[2], -val[6], -val[7], val[8]);
  Serial.println();
  Serial.print(q0);
  Serial.print("  ");
  Serial.print(q1);
  Serial.print("  ");
  Serial.print(q2);
  Serial.print("  ");
  Serial.print(q3);
  Serial.println();
  q[0] = q0;
  q[1] = q1;
  q[2] = q2;
  q[3] = q3;
}

/**
 * Returns the Euler angles in degrees defined with the Aerospace sequence.
 * See Sebastian O.H. Madwick report "An efficient orientation filter for 
 * inertial and intertial/magnetic sensor arrays" Chapter 2 Quaternion representation
 * 
 * @param angles three floats array which will be populated by the Euler angles in degrees
 */
void getEuler(float * angles) {
  getEulerRad(angles);
  arr3_rad_to_deg(angles);
}

/**
 * Returns the yaw pitch and roll angles, respectively defined as the angles in degrees between
 * the Earth North and the IMU X axis (yaw), the Earth ground plane and the IMU X axis (pitch)
 * and the Earth ground plane and the IMU Y axis.
 * 
 * @note This is not an Euler representation: the rotations aren't consecutive rotations but only
 * angles from Earth and the IMU. For Euler representation Yaw, Pitch and Roll see FreeIMU::getEuler
 * 
 * @param ypr three floats array which will be populated by Yaw, Pitch and Roll angles in degrees
 */
void getYawPitchRoll(float * ypr) {
  getYawPitchRollRad(ypr);
  arr3_rad_to_deg(ypr);
}

/**
 * Returns the Euler angles in radians defined in the Aerospace sequence.
 * See Sebastian O.H. Madwick report "An efficient orientation filter for 
 * inertial and intertial/magnetic sensor arrays" Chapter 2 Quaternion representation
 * 
 * @param angles three floats array which will be populated by the Euler angles in radians
 */
void getEulerRad(float * angles) {
  float q[4]; // quaternion
  getQ(q);
  angles[0] = atan2(2 * q[1] * q[2] - 2 * q[0] * q[3], 2 * q[0]*q[0] + 2 * q[1] * q[1] - 1); // psi
  angles[1] = -asin(2 * q[1] * q[3] + 2 * q[0] * q[2]); // theta
  angles[2] = atan2(2 * q[2] * q[3] - 2 * q[0] * q[1], 2 * q[0] * q[0] + 2 * q[3] * q[3] - 1); // phi
}

/**
 * Returns the yaw pitch and roll angles, respectively defined as the angles in radians between
 * the Earth North and the IMU X axis (yaw), the Earth ground plane and the IMU X axis (pitch)
 * and the Earth ground plane and the IMU Y axis.
 * 
 * @note This is not an Euler representation: the rotations aren't consecutive rotations but only
 * angles from Earth and the IMU. For Euler representation Yaw, Pitch and Roll see FreeIMU::getEuler
 * 
 * @param ypr three floats array which will be populated by Yaw, Pitch and Roll angles in radians
 */
void getYawPitchRollRad(float * ypr) {
  float q[4]; // quaternion
  float gx, gy, gz; // estimated gravity direction
  getQ(q);

  gx = 2 * (q[1]*q[3] - q[0]*q[2]);
  gy = 2 * (q[0]*q[1] + q[2]*q[3]);
  gz = q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3];

  ypr[0] = atan2(2 * q[1] * q[2] - 2 * q[0] * q[3], 2 * q[0]*q[0] + 2 * q[1] * q[1] - 1);
  ypr[1] = atan(gx / sqrt(gy*gy + gz*gz));
  ypr[2] = atan(gy / sqrt(gx*gx + gz*gz));
}

/**
 * Fast inverse square root implementation
 * @see http://en.wikipedia.org/wiki/Fast_inverse_square_root
 */
float invSqrt(float number) {
  volatile long i;
  volatile float x, y;
  volatile const float f = 1.5F;

  x = number * 0.5F;
  y = number;
  i = * ( long * ) &y;
  i = 0x5f375a86 - ( i >> 1 );
  y = * ( float * ) &i;
  y = y * ( f - ( x * y * y ) );
  return y;
}

/**
 * Quaternion implementation of the 'DCM filter' [Mayhony et al].  Incorporates the magnetic distortion
 * compensation algorithms from Sebastian Madgwick's filter which eliminates the need for a reference
 * direction of flux (bx bz) to be predefined and limits the effect of magnetic distortions to yaw
 * axis only.
 * 
 * @see: http://www.x-io.co.uk/node/8#open_source_ahrs_and_imu_algorithms
 */
void  AHRSupdate(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz) 
{
  float recipNorm;
  float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;
  float halfex = 0.0f, halfey = 0.0f, halfez = 0.0f;
  float qa, qb, qc;

  // Auxiliary variables to avoid repeated arithmetic
  q0q0 = q0 * q0;
  q0q1 = q0 * q1;
  q0q2 = q0 * q2;
  q0q3 = q0 * q3;
  q1q1 = q1 * q1;
  q1q2 = q1 * q2;
  q1q3 = q1 * q3;
  q2q2 = q2 * q2;
  q2q3 = q2 * q3;
  q3q3 = q3 * q3;

  // Use magnetometer measurement only when valid (avoids NaN in magnetometer normalisation)
  if((mx != 0.0f) && (my != 0.0f) && (mz != 0.0f)) {
    float hx, hy, bx, bz;
    float halfwx, halfwy, halfwz;

    // Normalise magnetometer measurement
    recipNorm = invSqrt(mx * mx + my * my + mz * mz);

    mx *= recipNorm;
    my *= recipNorm;
    mz *= recipNorm;

    // Reference direction of Earth's magnetic field
    hx = 2.0f * (mx * (0.5f - q2q2 - q3q3) + my * (q1q2 - q0q3) + mz * (q1q3 + q0q2));
    hy = 2.0f * (mx * (q1q2 + q0q3) + my * (0.5f - q1q1 - q3q3) + mz * (q2q3 - q0q1));
    bx = sqrt(hx * hx + hy * hy);
    bz = 2.0f * (mx * (q1q3 - q0q2) + my * (q2q3 + q0q1) + mz * (0.5f - q1q1 - q2q2));


    // Estimated direction of magnetic field
    halfwx = bx * (0.5f - q2q2 - q3q3) + bz * (q1q3 - q0q2);
    halfwy = bx * (q1q2 - q0q3) + bz * (q0q1 + q2q3);
    halfwz = bx * (q0q2 + q1q3) + bz * (0.5f - q1q1 - q2q2);


    // Error is sum of cross product between estimated direction and measured direction of field vectors
    halfex = (my * halfwz - mz * halfwy);
    halfey = (mz * halfwx - mx * halfwz);
    halfez = (mx * halfwy - my * halfwx);
  }

  // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
  if((ax != 0.0f) && (ay != 0.0f) && (az != 0.0f)) {
    float halfvx, halfvy, halfvz;

    // Normalise accelerometer measurement
    recipNorm = invSqrt(ax * ax + ay * ay + az * az);
    ax *= recipNorm;
    ay *= recipNorm;
    az *= recipNorm;

    // Estimated direction of gravity
    halfvx = q1q3 - q0q2;
    halfvy = q0q1 + q2q3;
    halfvz = q0q0 - 0.5f + q3q3;

    // Error is sum of cross product between estimated direction and measured direction of field vectors
    halfex += (ay * halfvz - az * halfvy);
    halfey += (az * halfvx - ax * halfvz);
    halfez += (ax * halfvy - ay * halfvx);
  }

  // Apply feedback only when valid data has been gathered from the accelerometer or magnetometer
  if(halfex != 0.0f && halfey != 0.0f && halfez != 0.0f) {
    // Compute and apply integral feedback if enabled
    if(twoKi > 0.0f) {
      integralFBx += twoKi * halfex * (1.0f / sampleFreq);  // integral error scaled by Ki
      integralFBy += twoKi * halfey * (1.0f / sampleFreq);
      integralFBz += twoKi * halfez * (1.0f / sampleFreq);
      gx += integralFBx;  // apply integral feedback
      gy += integralFBy;
      gz += integralFBz;
    }
    else {
      integralFBx = 0.0f; // prevent integral windup
      integralFBy = 0.0f;
      integralFBz = 0.0f;
    }

    // Apply proportional feedback
    gx += twoKp * halfex;
    gy += twoKp * halfey;
    gz += twoKp * halfez;

  }

  // Integrate rate of change of quaternion
  gx *= (0.5f * (1.0f / sampleFreq));   // pre-multiply common factors
  gy *= (0.5f * (1.0f / sampleFreq));
  gz *= (0.5f * (1.0f / sampleFreq));
  qa = q0;
  qb = q1;
  qc = q2;
  q0 += (-qb * gx - qc * gy - q3 * gz);
  q1 += (qa * gx + qc * gz - q3 * gy);
  q2 += (qa * gy - qb * gz + q3 * gx);
  q3 += (qa * gz + qb * gy - qc * gx);


  Serial.println();
  Serial.println();
  Serial.print(qa);
  Serial.print("  ");
  Serial.print(qb);
  Serial.print("   ");
  Serial.print(qc);
  Serial.print("  ");
  Serial.print(q0);
  Serial.println();
  Serial.println(); 

  // Normalise quaternion
  recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
  q0 *= recipNorm;
  q1 *= recipNorm;
  q2 *= recipNorm;
  q3 *= recipNorm;
}

/**
 * Converts a 3 elements array arr of angles expressed in radians into degrees
 */
void arr3_rad_to_deg(float * arr) {
  arr[0] *= 180/M_PI;
  arr[1] *= 180/M_PI;
  arr[2] *= 180/M_PI;
}

void dispAllPidVals()
{
  Serial.println();
  Serial.print(" Updating PID values");
  Serial.println();
  if (enableRollPid)
  {
    Serial.println(" Roll:  ");
    Serial.print("   Cons [p,d,i]: [");
    Serial.print(consKpRoll);
    Serial.print(" ; ");
    Serial.print(consKdRoll);
    Serial.print(" ; ");
    Serial.print(consKiRoll);
    Serial.print(" ]   Agg: [p,d,i]: [");
    Serial.print(aggKpRoll);
    Serial.print(" ; ");
    Serial.print(aggKdRoll);
    Serial.print(" ; ");
    Serial.print(aggKiRoll);
    Serial.print(" ]   Setpoint: ");
    Serial.println(SetpointRoll);
  }
  if (enablePitchPid)
  {
    Serial.println(" Pitch:  ");
    Serial.print("   Cons [p,d,i]: [");
    Serial.print(consKpPitch);
    Serial.print(" ; ");
    Serial.print(consKdPitch);
    Serial.print(" ; ");
    Serial.print(consKiPitch);
    Serial.print(" ]   Agg: [p,d,i]: [");
    Serial.print(aggKpPitch);
    Serial.print(" ; ");
    Serial.print(aggKdPitch);
    Serial.print(" ; ");
    Serial.print(aggKiPitch);
    Serial.print(" ]   Setpoint: ");
    Serial.println(SetpointPitch);
  }
  if (enableYawPid)
  {
    Serial.println(" Yaw:  ");
    Serial.print("   Cons [p,d,i]: [");
    Serial.print(consKpYaw);
    Serial.print(" ; ");
    Serial.print(consKdYaw);
    Serial.print(" ; ");
    Serial.print(consKiYaw);
    Serial.print(" ]   Agg: [p,d,i]: [");
    Serial.print(aggKpYaw);
    Serial.print(" ; ");
    Serial.print(aggKdYaw);
    Serial.print(" ; ");
    Serial.print(aggKiYaw);
    Serial.print(" ]   Setpoint: ");
    Serial.println(SetpointYaw);
  }
  if (enableAltitudePid)
  {
    Serial.println(" Alt:  ");
    Serial.print("   Cons [p,d,i]: [");
    Serial.print(consKpAltitude);
    Serial.print(" ; ");
    Serial.print(consKdAltitude);
    Serial.print(" ; ");
    Serial.print(consKiAltitude);
    Serial.print(" ]   Agg: [p,d,i]: [");
    Serial.print(aggKpAltitude);
    Serial.print(" ; ");
    Serial.print(aggKdAltitude);
    Serial.print(" ; ");
    Serial.print(aggKiAltitude);
    Serial.print(" ]   Setpoint: ");
    Serial.println(SetpointAltitude);
  }
  Serial.println();
}

void dispActualAllPidVals()
{
  Serial.println();
  Serial.print(" Updating PID values");
  Serial.println();
  if (enableRollPid)
  {
    Serial.println(" Roll:  ");
    Serial.print("   [p,d,i]: [");
    Serial.print(myRollPID.GetKp());
    Serial.print(" ; ");
    Serial.print(myRollPID.GetKd());
    Serial.print(" ; ");
    Serial.print(myRollPID.GetKi());
    Serial.print(" ]   Setpoint: ");
    Serial.println(SetpointRoll);
  }
  if (enablePitchPid)
  {
    Serial.println(" Pitch:  ");
    Serial.print("   [p,d,i]: [");
    Serial.print(myPitchPID.GetKp());
    Serial.print(" ; ");
    Serial.print(myPitchPID.GetKd());
    Serial.print(" ; ");
    Serial.print(myPitchPID.GetKi());
    Serial.print(" ]   Setpoint: ");
    Serial.println(SetpointPitch);
  }
  if (enableYawPid)
  {
    Serial.println(" Yaw:  ");
    Serial.print("  [p,d,i]: [");
    Serial.print(myYawPID.GetKp());
    Serial.print(" ; ");
    Serial.print(myPitchPID.GetKd());
    Serial.print(" ; ");
    Serial.print(myPitchPID.GetKi());
    Serial.print(" ]   Setpoint: ");
    Serial.println(SetpointYaw);
  }
  if (enableAltitudePid)
  {
    Serial.println(" Alt:  ");
    Serial.print("   [p,d,i]: [");
    Serial.print(myAltitudePID.GetKp());
    Serial.print(" ; ");
    Serial.print(myAltitudePID.GetKd());
    Serial.print(" ; ");
    Serial.print(myAltitudePID.GetKi());
    Serial.print(" ]   Setpoint: ");
    Serial.println(SetpointAltitude);
  }
  Serial.println();
}

