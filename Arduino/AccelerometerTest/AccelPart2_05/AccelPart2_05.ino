//#define DEBUGMODE

#include <Servo.h>

const float RESOLUTION=800; //0.8 v/g -> resolucion de 1.5g -> 800mV/g
const float VOLTAGE=3.3;  //voltage al que está conectado el acelerómetro

const float ZOUT_1G = 850;   // mv Voltage en Zout a 1G

const int NADJ  = 50;        // Número de lecturas para calcular el error

// Entradas analógicas donde van los sensores
const int xaxis = 0;
const int yaxis = 1;
const int zaxis = 2;

float XError,YError,ZError;
float xd,yd,zd,z,x,y;
float aax,aay,aaz;


Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;


// Acc Timers
unsigned long accTimer;
unsigned long lastAccTimer;
unsigned long timeToRead = 0;
unsigned long lastTimeToRead = 0;

int contSamples = 0;
int samplesNum = 500;

byte mode;
unsigned int sensorValue = 0;
boolean connectEd = false;


int rampTill = 85;
int motorRampDelayFast = 150;
int throttle = 0;

float AccelAdjust(int axis)
{
 float acc = 0;
 for (int j=0;j<NADJ;j++)
 {
   float lectura=analogRead(axis);
   acc = acc + ((lectura*5000)/1023.0);
   delay(11); //número primo para evitar ciclos de lectura proporcionales
 }
 return acc/NADJ;
}

typedef struct ctrTag { // 17 bytes
  unsigned char cmd;
  long param1;
  long param2;
  long param3;
  long param4;   
} 
MyCommand;


char buffer[20];


// angle estimated from kalman
double xAngle;
double yAngle;
double zAngle;

//Kalman
Kalman kalmanX;
Kalman kalmanY;
Kalman kalmanZ;

//Offset Kalman
int kalmanXOffset = -0;
int kalmanYOffset = 0;
int kalmanZOffset = 0;

void setup()
{  
 Serial.begin(9600); // 9600 bps
 pinMode(xaxis,INPUT);
 pinMode(yaxis,INPUT);
 pinMode(zaxis,INPUT);

 XError =  AccelAdjust(xaxis);
 YError =  AccelAdjust(yaxis);
 ZError =  AccelAdjust(zaxis);
 ZError = ZError - ZOUT_1G;
 
 
  servo1.attach(3);  
  servo2.attach(5);    
  servo3.attach(6);   
  servo4.attach(9);

  servo1.write(0);  
  servo2.write(0); 
  servo3.write(0); 
  servo4.write(0);
  
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
}

void loop()
{
 accRoutine(); 
 serialRoutine();
 motorSpeed(throttle);
}

void estimateAngle()
{
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
  
  pitchK = angK[0];
  rollK = angK[1];
  yaw = angK[2];

  timerMu = micros();
}

void serialRoutine()
{
   if (Serial.available()>1)
   {
     mode = Serial.read();
     if (mode == 82)
     {
       lastTimeToRead = millis();
       Serial.print("T");
       Serial.print(",");
       Serial.println("A");
       delay(100);
       while (contSamples <= samplesNum)
       {
         timeToRead = millis() - lastTimeToRead;
         x=analogRead(xaxis);
         y=analogRead(yaxis);
         z=analogRead(zaxis);
        
         aax = (((x*5000.0)/1023.0)-XError)/RESOLUTION;
         aay = (((y*5000.0)/1023.0)-YError)/RESOLUTION;
         aaz = (((z*5000.0)/1023.0)-ZError)/RESOLUTION;
        
         // gets the value sample time
         accTimer = millis() - lastAccTimer;
         // updates last reading timer
         lastAccTimer = millis(); 
         
         Serial.print("S");
         Serial.print(",");
         Serial.print(aax);
         Serial.print(",");
         Serial.print(aay);
         Serial.print(",");
         Serial.print(aaz);
         Serial.print(",");
         Serial.println(lastAccTimer);
         //Plot terminator carriage
         //Serial.write(13);
         contSamples++;
         motorSpeed(throttle);
       }       
       Serial.print("T");
       Serial.print(",");
       Serial.println("B");   
       land();  
       contSamples = 0;  
     } 
     else if (mode == 84)
     {
       Serial.print("R");
       Serial.print(",");
       Serial.print(aax);
       Serial.print(",");
       Serial.print(aay);
       Serial.print(",");
       Serial.print(aaz);
       Serial.print(",");
       Serial.println(lastAccTimer);
     }
    if(mode == 16)
    {  	
      Serial.write(17);
    }
    if (mode == 18)
    {
      connectEd = true;
      Serial.write(19);      
      initialize();
    }
   }
}

void initialize()
{
   for (int j=0; j<rampTill;j++)
   {
      motorSpeed(j);
      //Serial.println(j);
      delay(motorRampDelayFast); 
   }
   throttle=rampTill;
}

void motorSpeed(int x)
{    
  servo1.write(x);      
  servo2.write(x); 
  servo3.write(x); 
  servo4.write(x); 
}

void land()
{
  for (int j=throttle; j>40 ;j--)
  {
    motorSpeed(j);
    //Serial.println(j);
    // Kind or brutal land
    delay(motorRampDelayFast);
  }  
  throttle=0;
}

void accRoutine()
{
  
   x=analogRead(xaxis);
   y=analogRead(yaxis);
   z=analogRead(zaxis);
  
   aax = (((x*5000.0)/1023.0)-XError)/RESOLUTION;
   aay = (((y*5000.0)/1023.0)-YError)/RESOLUTION;
   aaz = (((z*5000.0)/1023.0)-ZError)/RESOLUTION;
  
   // gets the value sample time
   accTimer = millis() - lastAccTimer;
   // updates last reading timer
   lastAccTimer = millis(); 
  
  #ifdef DEBUGMODE
   Serial.print(aax);
   Serial.print(" ");
   Serial.print(aay);
   Serial.print(" ");          
   Serial.print(aaz);
   Serial.println();
  #endif
}
