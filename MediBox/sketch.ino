// include libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

// OLED display parameters
#define SCREEN_WIDTH 128 //OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3c ///< See datashee for Address; 03xD for 128x64, 0x3C for 128x32

// Pin definitions
#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12

#define NTP_SERVER     "pool.ntp.org" // NTP server address
#define UTC_OFFSET_DST 0

// Initialize time zone offset variables
int UTC_OFFSET = 0;
int UTC_OFFSET_MIN = 0;
int UTC_OFFSET_HOUR = 0;

// Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Global variables for time and alarms
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

bool alarm_enabled = true;
int n_alarms = 3;
int alarm_hours[] = {0,1,0};
int alarm_minutes[] = {0,10,0};
bool alarm_triggered[] = {false, false, false};

// Define musical notes
int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

// Define modes for the menu
int current_mode = 0;
int max_modes = 5;
String modes[] = {"1 - Set Time Zone", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Set Alarm 3", "5 - Disable Alarm"};

void setup() {
  // Initialize pins
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  
  dhtSensor.setup(DHTPIN, DHTesp::DHT22); // Setup DHT sensor

  Serial.begin(9600); // Initialize serial communication
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Dont't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializies this with an Adafruit splash screen.
  display.display();
  delay(500); // Pause for 0.5 seconds

  // Connect to Wi-Fi network
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI", 0, 0, 2);
  }
  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 2);

  // Configure time synchronization with NTP server using the calculated offset
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  display.clearDisplay();   //Clear the buffer
  print_line("Welcome to Medibox!", 10, 20, 2); // Display welcome message
  delay(500);
  display.clearDisplay();
}

void loop() {
  update_time_with_check_alarm(); // Update time and check for alarms
  check_temp(); // Check temperature and humidity
  delay(100);
  // Check for menu button press
  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    Serial.println("Menu");
    go_to_menu();
  }
}

// Function to print a line of text in a given text size and the given position
void print_line(String text, int column, int row, int text_size) {
  //display.clearDisplay();
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);

  display.display(); 
  //delay(2000);
}

// Function to update and print the current time
void print_time_now(void) {
  display.clearDisplay();
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);
}

// Function to update time
void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  // Add time zone offset
  timeinfo.tm_hour = (timeinfo.tm_hour + UTC_OFFSET_HOUR) % 24; // Adjust hour by time zone offset
  timeinfo.tm_min += UTC_OFFSET_MIN; // Adjust minutes by time zone offset
  if (timeinfo.tm_min >= 60) { // If minutes exceed 60, adjust hour and minutes
    timeinfo.tm_hour += 1;
    timeinfo.tm_min -= 60;
  } else if (timeinfo.tm_min < 0) { // If minutes are negative, adjust hour and minutes
    timeinfo.tm_hour -= 1;
    timeinfo.tm_min += 60;
  }
  
  // Extract and assign hour, minute, second, and day from adjusted timeinfo
  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);
}

// Function to automatically update the current time whike checking for alarms
void update_time_with_check_alarm(void) {
  update_time();
  print_time_now();

  if (alarm_enabled == true) {
    for (int i = 0; i < n_alarms ; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes){
        ring_alarm(); // call the ringing function
        alarm_triggered[i] = true;
      }
    }
  }
}

// Function to ring an alarm
void ring_alarm() {
  // Show message on display
  display.clearDisplay();
  print_line("MEDICINE TIME!", 2, 0, 0);

  digitalWrite(LED_1, HIGH); // Turn on LED

  bool break_happened = false;

  // Ring the buzzer
  while (break_happened == false && digitalRead(PB_CANCEL) == HIGH) {
    for (int i=0; i<n_notes; i++){
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        break_happened = true;
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}
// Function to wait for a button press and return the pressed button
int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

// Function to navigate through the menu
void go_to_menu() {
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2); 
      
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;

    }
    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode<0) {
        current_mode = max_modes - 1;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      run_mode(current_mode);
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

// Function to set an alarm
void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);
    
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour<0) {
        temp_hour = 23;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_minute = alarm_minutes[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);
    
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute<0) {
        temp_minute = 59;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
  display.clearDisplay();
  alarm_enabled = true;// Set alarm_enabled flag to true, indicating that alarms are enabled
  print_line("Alarm is set", 0, 0, 2);
}

