#include <Adafruit_INA219.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "BluetoothSerial.h"
#include "Org_01.h"

static const unsigned char PROGMEM image_Layer_12_bits[] = {0x80,0x00,0x00,0x80};

const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data

boolean newData = false;

int dataNumber = 0;             // new for this version

//Timer
unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;

String device_name = "ESP32-BT-Slave";

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;

/*
BLUETOOTH COMMAND CODES
Its important to change these command value below as this is just an example!
*/
//WIFI
const int start_wifi_reconn = 212912812;
const int stop_wifi_reconn = 201829232;

//Oled & ina219 config
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin 
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_INA219 ina219;

//debounce
#define DEBOUNCE_TIME  50 // the debounce time in millisecond, increase this time if it still chatters
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
static const unsigned char PROGMEM image_arrow_right_bits[] = {0x08,0x04,0xfe,0x04,0x08};
static const unsigned char PROGMEM image_battery_charging_bits[] = {0x00,0x02,0x00,0x0f,0xe4,0xfe,0x10,0x0c,0x01,0x10,0x08,0x01,0x70,0x18,0x01,0x80,0x30,0x01,0x80,0x3f,0x81,0x80,0x7f,0x01,0x80,0x03,0x01,0x80,0x06,0x01,0x70,0x04,0x01,0x10,0x0c,0x01,0x10,0x08,0x01,0x0f,0xd3,0xfe,0x00,0x10,0x00,0x00,0x00,0x00};
static const unsigned char PROGMEM image_music_radio_broadcast_bits[] = {0x07,0xc0,0x18,0x30,0x27,0xc8,0x48,0x24,0x93,0x92,0xa4,0x4a,0xa9,0x2a,0xa3,0x8a,0x06,0xc0,0x03,0x80,0x01,0x00,0x03,0x80,0x02,0x80,0x06,0xc0,0x04,0x40,0x00,0x00};
static const unsigned char PROGMEM image_bluetooth_bits[] = {0x01,0x00,0x02,0x80,0x02,0x40,0x22,0x20,0x12,0x20,0x0a,0x40,0x06,0x80,0x03,0x00,0x06,0x80,0x0a,0x40,0x12,0x20,0x22,0x20,0x02,0x40,0x02,0x80,0x01,0x00,0x00,0x00};

// Replace the next variables with your SSID/Password combination
const char* ssid = "Fersadi88";
const char* password = "Evan808080";
bool is_wifi_connected = false;
const unsigned long connecting_timeout = 5000;

// Add your MQTT Broker IP address, example:
//const char* mqtt_server = "192.168.1.144";
const char* broker_ip = "192.168.0.10";
const int broker_port = 3008;
bool is_mqtt_connected = false;

//wifi
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0; //for publish message every 5 sec
char msg[50];
int value = 0;

//topics
char* topics[] = {"esp32/drive"};

//Additional
int battery_cap = 2200 ;//mAh

//note to change the motor and etc pins to snake_case
// Front group motor
int motor1Pin1 = 34; 
int motor1Pin2 = 35; 
int sleepFrontPin = 19; 
int motor2Pin1 = 32; 
int motor2Pin2 = 33; 

//Back group motor
int motor3Pin1 = 25; 
int motor3Pin2 = 26; 
int sleepBackPin = 18; 
int motor4Pin1 = 27; 
int motor4Pin2 = 14; 

//buzzer
int buzzerPin = 4;

//buttons
int upper_button = 2;
int lower_button = 15;

//menu buttons state 
int upper_last_steady = LOW; // the previous steady state from the input pin
int upper_button_flickerable = LOW; // the previous flickerable state from the input pin
int upper_button_current;   // the current reading from the input pin
int lower_last_steady = LOW;
int lower_button_flickerable = LOW;
int lower_button_current;

//menu state
int total_state = 3; //there's 3, but im using index
int home_state = 0;
int current_state = 0;
bool is_on_display = false; //is the menu chosen or viewed

//INITS
void init_drv8833() {
  // Front group motor
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);
  pinMode(sleepFrontPin, OUTPUT);
  //Back group motor
  pinMode(motor3Pin1, OUTPUT);
  pinMode(motor3Pin2, OUTPUT);
  pinMode(motor4Pin1, OUTPUT);
  pinMode(motor4Pin2, OUTPUT);
  pinMode(sleepBackPin, OUTPUT);
}

void init_mqtt_client(const char* broker_ip, const int broker_port) {
  client.setServer(broker_ip, broker_port);
  client.setCallback(callback);
}

