#include <Adafruit_INA219.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

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
static const unsigned char PROGMEM image_wifi_bits[] = {0x01,0xf0,0x00,0x06,0x0c,0x00,0x18,0x03,0x00,0x21,0xf0,0x80,0x46,0x0c,0x40,0x88,0x02,0x20,0x10,0xe1,0x00,0x23,0x18,0x80,0x04,0x04,0x00,0x08,0x42,0x00,0x01,0xb0,0x00,0x02,0x08,0x00,0x00,0x40,0x00,0x00,0xa0,0x00,0x00,0x40,0x00,0x00,0x00,0x00};

// Replace the next variables with your SSID/Password combination
const char* ssid = "Fersadi88";
const char* password = "Evan808080";
bool is_wifi_connected = false;
const unsigned long reconnect_wifi_delay_period = 5000;

// Add your MQTT Broker IP address, example:
//const char* mqtt_server = "192.168.1.144";
const char* broker_ip = "192.168.0.10";
const int broker_port = 3008;
bool is_mqtt_connected = false;

//scheduler vars
unsigned long startMillis;  //some global variables available anywhere in the program

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
  //backward, forward, stop
  //backward: pin1: high, pin2: low
  //forward: pin1: low, pin2: high
  //stop: pin1: low, pin2: low
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

void display_main_menu() {
  display.clearDisplay();
  display.fillScreen(0x0);
  // battery_charging
  display.drawBitmap(94, 9, image_battery_charging_bits, 24, 16, WHITE);
  // wifi
  display.drawBitmap(53, 9, image_wifi_bits, 19, 16, WHITE);
  // music_radio_broadcast
  display.drawBitmap(16, 9, image_music_radio_broadcast_bits, 15, 16, WHITE);
  display.display();
}

void display_option_battery() {
//i think oled 0.91 can only be white tho, this is should be constant ("WHITE" value)
  display.clearDisplay();
  display.fillScreen(0x0);
  // battery_charging
  display.drawBitmap(94, 9, image_battery_charging_bits, 24, 16, WHITE);
  // wifi
  display.drawBitmap(53, 9, image_wifi_bits, 19, 16, WHITE);
  // Layer 4
  display.drawCircle(106, 16, 14, WHITE);
  // music_radio_broadcast
  display.drawBitmap(16, 9, image_music_radio_broadcast_bits, 15, 16, WHITE);
  display.display();
}

void display_option_wifi() {
  display.clearDisplay();
  display.fillScreen(0x0);
  // battery_charging
  display.drawBitmap(94, 9, image_battery_charging_bits, 24, 16, WHITE);
  // wifi
  display.drawBitmap(53, 9, image_wifi_bits, 19, 16, WHITE);
  // Layer 4
  display.drawCircle(62, 15, 14, WHITE);
  // music_radio_broadcast
  display.drawBitmap(16, 9, image_music_radio_broadcast_bits, 15, 16, WHITE);
  display.display();
}

void display_option_mqtt() {
  display.clearDisplay();
  display.fillScreen(0x0);
  // battery_charging
  display.drawBitmap(94, 9, image_battery_charging_bits, 24, 16, WHITE);
  // wifi
  display.drawBitmap(53, 9, image_wifi_bits, 19, 16, WHITE);
  // Layer 4
  display.drawCircle(23, 16, 14, WHITE);
  // music_radio_broadcast
  display.drawBitmap(16, 9, image_music_radio_broadcast_bits, 15, 16, WHITE);
  display.display();
}

//the battery_cap type is hardcoded (mAh)
void display_battery(float voltage, float current, int battery_cap, float percentage) {
  display.clearDisplay();
  display.fillScreen(0x0);
  display.drawBitmap(5, 1, image_battery_charging_bits, 48, 32, WHITE);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setCursor(22, 13);
  display.print(int(percentage)); display.println("%"); 
  display.setCursor(59, 5);
  display.print(int(voltage)); display.println("V");
  display.setCursor(75, 5);
  display.print(int(current)); display.println("A");
  display.setCursor(59, 18);
  display.print(battery_cap); display.println("mAh");
  display.drawBitmap(117, 3, image_arrow_right_bits, 7, 5, WHITE);
  display.display();
}

