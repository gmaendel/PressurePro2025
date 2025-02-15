/* 
 * Project PressurePro2025
 * Author: Greg Maendel
 * Date: 2/14/2025
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */
#include "Particle.h"
#include "oled-wing-adafruit.h"

// Create an instance of the OLED Wing display.
OledWingAdafruit display;

// ----- Global Variables & Defaults (using double) ----- //
double lowSetpoint    = 25.0;   // Default low setpoint (PSI)
double highSetpoint   = 100.0;  // Default high setpoint (PSI)
double lowHys         = 2.0;    // Hysteresis for low setpoint (PSI)
double highHys        = 2.0;    // Hysteresis for high setpoint (PSI)
// selectedSetpoint: 0 = use lowSetpoint, 1 = use highSetpoint
int selectedSetpoint = 0;       

// Variable to hold the current tire pressure (PSI)
double currentPressure = 0.0;

// ----- Sensor Conversion Constants ----- //
// Assumes that analogRead(A0) returns 0-4095 corresponding to 0-150 PSI.
const double PSI_MAX       = 150.0;
const double ADC_MAX       = 4095.0;
const double PSI_PER_UNIT  = PSI_MAX / ADC_MAX;

// Global variable for publish timing
unsigned long lastPublishTime = 0;

// ----- Cloud Function Prototypes ----- //
int setHighSetpoint(String args);
int setLowSetpoint(String args);
int setSetpointSelection(String args);
int setLowHys(String args);
int setHighHys(String args);

// ----- Button Pin Definitions ----- //
// Button A (wired to D2) selects the HIGH setpoint.
// Button B (wired to D3) selects the LOW setpoint.
const int buttonA = D2; // HIGH setpoint button
const int buttonB = D3; // LOW setpoint button

void setup() {
    // ----- Initialize Valve Outputs ----- //
    pinMode(D6, OUTPUT);  // Deflation valve control
    pinMode(D7, OUTPUT);  // Inflation valve control
    digitalWrite(D6, LOW);
    digitalWrite(D7, LOW);

    // ----- Initialize the OLED Wing Display ----- //
    display.setup();
    display.clearDisplay();
    display.display();

    // ----- Setup Buttons (using internal pull-ups) ----- //
    pinMode(buttonA, INPUT_PULLUP);
    pinMode(buttonB, INPUT_PULLUP);

    // ----- Register Particle Cloud Functions ----- //
    Particle.function("HighSetpoint", setHighSetpoint);
    Particle.function("LowSetpoint", setLowSetpoint);
    Particle.function("SetpointSelection", setSetpointSelection);
    Particle.function("LowHys", setLowHys);
    Particle.function("HighHys", setHighHys);

    // ----- Expose the current pressure as a Particle variable ----- //
    Particle.variable("pressure", currentPressure);
}

void loop() {
    // Run the display's internal loop.
    display.loop();

    // ----- Read and Convert Sensor Value ----- //
    int sensorValue = analogRead(A0);
    currentPressure = sensorValue * PSI_PER_UNIT;

    // ----- Publish Pressure to Particle Cloud Every 15 Seconds ----- //
    if (millis() - lastPublishTime >= 21600000) {
        // Publish as a private event; adjust event name as needed
        Particle.publish("pressure", String(currentPressure, 1), PRIVATE);
        lastPublishTime = millis();
    }

    // ----- Check Button Presses for Setpoint Selection ----- //
    // (Buttons are active LOW)
    if (digitalRead(buttonA) == LOW) {
        selectedSetpoint = 1;  // Button A: select HIGH setpoint
        delay(200); // Simple debounce delay
    }
    if (digitalRead(buttonB) == LOW) {
        selectedSetpoint = 0;  // Button B: select LOW setpoint
        delay(200); // Simple debounce delay
    }

    // ----- Determine the Active Target and Hysteresis ----- //
    double target, hysteresis;
    if (selectedSetpoint == 0) {
        target = lowSetpoint;
        hysteresis = lowHys;
    } else {
        target = highSetpoint;
        hysteresis = highHys;
    }

    // ----- Control Logic for Valve Operation ----- //
    // Inflate if pressure is below target minus hysteresis.
    if (currentPressure < target - hysteresis) {
        digitalWrite(D7, HIGH); // Activate inflation valve
        digitalWrite(D6, LOW);  // Ensure deflation valve is off
    }
    // Deflate if pressure is above target plus hysteresis.
    else if (currentPressure > target + hysteresis) {
        digitalWrite(D6, HIGH); // Activate deflation valve
        digitalWrite(D7, LOW);  // Ensure inflation valve is off
    }
    // Within the hysteresis band: both valves off.
    else {
        digitalWrite(D6, LOW);
        digitalWrite(D7, LOW);
    }

    // ----- Update the OLED Display ----- //
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Tire Pressure:");
    display.print(currentPressure, 1); // One decimal place
    display.println(" PSI");

    display.println();
    display.print("Active Setpoint: ");
    if (selectedSetpoint == 0) {
        display.println("LOW");
    } else {
        display.println("HIGH");
    }
    display.display();

    delay(1000); // Loop delay (adjust as needed)
}

// ----- Cloud Function Implementations ----- //

/*
 * Sets the high setpoint.
 * Expects a string representing a positive value (PSI).
 * Uses a local float variable for conversion.
 */
int setHighSetpoint(String args) {
    float val = atof(args.c_str());
    if(val > 0) {
        highSetpoint = val;
        return 1;
    }
    return -1;
}

/*
 * Sets the low setpoint.
 * Expects a string representing a positive value (PSI).
 * Uses a local float variable for conversion.
 */
int setLowSetpoint(String args) {
    float val = atof(args.c_str());
    if(val > 0) {
        lowSetpoint = val;
        return 1;
    }
    return -1;
}

/*
 * Selects the active setpoint.
 * Expects "low" or "high" (case insensitive) or a numeric value (0 for low, 1 for high).
 */
int setSetpointSelection(String args) {
    args.toLowerCase();
    if (args == "low") {
        selectedSetpoint = 0;
        return 0;
    } else if (args == "high") {
        selectedSetpoint = 1;
        return 1;
    } else {
        int sel = args.toInt();
        if (sel == 0 || sel == 1) {
            selectedSetpoint = sel;
            return sel;
        }
    }
    return -1;
}

/*
 * Sets the hysteresis value for the low setpoint.
 * Expects a string representing a non-negative value (PSI).
 * Uses a local float variable for conversion.
 */
int setLowHys(String args) {
    float val = atof(args.c_str());
    if(val >= 0) {
        lowHys = val;
        return 1;
    }
    return -1;
}

/*
 * Sets the hysteresis value for the high setpoint.
 * Expects a string representing a non-negative value (PSI).
 * Uses a local float variable for conversion.
 */
int setHighHys(String args) {
    float val = atof(args.c_str());
    if(val >= 0) {
        highHys = val;
        return 1;
    }
    return -1;
}