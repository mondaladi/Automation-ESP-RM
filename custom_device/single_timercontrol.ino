#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include <Bounce2.h>

// Provisioning service details
const char *service_name = "PROV_aditya";
const char *pop = "12345678";

// Define device names
char deviceName_1[] = "Power Switch";
char deviceName_2[] = "Timer Input Selector";

// GPIO pin definitions
static uint8_t RelayPin1 = 26;
static uint8_t SwitchPin1 = 27;
static uint8_t gpio_reset = 0;

// Variables for states
bool toggleState_1 = LOW; // Relay state
int timerDuration = 1;    // Default timer (in minutes)

// Debounce object for manual button
Bounce debouncer1 = Bounce();

// Timer handle for auto turn-off
TimerHandle_t timerHandle = NULL;

// Declare devices
static Switch my_switch1(deviceName_1, &RelayPin1);
static Device my_device2(deviceName_2);

// Pointer to timer parameter
Param *timer_param_ptr;

// Timer Callback Function (Turns Off Relay)
void timerCallback(TimerHandle_t xTimer)
{
    Serial.println("Timer ended. Turning OFF relay.");
    digitalWrite(RelayPin1, LOW);
    toggleState_1 = LOW;
    my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, toggleState_1);
}

// Write Callback Function
void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();

    if (strcmp(device_name, deviceName_1) == 0 && strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0)
    {
        Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
        
        if (val.val.b) {
            digitalWrite(RelayPin1, HIGH);
            if (timerHandle) {
                xTimerStart(timerHandle, 0);  // Start the countdown timer
            }
        } else {
            digitalWrite(RelayPin1, LOW);
            xTimerStop(timerHandle, 0);  // Stop the timer if manually turned off
        }
        
        toggleState_1 = val.val.b;
        param->updateAndReport(val);
    }
    else if (strcmp(device_name, deviceName_2) == 0 && strcmp(param_name, "Input Selector") == 0)
    {
        Serial.printf("Timer duration changed to: %s\n", val.val.s);
        
        // Convert string input to integer (minutes)
        timerDuration = atoi(val.val.s);
        timer_param_ptr->updateAndReport(val);

        // Update timer duration
        if (timerHandle) {
            xTimerChangePeriod(timerHandle, pdMS_TO_TICKS(timerDuration * 60000), 0);
        }
    }
}

void setup()
{
    Serial.begin(115200);

    // Set GPIO modes
    pinMode(RelayPin1, OUTPUT);
    pinMode(SwitchPin1, INPUT_PULLUP);
    pinMode(gpio_reset, INPUT);

    // Initialize debouncer
    debouncer1.attach(SwitchPin1);
    debouncer1.interval(25); // Debounce interval in ms

    // Default relay state
    digitalWrite(RelayPin1, toggleState_1);

    // Initialize ESP RainMaker node
    Node my_node = RMaker.initNode("Home Server");

    // Add standard Power Switch device
    my_switch1.addCb(write_callback);
    my_node.addDevice(my_switch1);

    // Add Timer Input Selector
    my_device2.addNameParam();
    timer_param_ptr = new Param("Input Selector", "esp.param.input-selector", value("1"), PROP_FLAG_READ | PROP_FLAG_WRITE);
    timer_param_ptr->addUIType(ESP_RMAKER_UI_DROPDOWN);
    
    const char *validOptions[] = {"1", "2", "3", "4", "5"};
    timer_param_ptr->addValidStrList(validOptions, 5);

    my_device2.addParam(*timer_param_ptr);
    my_node.addDevice(my_device2);

    // Enable RainMaker features
    RMaker.enableOTA(OTA_USING_PARAMS);
    RMaker.enableTZService();
    RMaker.enableSchedule();

    // Start ESP RainMaker
    Serial.printf("\nStarting ESP-RainMaker\n");
    RMaker.start();

    // Configure Wi-Fi provisioning
    WiFi.onEvent([](arduino_event_t *sys_event) {
        if (sys_event->event_id == ARDUINO_EVENT_PROV_START) {
#if CONFIG_IDF_TARGET_ESP32
            Serial.printf("\nProvisioning Started on BLE with name: %s\n", service_name);
#else
            Serial.printf("\nProvisioning Started on SoftAP with name: %s\n", service_name);
#endif
        }
    });

#if CONFIG_IDF_TARGET_ESP32
    WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM, NETWORK_PROV_SECURITY_1, pop, service_name);
#else
    WiFiProv.beginProvision(NETWORK_PROV_SCHEME_SOFTAP, NETWORK_PROV_SCHEME_HANDLER_NONE, NETWORK_PROV_SECURITY_1, pop, service_name);
#endif

    // Initialize timer for relay auto-off
    timerHandle = xTimerCreate("RelayTimer", pdMS_TO_TICKS(timerDuration * 60000), pdFALSE, NULL, timerCallback);

    // Update initial states
    my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, toggleState_1);
    timer_param_ptr->updateAndReport(value("1"));
}

void loop()
{
    // Handle manual button press for Power Switch
    debouncer1.update();
    if (debouncer1.fell())
    {
        Serial.println("Manual Button Press Detected!");
        
        // Toggle relay state manually
        toggleState_1 = !toggleState_1;
        digitalWrite(RelayPin1, toggleState_1);

        // Start timer if turned ON manually
        if (toggleState_1 && timerHandle) {
            xTimerStart(timerHandle, 0);
        } else {
            xTimerStop(timerHandle, 0);
        }

        my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, toggleState_1);
    }

    delay(100);
}