void display_mqtt(const char* broker_ip, bool status) {
    display.clearDisplay();
  display.fillScreen(0x0);
  // Layer 2
  display.setTextColor(WHITE);
  display.setTextWrap(false);
  display.setCursor(34, 8);
  display.print("IP:");
  // Layer 3
  display.setCursor(34, 18);
  display.print("Stat:");
  // Layer 2 copy 1
  display.setCursor(53, 8);
  display.print(broker_ip);
  // Layer 2 copy 2
  display.setCursor(66, 18);
  display.print(status);
  // arrow_right
  display.drawBitmap(115, 4, image_arrow_right_bits, 7, 5, WHITE);
  // music_radio_broadcast
  display.drawBitmap(12, 9, image_music_radio_broadcast_bits, 15, 16, WHITE);
  display.display();
}

void display_wifi(const char* ssid, const char* pass) {
  display.clearDisplay();
  display.fillScreen(0x0);
  // wifi
  display.drawBitmap(9, 8, image_wifi_bits, 19, 16, WHITE);
  //Layer 2
  display.setTextColor(WHITE);
  display.setTextWrap(false);
  display.setCursor(34, 8);
  display.print("SSID:");
  //Layer 3
  display.setCursor(34, 18);
  display.print("PW:");
  //Layer 2 copy 1
  display.setCursor(64, 8);
  display.print(ssid);
  //Layer 2 copy 2
  display.setCursor(64, 18);
  display.print(pass);
  //arrow_right
  display.drawBitmap(115, 4, image_arrow_right_bits, 7, 5, WHITE);
  display.display();
}

//APPLICATIONS
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
          display_option_mqtt();
        break;
      case 2:
        // statements
          display_option_wifi();
        break;
            case 3:
        // statements
          display_option_battery();
        break;
    }
}

//function below have the same return in case 0, 
//so you can use either two of them if necessary
void pick_display(int index) {
   switch (index) {
      case 0:
         display_main_menu();
        break;
      case 1:
        display_mqtt(broker_ip, is_mqtt_connected);
        break;
      case 2:
        display_wifi(ssid, password);
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
          Serial.println("The upper button is released when is_on_display = false");
          //reset if incremented beyond total state, so it looped the options
          if (current_state > total_state) {
            current_state = 0;
          }
        }

        //mean if user clicked view/lower button
        if (lower_pressed) {
          Serial.println("The lower button is released when is_on_display = false");
          is_on_display = true;
        }
      } else {
        pick_display(current_state);

        if (lower_pressed) {
          Serial.println("The lower button is released when is_on_display = true");
          is_on_display = false;
          current_state = home_state;
        }
      }

      upper_last_steady = upper_button_current;
      lower_last_steady = lower_button_current;
    }
}

void connect_to_broker(char* *topics) {
  if (!is_mqtt_connected && is_wifi_connected) {
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

//scheduler
bool is_time_to_wifi_reconnect() { //reconnect every 500 milsec if still unconnected
   //millis() get the current "time" (actually the number of milliseconds since the program started)
  if (millis() - startMillis >= reconnect_wifi_delay_period && !is_wifi_connected)  //test whether the period has elapsed
  {
    return true;
  } else {
    return false;
  }
}

//note that wifi needs a delay time every Wifi.begin execution
void connect_to_wifi(const char* ssid, const char* password) {
  // We start by connecting to a WiFi network
  WiFi.begin(ssid, password);

  if (WiFi.status() == WL_CONNECTED) {
    is_wifi_connected = true;
  } else {
    is_wifi_connected = false;
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
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

void setup_mqtt_client(const char* broker_ip, const int broker_port) {
  client.setServer(broker_ip, broker_port);
  client.setCallback(callback);
}

void setup() {
  // sets the pins as outputs:
  init_drv8833();
  init_buzzer();
  init_ssd1306();
  init_menu_buttons();
  Wire.begin();
  ina219.begin();
  setup_mqtt_client(broker_ip, broker_port);
  Serial.begin(9600);
  startMillis = millis();
}

void loop() {
  application_menu();   
 // connect_to_broker(topics);
     if (is_time_to_wifi_reconnect()) {
    connect_to_wifi(ssid, password);
  }
  //client.loop();
}