#include <BleGamepad.h>
#include <BleGamepadConfiguration.h>
#include <NimBLEDescriptor.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <vector>
#include <map>
#include <set>

// Define available GPIO pins for hardware buttons
std::vector<int> availableGPIOPins = {12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23}; // Example GPIO pins

// Pin Definitions
const int DEFAULT_PIN = 12;  // Default Pin

// Create a gamepad configuration
BleGamepadConfiguration config;

// Initialize the gamepad instance with the name 'Sequential Shifter'
BleGamepad gamepad("Gamepad", "Bluetooth", 100);

// Create a web server object
WebServer server(80);

// Button configurations
std::vector<int> buttonPins = {DEFAULT_PIN};  // Vector to store button pins
std::vector<int> buttonOutputs;  // Vector to store button outputs

// Wi-Fi credentials
const char* ssid = "Your SSID";
const char* password = "Your Password";

// Define possible button outputs
const char* buttonOptions[] = {
    "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "Back", "Start", "Left Stick Click", "Right Stick Click", 
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right"
};

// Map button names to their corresponding output numbers
std::map<String, int> buttonMap = {
    {"A", 1},
    {"B", 2},
    {"X", 3},
    {"Y", 4},
    {"LB", 5},
    {"RB", 6},
    {"LT", 7},
    {"RT", 8},
    {"Back", 9},
    {"Start", 10},
    {"Left Stick Click", 11},
    {"Right Stick Click", 12},
    {"D-Pad Up", 13},
    {"D-Pad Down", 14},
    {"D-Pad Left", 15},
    {"D-Pad Right", 16}
};

// Set to track used outputs and GPIO pins
std::set<int> usedOutputs;  // Declare usedOutputs here
std::set<int> usedGPIOPins; // Set to track used GPIO pins

// Function to save button outputs to SPIFFS
void saveButtonOutputs() {
    File file = SPIFFS.open("/button_config.txt", "w"); // Open file in write mode
    if (file) {
        for (size_t i = 0; i < buttonPins.size(); i++) {
            file.println(String(buttonPins[i]) + "," + buttonOutputs[i]); // Save pin and output
        }
        file.close();
        Serial.println("Button outputs saved.");
    } else {
        Serial.println("Failed to open file for writing.");
    }
}

// Function to load button outputs from SPIFFS
void loadButtonOutputs() {
    File file = SPIFFS.open("/button_config.txt", "r");
    if (file) {
        buttonPins.clear(); // Clear the vector before loading new values
        buttonOutputs.clear(); // Clear the outputs as well
        String line;
        while (file.available()) {
            line = file.readStringUntil('\n');
            int pin = line.substring(0, line.indexOf(',')).toInt();
            int output = line.substring(line.indexOf(',') + 1).toInt();
            buttonPins.push_back(pin); // Add the loaded pin
            buttonOutputs.push_back(output); // Add the loaded output
        }
        file.close();
        Serial.println("Button outputs loaded.");
        
        // Populate usedOutputs and usedGPIOPins from loaded outputs
        usedOutputs.clear();
        usedGPIOPins.clear();
        for (const auto& output : buttonOutputs) {
            usedOutputs.insert(output);
        }
        for (const auto& pin : buttonPins) {
            usedGPIOPins.insert(pin);
        }
    } else {
        Serial.println("No previous button config found. Using default.");
    }
}

// Function to clear button configuration
void clearButtonConfig() {
    if (SPIFFS.remove("/button_config.txt")) {
        Serial.println("Button configuration cleared.");
        buttonPins.clear(); // Clear the vector as well
        buttonOutputs.clear(); // Clear outputs
        usedOutputs.clear(); // Clear used outputs
        usedGPIOPins.clear(); // Clear used GPIO pins
    } else {
        Serial.println("Failed to clear button configuration.");
    }
}

// Function to handle the root URL
void handleRoot() {
    // Rebuild the usedOutputs and usedGPIOPins sets to ensure they are up to date
    usedOutputs.clear();
    usedGPIOPins.clear();
    for (const auto& output : buttonOutputs) {
        usedOutputs.insert(output);
    }
    for (const auto& pin : buttonPins) {
        usedGPIOPins.insert(pin);
    }

    String html = "<html><body><h1>Button Configuration</h1>";
    html += "<form action=\"/set\" method=\"POST\">";
    
    // Display existing buttons with GPIO pin dropdown and output options
    for (size_t i = 0; i < buttonPins.size(); i++) {
        html += "GPIO Pin: <select name=\"gpio" + String(i) + "\">";
        for (size_t j = 0; j < availableGPIOPins.size(); j++) {
            // Only display pins that are not already used
            if (usedGPIOPins.find(availableGPIOPins[j]) == usedGPIOPins.end() || buttonPins[i] == availableGPIOPins[j]) {
                html += "<option value=\"" + String(availableGPIOPins[j]) + "\"" + (buttonPins[i] == availableGPIOPins[j] ? " selected" : "") + ">" + String(availableGPIOPins[j]) + "</option>";
            }
        }
        html += "</select> ";
        
        html += "Output: <select name=\"button" + String(i) + "\">";
        for (int j = 0; j < sizeof(buttonOptions) / sizeof(buttonOptions[0]); j++) {
            int currentOutput = buttonMap[buttonOptions[j]];
            // Only display options that have not already been selected, or the currently selected output for this button
            if (usedOutputs.find(currentOutput) == usedOutputs.end() || buttonOutputs[i] == currentOutput) {
                html += "<option value=\"" + String(buttonOptions[j]) + "\"" + (buttonOutputs[i] == currentOutput ? " selected" : "") + ">" + buttonOptions[j] + "</option>";
            }
        }
        html += "</select>";
        html += "<input type=\"submit\" name=\"remove" + String(i) + "\" value=\"Remove\"><br>";
    }

    // Add Button, Save, Load, and Clear buttons
    html += "<input type=\"submit\" name=\"add\" value=\"Add\"><br>"; // Add button
    html += "<input type=\"submit\" name=\"save\" value=\"Save Config\"><br>";
    html += "<input type=\"submit\" name=\"load\" value=\"Load Config\"><br>";
    html += "<input type=\"submit\" name=\"clear\" value=\"Clear Config\"><br>"; // Clear Config button
    
    // Restart Bluetooth button
    html += "<input type=\"submit\" name=\"restartBluetooth\" value=\"Restart Bluetooth\"><br>";

    html += "</form></body></html>";
    
    server.send(200, "text/html", html);
}