void init_menu_buttons() {
  pinMode(upper_button, INPUT_PULLUP); // upper button
  pinMode(lower_button, INPUT_PULLUP); //bottom button
}

void init_ssd1306(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  //display.display();
  //delay(1000); // Pause for 2 seconds

  // Clear the buffer
  delay(2000);
  display.clearDisplay();
}

void init_buzzer(){
  pinMode(buzzerPin, OUTPUT);
}

//DRIVERS
void driver_drv8833_basic (char pin1, char pin2) {
  /*
  backward, forward, stop
  backward: pin1: high, pin2: low
  forward: pin1: low, pin2: high
  stop: pin1: low, pin2: low
  */
  digitalWrite(motor1Pin1, pin1);
  digitalWrite(motor1Pin2, pin2); 
  digitalWrite(motor2Pin1, pin1);
  digitalWrite(motor2Pin2, pin2); 
  digitalWrite(motor3Pin1, pin1);
  digitalWrite(motor3Pin2, pin2); 
  digitalWrite(motor4Pin1, pin1);
  digitalWrite(motor4Pin2, pin2); 
}

void driver_buzzer_buzz() {
    digitalWrite(buzzerPin, HIGH);
}

void driver_drv8833_mode(int mode) {
  //mode = 1, active
  //mode = 0, sleep
  digitalWrite(sleepFrontPin, mode);
  digitalWrite(sleepBackPin, mode);
}


/****DISPLAY FUNCTIONS****/
void display_main_menu() {
  display.clearDisplay();
  display.fillScreen(0x0);
  display.drawBitmap(94, 9, image_battery_charging_bits, 24, 16, WHITE);
  display.drawBitmap(53, 9, image_bluetooth_bits, 19, 16, WHITE);
  display.drawBitmap(16, 9, image_music_radio_broadcast_bits, 15, 16, WHITE);
  display.display();
}

void display_option_battery() {
//i think oled 0.91 can only be white tho, this is should be constant ("WHITE" value)
  display.clearDisplay();
  display.fillScreen(0x0);
  display.drawBitmap(94, 9, image_battery_charging_bits, 24, 16, WHITE);
  display.drawBitmap(53, 9, image_bluetooth_bits, 19, 16, WHITE);
  display.drawCircle(106, 16, 14, WHITE);
  display.drawBitmap(16, 9, image_music_radio_broadcast_bits, 15, 16, WHITE);
  display.display();
}

void display_listen_bt() {
    display.clearDisplay();
    display.fillScreen(0x0);

    // Layer 2
    display.setTextColor(WHITE);
    display.setTextWrap(false);
    display.setFont(&Org_01);
    display.setCursor(40, 18);
    display.print("Listening...");
    display.display();
}

void display_option_bluetooth() {
  display.clearDisplay();
  display.fillScreen(0x0);
  display.drawBitmap(94, 9, image_battery_charging_bits, 24, 16, WHITE);
  display.drawBitmap(53, 9, image_bluetooth_bits, 19, 16, WHITE);
  display.drawCircle(62, 15, 14, WHITE);
  display.drawBitmap(16, 9, image_music_radio_broadcast_bits, 15, 16, WHITE);
  display.display();
}

void display_option_broker_wifi() {
  display.clearDisplay();
  display.fillScreen(0x0);
  display.drawBitmap(94, 9, image_battery_charging_bits, 24, 16, WHITE);
  display.drawBitmap(53, 9, image_bluetooth_bits, 19, 16, WHITE);
  display.drawCircle(23, 16, 14, WHITE);
  display.drawBitmap(16, 9, image_music_radio_broadcast_bits, 15, 16, WHITE);
  display.display();
}

void display_battery(float voltage, float current, int battery_cap, float percentage) { 
     display.clearDisplay();
  //the battery_cap type is hardcoded (mAh)
    display.fillScreen(0x0);

    // Layer 1
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setTextWrap(false);
    display.setFont(&Org_01);
    display.setCursor(6, 15);
    display.print(int(percentage) + "%");

    // arrow_right
    display.drawBitmap(119, 2, image_arrow_right_bits, 7, 5, WHITE);

    // Layer 3
    display.setTextSize(1);
    display.setCursor(48, 11);
    display.print(int(voltage) + "V");

    // Layer 4
    display.setCursor(48, 18);
    display.print(int(current) + "A");

    // Layer 3 copy 1
    display.setCursor(6, 24);
    display.print(battery_cap + "mAh");
       display.display();
}

