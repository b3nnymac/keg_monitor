
// Include libraries
#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>


byte sensorInterrupt = 0;
byte flowflowSensorPin = 7;
int temperature_sensor = 2; //temperature sensor Signal pin on digital 2

OneWire ds(temperature_sensor);  // on digital pin 2

// The hall-effect flow sensor outputs approximately 4.5 pulses per second per litre/minute of flow.
float calibrationFactor = 4.5;

volatile byte pulseCount;

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
float temperature ;

unsigned long oldTime;

// Enter a MAC address for your controller below.
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xFE, 0x40 };

// ThingSpeak Settings
char thingSpeakAddress[] = "api.thingspeak.com";
String thingtweetAPIKey = "xxxxxxxx"; //fill in thinkgtweet key here

// Tweets Variables Setup
long lastConnectionTime = 0;
boolean lastConnected = false;
int failedCounter = 0;
int tappedMessageCounter = 0;


// Set the static IP address for your board
IPAddress ip(xxx, xx, xx, xxx);

// IP address of your computer
IPAddress server(xxx, xxx, xxx, xxx);

// Initialize the Ethernet client
EthernetClient client;

void setup()
{

  // Initialize a serial connection for reporting values to the host
  Serial.begin(115200);

  // Start the Ethernet connection
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, ip);
  }

  // Display IP
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());

  // Give the Ethernet shield a second to initialize
  delay(1000);
  Serial.println("Connecting...");

  pinMode(flowSensorPin, INPUT);
  digitalWrite(flowSensorPin, HIGH);

  pulseCount        = 0;
  flowRate          = 0.0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
  oldTime           = 0;

  // The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
  // Configured to trigger on a FALLING state change (transition from HIGH
  // state to LOW state)
  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
}

/**
   Main program loop
*/
void loop()
{

  temperature = getTemp();

  if ((millis() - oldTime) > 1000)   // Only process counters once per second
  {

    logData();

    // Disable the interrupt while calculating flow rate and sending the value to the host
    detachInterrupt(sensorInterrupt);

    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;

    // Note the time this processing pass was executed. Note that because we've
    // disabled interrupts the millis() function won't actually be incrementing right
    // at this point, but it will still return the value it was set to just before
    // interrupts went away.
    oldTime = millis();

    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 1000;

    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;

    unsigned int frac;

    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable
    Serial.print(".");             // Print the decimal point
    // Determine the fractional part. The 10 multiplier gives us 1 decimal place.
    frac = (flowRate - int(flowRate)) * 10;
    Serial.print(frac, DEC) ;      // Print the fractional part of the variable
    Serial.print("L/min");
    // Print the number of litres flowed in this second
    Serial.print("  Current Liquid Flowing: ");             // Output separator
    Serial.print(flowMilliLitres);
    Serial.print("mL/Sec");

    // Print the cumulative total of litres flowed since starting
    Serial.print("  Output Liquid Quantity: ");             // Output separator
    Serial.print(totalMilliLitres);
    Serial.println("mL");

    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;

    // Enable the interrupt again now that we've finished sending output
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
    checkIfTweet();
  }
}

float getTemp() {
  //returns the temperature from one tempperature sensor in Celsius

  byte data[12];
  byte addr[8];

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end

  byte present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read Scratchpad


  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
  }

  ds.reset_search();

  byte MSB = data[1];
  byte LSB = data[0];

  float tempRead = ((MSB << 8) | LSB); //using two's compliment
  float TemperatureSum = tempRead / 16;

  return TemperatureSum;

}

/*
  Insterrupt Service Routine
*/
void pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;

}

void checkIfTweet() {
  int  currentTime = millis();
  Serial.println("Checking Tweets: " + currentTime);

  if (tappedMessageCounter == 0)
  {
    tappedMessageCounter++;

    // Update Twitter via ThingTweet
    updateTwitterStatus("Keg is tapped!");
  }
  else if (totalMilliLitres > 5000 && tappedMessageCounter == 1  )
  {
    tappedMessageCounter++;

    updateTwitterStatus("Beer is flowing, 3/4 of the keg is left!");

  }

  else if (totalMilliLitres > 10000 && tappedMessageCounter == 2  )
  {
    tappedMessageCounter++;

    updateTwitterStatus("Better Hurry, 1/2 of the keg is left!");

  }
  else if (totalMilliLitres > 15000 && tappedMessageCounter == 3  )
  {
    tappedMessageCounter++;
    updateTwitterStatus("Better Hurry, 1/4 of the keg is left!");

  }
  else if (totalMilliLitres > 20000 && tappedMessageCounter == 4)
  {
    tappedMessageCounter++;
    updateTwitterStatus("Better Hurry, 1/4 of the keg is left!");

  }
  else
  {
    // do Thing C
  }
}

void updateTwitterStatus(String tsData)
{
  if (client.connect(thingSpeakAddress, 80))
  {
    Serial.println("Tweeting Message: " + tappedMessageCounter);
    // Create HTTP POST Data
    tsData = "api_key=" + thingtweetAPIKey + "&status=" + tsData;

    client.print("POST /apps/thingtweet/1/statuses/update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(tsData.length());
    client.print("\n\n");

    client.print(tsData);

    lastConnectionTime = millis();

    if (client.connected())
    {
      Serial.println("Connecting to ThingSpeak...");
      Serial.println();

      failedCounter = 0;
    }
    else
    {
      failedCounter++;

      Serial.println("Connection to ThingSpeak failed (" + String(failedCounter, DEC) + ")");
      Serial.println();
    }

  }
  else
  {
    failedCounter++;

    Serial.println("Connection to ThingSpeak Failed (" + String(failedCounter, DEC) + ")");
    Serial.println();

    lastConnectionTime = millis();
  }
}

void logData() {
  Serial.println("Logging Data");

  if (client.connect(server, 80)) {
    if (client.connected()) {

      // Make a HTTP request:
      Serial.println("Logging Data");

      client.print("GET /datalogger/datalogger.php?flowrate=");
      client.print(flowRate);
      client.print("&flowmilliLitres=");
      client.print(flowMilliLitres);
      client.print("&totalmilliLitres=");
      client.println(totalMilliLitres);
      client.print("&temperature=");
      client.println(temperature);
      client.println("Host: 10.0.1.5");
      client.println("Connection: close");
      client.println();

    }
    else {
      // If you didn't get a connection to the server
      Serial.println("connection failed");
    }

    // Read the answer
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        Serial.print(c);
      }
    }

    // If the server's disconnected, stop the client:
    if (!client.connected()) {
      Serial.println();
      Serial.println("disconnecting.");
      client.stop();
    }
  }

}