// Function to handle setting button outputs
void handleSet() {
    // Clear configuration if requested
    if (server.hasArg("clear")) {
        clearButtonConfig();
        server.sendHeader("Location", "/");
        server.send(303); // Redirect to root
        return;
    }

    // Remove buttons
    for (size_t i = 0; i < buttonPins.size(); i++) {
        if (server.hasArg("remove" + String(i))) {
            // Restore output and GPIO pin when a button is removed
            usedOutputs.erase(buttonOutputs[i]);
            usedGPIOPins.erase(buttonPins[i]); // Free up GPIO pin
            buttonPins.erase(buttonPins.begin() + i);
            buttonOutputs.erase(buttonOutputs.begin() + i);
            saveButtonOutputs();  // Save updated button outputs
            server.sendHeader("Location", "/");
            server.send(303); // Redirect to root
            return; // Exit the function after removal
        }
    }

    // Update button pins and outputs
    for (size_t i = 0; i < buttonOutputs.size(); i++) {
        if (server.hasArg("gpio" + String(i))) {
            int newPin = server.arg("gpio" + String(i)).toInt(); // Update GPIO pin
            // Check if the newPin is already used
            if (usedGPIOPins.find(newPin) == usedGPIOPins.end() || buttonPins[i] == newPin) {
                usedGPIOPins.erase(buttonPins[i]); // Free up old pin
                usedGPIOPins.insert(newPin); // Mark new pin as used
                buttonPins[i] = newPin; // Update to new pin
            }
        }
        if (server.hasArg("button" + String(i))) {
            String outputStr = server.arg("button" + String(i));
            buttonOutputs[i] = buttonMap[outputStr]; // Update output
            usedOutputs.insert(buttonOutputs[i]); // Mark output as used
        }
    }

    // Add new button configuration
    if (server.hasArg("add")) {
        // Check if we can add a new button
        if (buttonPins.size() < 16) { // Limit to 16 buttons
            int newPin = -1;
            int newOutput = -1;
            // Find the next available GPIO pin
            for (size_t i = 0; i < availableGPIOPins.size(); i++) {
                if (usedGPIOPins.find(availableGPIOPins[i]) == usedGPIOPins.end()) {
                    newPin = availableGPIOPins[i]; // Get the first available pin
                    break;
                }
            }

            // Find the next available button output
            for (int i = 0; i < sizeof(buttonOptions) / sizeof(buttonOptions[0]); i++) {
                int outputValue = buttonMap[buttonOptions[i]];
                if (usedOutputs.find(outputValue) == usedOutputs.end()) {
                    newOutput = outputValue; // Get the first available output
                    break;
                }
            }

            if (newPin != -1 && newOutput != -1) { // Ensure both newPin and newOutput are valid
                buttonPins.push_back(newPin);
                buttonOutputs.push_back(newOutput);
                usedOutputs.insert(newOutput); // Mark output as used
                usedGPIOPins.insert(newPin); // Mark GPIO pin as used
            } else {
                Serial.println("No available outputs or GPIO pins left to add a new button.");
            }
        }

        server.sendHeader("Location", "/");
        server.send(303); // Redirect to root
        return;
    }

    // Save button outputs when "Save Config" is clicked
    if (server.hasArg("save")) {
        saveButtonOutputs();
        server.sendHeader("Location", "/");
        server.send(303); // Redirect to root
        return;
    }

    // Load button outputs when "Load Config" is clicked
    if (server.hasArg("load")) {
        loadButtonOutputs();
        server.sendHeader("Location", "/");
        server.send(303); // Redirect to root
        return;
    }

    // Restart Bluetooth if requested
    if (server.hasArg("restartBluetooth")) {
        gamepad.end();
        delay(200);
        gamepad.begin(&config);
        Serial.println("Bluetooth restarted successfully.");
        
        server.sendHeader("Location", "/");
        server.send(303); // Redirect to root
    }
}

void setup() {
    Serial.begin(115200);

    // Start SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount file system");
        return;
    }

    loadButtonOutputs(); // Load the configuration at startup

    // Configure button pins as inputs with pull-up resistors
    for (int pin : availableGPIOPins) {
        pinMode(pin, INPUT_PULLUP);
    }

    // Initialize Wi-Fi
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected.");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Failed to connect to WiFi. Starting in AP mode.");
        WiFi.softAP("Gamepad-Config", "12345678");
        Serial.println("AP mode active.");
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP());
    }

    // Start the web server
    server.on("/", handleRoot);
    server.on("/set", handleSet);
    server.begin();
    
    // Initialize the gamepad
    gamepad.begin(&config); // Start Bluetooth with custom config
}

void loop() {
    server.handleClient(); // Handle web server clients

    // Check for button presses
    for (size_t i = 0; i < buttonPins.size(); i++) {
        if (digitalRead(buttonPins[i]) == LOW) {
            gamepad.press(buttonOutputs[i]); // Send corresponding output
        } else {
            gamepad.release(buttonOutputs[i]); // Release the button if not pressed
        }
    }
}