void display_broker_and_wifi(const char* broker_ip, const int broker_port, bool broker_status, const char* ssid, const char* pass, bool wifi_status) {
   display.clearDisplay();
    display.fillScreen(0x0);

    // Layer 2 copy 1
    display.setTextColor(WHITE);
    display.setTextWrap(false);
    display.setFont(&Org_01);
    display.setCursor(6, 11);
    display.print(broker_ip);

    // Layer 2 copy 2
    display.setCursor(57, 11);
    display.print(broker_port);

    // arrow_right
    display.drawBitmap(119, 2, image_arrow_right_bits, 7, 5, WHITE);

    // Layer 2 copy 4
    display.setCursor(6, 25);
    display.print(ssid);

    // Layer 2 copy 3
    display.setCursor(6, 18);
    display.print(pass);

    // Layer 10
    display.drawLine(93, 7, 93, 11, WHITE);

    // Layer 11
    display.setCursor(96, 21);
    display.print(broker_status);

    // Layer 12
    display.drawBitmap(54, 8, image_Layer_12_bits, 1, 4, WHITE);

    // Layer 13
    display.drawLine(93, 14, 93, 25, WHITE);

    // Layer 11 copy 1
    display.setCursor(96, 11);
    display.print(wifi_status);
  display.display();
}

/****HELPER FUNCTIONS****/
void recvWithEndMarker() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
    
    if (SerialBT.available() > 0) {
        rc = SerialBT.read();

        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData = true;
        }
    }
}

void assignNewNumber() {
    if (newData == true) {
        dataNumber = 0;             // new for this version
        dataNumber = atoi(receivedChars);   // new for this version
        /*
        Serial.print("This just in ... ");
        Serial.println(receivedChars);
        Serial.print("Data as Number ... ");    // new for this version
        Serial.println(dataNumber);     // new for this version
        */
        newData = false;
    }
}

float get_battery_soc (float voltage) {
  //use voltage corellation
  int total_soc = 11;
  float voltages[total_soc] = {2.5, 3.0, 3.2, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9, 4.0, 4.2};
  float socs[total_soc] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
  
  for (int i = 0; i <= total_soc; i++) {
    bool is_higher = (voltage >= voltages[i]);
     if (!is_higher) {
       return socs[i];
     }
  }
}

void pick_option_display(int index) {
  switch (index) {
      case 0:
        // statements
         display_main_menu();
        break;
      case 1:
        // statements
          display_option_broker_wifi();
        break;
      case 2:
        // statements
          display_option_bluetooth();
        break;
            case 3:
        // statements
          display_option_battery();
        break;
    }
}

void pick_display(int index) {
    //this function have the same return in case 0, 
   //so you can use either two of them if necessary
   switch (index) {
      case 0:
         display_main_menu();
        break;
      case 1:
      display_broker_and_wifi(broker_ip, broker_port, is_mqtt_connected, ssid, password, is_wifi_connected);
        break;
      case 2:
        display_listen_bt();
        break;
      case 3: 
      {
        //float voltage, float current, int battery_cap, float percentage
        float shunt_voltage = ina219.getShuntVoltage_mV();
        float bus_voltage = ina219.getBusVoltage_V();
        float load_voltage = bus_voltage + (shunt_voltage / 1000);
        int soc = get_battery_soc(load_voltage);
        display_battery(load_voltage, ina219.getCurrent_mA(), battery_cap, soc);
        break;
      }
    }
}

