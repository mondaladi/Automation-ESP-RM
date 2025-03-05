#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include <Bounce2.h>

const char *service_name = "PROV_aditya";
const char *pop = "12345678";

// define the Device Names
char deviceName_1[] = "Power Switch";

// define the GPIO connected with Relays and switches
static uint8_t RelayPin1 = 22; 
static uint8_t SwitchPin1 = 27; 
static uint8_t gpio_reset = 0;

/* Variable for reading pin status */
bool toggleState_1 = LOW; // Define integer to remember the toggle state for relay 1

Bounce debouncer1 = Bounce();

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx);

// The framework provides some standard device types like switch, lightbulb, fan, temperature sensor.
static Switch my_switch1(deviceName_1, &RelayPin1);

void sysProvEvent(arduino_event_t *sys_event)
{
    switch (sys_event->event_id)
    {
    case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
        printQR(service_name, pop, "ble");
#else
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
        printQR(service_name, pop, "softap");
#endif
        break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.printf("\nConnected to Wi-Fi!\n");
        break;
    }
}

void setup()
{
    uint32_t chipId = 0;

    Serial.begin(115200);

    // Set the Relays GPIOs as output mode
    pinMode(RelayPin1, OUTPUT);

    // Configure the input GPIOs
    pinMode(SwitchPin1, INPUT_PULLUP);
    pinMode(gpio_reset, INPUT);

    // Initialize debouncer
    debouncer1.attach(SwitchPin1);
    debouncer1.interval(25); // Debounce interval in ms

    // Write to the GPIOs the default state on booting
    digitalWrite(RelayPin1, toggleState_1);

    Node my_node;
    my_node = RMaker.initNode("Home Server");

    // Standard switch device
    my_switch1.addCb(write_callback);

    // Add switch device to the node
    my_node.addDevice(my_switch1);

    // This is optional
    RMaker.enableOTA(OTA_USING_PARAMS);
    RMaker.enableTZService();
    RMaker.enableSchedule();

    for (int i = 0; i < 17; i = i + 8)
    {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }

    Serial.printf("\nChip ID:  %d Service Name: %s\n", chipId, service_name);

    Serial.printf("\nStarting ESP-RainMaker\n");
    RMaker.start();

    WiFi.onEvent(sysProvEvent);
#if CONFIG_IDF_TARGET_ESP32
    WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM, NETWORK_PROV_SECURITY_1, pop, service_name);
#else
    WiFiProv.beginProvision(NETWORK_PROV_SCHEME_SOFTAP, NETWORK_PROV_SCHEME_HANDLER_NONE, NETWORK_PROV_SECURITY_1, pop, service_name);
#endif

    my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, false);
}

void toggleRelayForDuration()
{
    digitalWrite(RelayPin1, HIGH); // Turn on the relay
    delay(1500);                 // Keep it on for 1.5 seconds
    digitalWrite(RelayPin1, LOW); // Turn off the relay
}

void loop()
{
    // Read GPIO0 (external button to reset device)
    if (digitalRead(gpio_reset) == LOW)
    { // Push button pressed
        Serial.printf("Reset Button Pressed!\n");
        // Key debounce handling
        delay(100);
        int startTime = millis();
        while (digitalRead(gpio_reset) == LOW)
            delay(50);
        int endTime = millis();

        if ((endTime - startTime) > 10000)
        {
            // If key pressed for more than 10secs, reset all
            Serial.printf("Reset to factory.\n");
            RMakerFactoryReset(2);
        }
        else if ((endTime - startTime) > 3000)
        {
            Serial.printf("Reset Wi-Fi.\n");
            // If key pressed for more than 3secs, but less than 10, reset Wi-Fi
            RMakerWiFiReset(2);
        }
    }
    delay(100);

    // Handle manual button press
    debouncer1.update();
    if (debouncer1.fell())
    {
        Serial.println("Manual Button Press Detected!");
        toggleRelayForDuration(); // Toggle relay for 1.5 seconds
        toggleState_1 = !toggleState_1;
        my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, toggleState_1);
    }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();

    if (strcmp(device_name, deviceName_1) == 0)
    {
        Serial.printf("Lightbulb = %s\n", val.val.b ? "true" : "false");

        if (strcmp(param_name, "Power") == 0)
        {
            Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
            toggleRelayForDuration(); // Toggle relay for 1.5 seconds
            toggleState_1 = val.val.b;
            param->updateAndReport(val);
        }
    }
}