// Function to set time zone
void set_time_zone() {
  int temp_offset_hour = UTC_OFFSET_HOUR; // Initialize temp_offset_hour with the current UTC offset hour
  // Loop to set the offset hour
  while (true) {
    display.clearDisplay();
    // Display prompt for entering offset hours 
    print_line("Enter offset hours: " + String(temp_offset_hour), 0, 0, 2); 
    int pressed = wait_for_button_press();  // Wait for button press
    // UTC offsets range from UTC−12:00 to UTC+14:00
    if (pressed == PB_UP) {
      delay(200);
      temp_offset_hour += 1; // Increment offset hour
      if (temp_offset_hour> 14) { // If offset hour exceeds 14
        temp_offset_hour = -12; // Set it to -12 (wrapping around to the start)
      }
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_offset_hour -= 1; // Decrement offset hour
      if (temp_offset_hour < -12) { // If offset hour is less than -12
        temp_offset_hour = 14; // Set it to 14 (wrapping around to the end)
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      UTC_OFFSET_HOUR = temp_offset_hour; // Update UTC_OFFSET_HOUR
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_offset_min = UTC_OFFSET_MIN; // Initialize temp_offset_min with the current UTC offset minute
  // Loop to set the offset minute
  while (true) {
    display.clearDisplay();
     // Display prompt for entering offset minutes
    print_line("Enter offset minutes: " + String(temp_offset_min), 0, 0, 2);
    int pressed = wait_for_button_press(); // Wait for button press
    // Few zones are offset by an additional 30 or 45 minutes
    if (pressed == PB_UP) {
      delay(200);
      temp_offset_min += 15; // Increment offset minute by 15 minutes
      if (temp_offset_min> 59) { // If offset minute exceeds 59
        temp_offset_min = 0; // Set it to 0 (wrapping around to the start)
      }
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_offset_min -= 15; // Decrement offset minute by 15 minutes
      if (temp_offset_min < 0) { // If offset minute is less than 0
        temp_offset_min = 45; // Set it to 45 (wrapping around to the end)
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      UTC_OFFSET_MIN = temp_offset_min; // Update UTC_OFFSET_MIN
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;  // Exit loop
    }
  }
  display.clearDisplay();
  print_line("Time zone set", 0, 0, 2); // Display confirmation message
  delay(1000);
}

// Function to disable all alarms
void disable_alarms() {
  alarm_enabled = false;// Set alarm_enabled flag to false, indicating that alarms are disabled
  // Loop through all alarms
  for (int i = 0; i < n_alarms; i++) {
    alarm_triggered[i] = false; // Set each alarm_triggered flag to false, indicating that none of the alarms are triggered
  }
  display.clearDisplay();
  print_line(" Disabled all alarms.", 0, 0, 2); // Print a message indicating that all alarms are disabled
  delay(1000);
}

// Function to run different modes based on the selected mode
void run_mode(int mode) {
  // If mode is 0, call the function to set the time zone
  if (mode == 0) {
    set_time_zone();
  }
  else if (mode >= 1 && mode <= 3) {
    // If mode is between 1 and 3 (inclusive), call the function to set the alarm corresponding to the mode index
    set_alarm(mode - 1);
  }
  else if (mode == 4) {
    disable_alarms();  // If mode is 4, call the function to disable all alarms
  } 
}

// Function to check temperature and humidity
void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity(); // Get temperature and humidity data from sensor
  
  // Temperature and humidity Warnings from buzzer and LED
  if (data.temperature >= 32 || data.temperature <= 26 || data.humidity >= 80 || data.humidity <= 60) {
    digitalWrite(LED_1, HIGH);
    tone(BUZZER, notes[0]);
    delay(300);
    noTone(BUZZER);
    digitalWrite(LED_1, LOW);
    display.clearDisplay();
  }
  // Display messages based on temperature and humidity levels
  if (data.temperature >= 32) {
    print_line("TEMP HIGH", 0, 40, 1);
  }
  else if (data.temperature <= 26) {
    print_line("TEMP LOW", 0, 40, 1);
  }

  if (data.humidity >= 80) {
    print_line("Humidtiy HIGH", 0, 50, 1);
  }
  else if (data.humidity <= 60) {
    print_line("TEMP LOW", 0, 50, 1);
  }
}