/****APPLICATION/HIGH LEVEL FUNCTIONS****/
void application_menu() {
  bool debounce;
  bool upper_pressed;
  bool lower_pressed;
  upper_button_current = digitalRead(upper_button);
  lower_button_current = digitalRead(lower_button);

  if (upper_button_current != upper_button_flickerable) {
    // reset the debouncing timer
    lastDebounceTime = millis();
    // save the the last flickerable state
    upper_button_flickerable = upper_button_current;
  }

  if (lower_button_current != lower_button_flickerable) {
    // reset the debouncing timer
    lastDebounceTime = millis();
    // save the the last flickerable state
    lower_button_flickerable = lower_button_current;
  }
  // whatever the reading is at, it's been there for longer than the debounce
  // delay, so take it as the actual current state:
  // if the button state has changed:
  /*
  Serial.print("**DEBUG**"); 
  Serial.println("");
  Serial.print("Upper pressed: "); Serial.println(upper_pressed);
  Serial.print("Lower pressed: "); Serial.println(lower_pressed); 
  Serial.print("Debounce: "); Serial.println(debounce);
  Serial.print("Upper_last_steady: "); Serial.println(upper_last_steady);
  Serial.print("Lower_last_steady: "); Serial.println(lower_last_steady); 
  Serial.print("upper_button_current: "); Serial.println(upper_button_current);  
  Serial.print("lower_button_current: "); Serial.println(lower_button_current);  
  Serial.print("upper_button_flickerable: "); Serial.println(upper_button_flickerable); 
  Serial.print("lower_button_flickerable: "); Serial.println(lower_button_flickerable); 
  */

  debounce = ((millis() - lastDebounceTime) > DEBOUNCE_TIME);
  //validate if only upper/lower is pressed
  upper_pressed = (upper_last_steady == HIGH && upper_button_current == LOW);
  lower_pressed = (lower_last_steady == HIGH && lower_button_current == LOW);

  //this logic is for 2 button navigation on oled 0.91 inch
  if (debounce) {
      if (is_on_display == false) {
      pick_option_display(current_state);
        if (upper_pressed) {
          current_state++; 
          //Serial.print(current_state);
          //reset if incremented beyond total state, so it looped the options
          if (current_state > total_state) {
            current_state = 0;
          }
        }

        //mean if user clicked view/lower button
        if (lower_pressed) {
          is_on_display = true;
        }
      } else {
        pick_display(current_state);

        if (lower_pressed) {
          is_on_display = false;
          current_state = home_state;
        }
      }

      upper_last_steady = upper_button_current;
      lower_last_steady = lower_button_current;
    }
}

/****CONNECTION FUNCTIONS****/
void connect_to_broker(char* *topics) {
  if (!is_mqtt_connected && is_wifi_connected) { //remember that we cannot connect to mqtt if is_wifi_connected variable isn't true
    if (client.connect("ESP32WROOM_Client")) {
      is_mqtt_connected = true;
      for (byte i = 0; i < (sizeof(topics) / sizeof(topics[0])); i++) {
      // do something with myValues[i]
      client.subscribe(topics[i]);
      }
    } else {
      is_mqtt_connected = false;
    }
  }
}

void connect_to_wifi(const char* ssid, const char* password, const unsigned long timeout = 0, bool is_listening_bluetooth = false) {
  //NOTE: wifi needs an actual delay time every Wifi.begin execution
  // We start by connecting to a WiFi network
  WiFi.begin(ssid, password);
  is_wifi_connected = true; // set starting value

  if (is_listening_bluetooth == true) {
    SerialBT.begin(device_name);
    Serial.println("");
    Serial.print("Bluetooth is ON");
  }
  
  startMillis = millis();  //initial start time
  while (WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
    Serial.println("");
    if (currentMillis - startMillis >= timeout && is_listening_bluetooth == false)  //test whether the period has elapsed
    {
      is_wifi_connected = false;
      startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
       Serial.print("Wifi attempt timeout");
      break;  
    }

    if (is_listening_bluetooth == true) {
      recvWithEndMarker(); //read and convert serial data
      assignNewNumber(); //update dataNumber variable
      if (dataNumber == stop_wifi_reconn) {
          Serial.print("Wifi attempt aborted");
                is_wifi_connected = false;
          dataNumber = 0; //reset it
          break;
      }
    }
  }

  if (is_listening_bluetooth == true) {
    SerialBT.flush();  
    SerialBT.disconnect();
    SerialBT.end();
        Serial.println("");
    Serial.print("Bluetooth is OFF");
  }
}

void bt_commands_register(int code) {
    switch (code) {
      case start_wifi_reconn: 
      connect_to_wifi(ssid, password, 0, true);
      break;
    }
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print(topic);
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  /*
  if (String(topic) == "esp32/output") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
  */
}


/****PRIMARY FUNCTIONS****/

void setup() {
  Serial.begin(9600);
  init_drv8833(); // sets the pins as outputs:
  init_buzzer();
  init_ssd1306();
  init_menu_buttons();
  Wire.begin();
  ina219.begin();
  Serial.printf("The device with name \"%s\" is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str());
  connect_to_wifi(ssid, password, 0, true);
  init_mqtt_client(broker_ip, broker_port);
}

void loop() {
  //background high level tasks
  application_menu();   
  connect_to_broker(topics);
  client.loop();
}