/** =========================================================================
 * @example{lineno} timing_tests.ino
 * @author Sara Damiano <sdamiano@stroudcenter.org>
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 *
 * @brief This is a series of timing tests for the Geolux Hydrocam.
 *
 * @m_examplenavigation{extra_timing_tests,}
 * ======================================================================= */

// ---------------------------------------------------------------------------
// Include the base required libraries
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <GeoluxCamera.h>
#include <SdFat.h>

// Construct the camera instance
GeoluxCamera  camera;
const int32_t serialBaud = 115200;  // Baud rate for serial monitor
// int16_t       camera_power_pin = 65;      // power pin for the camera
// int16_t camera_power_pin       = 22;  // power pin for the camera
int16_t camera_power_pin       = 56;  // power pin for the camera
int16_t adapter_power_pin      = 22;  // power pin for the RS232 adapter
int16_t seconds_between_images = 5;   // how long to wait between snapshot attempts
bool    flip                   = 0;

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else   // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN
#ifndef SDCARD_SPI
#define SDCARD_SPI SPI
#endif  // SDCARD_SPI

#if (defined(ARDUINO_ARCH_SAMD)) && !defined(__SAMD51__)
// Dispite the 48MHz clock speed, the max SPI speed of a SAMD21 is 12 MHz
// see https://github.com/arduino/ArduinoCore-samd/pull/212
// The Adafruit SAMD core does NOT automatically manage the SPI speed, so
// this needs to be set.
SdSpiConfig customSdConfig(static_cast<SdCsPin_t>(SD_CS_PIN), (uint8_t)(DEDICATED_SPI),
                           SD_SCK_MHZ(12), &SDCARD_SPI);
#elif defined(ARDUINO_ARCH_SAMD)
// The SAMD51 is fast enough to handle SPI_FULL_SPEED=SD_SCK_MHZ(50).
// The SPI library of the Adafruit/Arduino AVR core will automatically
// adjust the full speed of the SPI clock down to whatever the board can
// handle.
SdSpiConfig customSdConfig(static_cast<SdCsPin_t>(SD_CS_PIN), (uint8_t)(DEDICATED_SPI),
                           SPI_FULL_SPEED, &SDCARD_SPI);
#else
SdSpiConfig customSdConfig(static_cast<SdCsPin_t>(SD_CS_PIN));
#endif


// construct the SD card and file instances
#if SD_FAT_TYPE == 0 && !defined(ESP8266) && !(defined(__AVR__) && FLASHEND < 0X8000)
SdFat sd;
File  imgFile;
File  metadataFile;
#elif SD_FAT_TYPE == 1 || defined(ESP8266) || (defined(__AVR__) && FLASHEND < 0X8000)
SdFat32 sd;
File32  imgFile;
File32  metadataFile;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile  imgFile;
ExFile  metadataFile;
#elif SD_FAT_TYPE == 3
SdFs   sd;
FsFile imgFile;
FsFile metadataFile;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

// Construct a Serial object for Modbus
#if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_FEATHER328P)
// The Uno only has 1 hardware serial port, which is dedicated to comunication with the
// computer. If using an Uno, you will be restricted to using AltSofSerial or
// SoftwareSerial
#include <SoftwareSerial.h>
const int SSRxPin = 10;  // Receive pin for software serial (Rx on RS485 adapter)
const int SSTxPin = 11;  // Send pin for software serial (Tx on RS485 adapter)
#pragma message("Using Software Serial for the Uno on pins 10 and 11")
SoftwareSerial cameraSerial(SSRxPin, SSTxPin);
// AltSoftSerial cameraSerial;
#elif defined ESP8266
#include <SoftwareSerial.h>
#pragma message("Using Software Serial for the ESP8266")
SoftwareSerial cameraSerial;
#elif defined(NRF52832_FEATHER) || defined(ARDUINO_NRF52840_FEATHER)
#pragma message("Using TinyUSB for the NRF52")
#include <Adafruit_TinyUSB.h>
HardwareSerial& cameraSerial = Serial1;
#elif !defined(NO_GLOBAL_SERIAL1) && !defined(STM32_CORE_VERSION)
// This is just a assigning another name to the same port, for convienence
// Unless it is unavailable, always prefer hardware serial.
#pragma message("Using HarwareSerial / Serial1")
HardwareSerial& cameraSerial = Serial1;
#else
// This is just a assigning another name to the same port, for convienence
// Unless it is unavailable, always prefer hardware serial.
#pragma message("Using HarwareSerial / Serial")
HardwareSerial& cameraSerial = Serial;
#endif

uint16_t                    image_number = 1;  // for file naming
uint32_t                    start_millis = 0;  // for tracking timing
GeoluxCamera::geolux_status camera_status;     // for the current status
uint32_t                    wait_time;         // for tracking how long operations take
uint32_t                    min_boot_time     = 120000;
uint32_t                    max_boot_time     = 0;
uint32_t                    min_ready_time    = 120000;
uint32_t                    max_ready_time    = 0;
uint32_t                    min_settings_time = 120000;
uint32_t                    max_settings_time = 0;
uint32_t                    min_image_time    = 120000;
uint32_t                    max_image_time    = 0;
uint32_t                    min_xfer_time     = 120000;
uint32_t                    max_xfer_time     = 0;

void autofocus_camera() {
    Serial.println("Asking camera to autofocus");
    if (camera.runAutofocus() == GeoluxCamera::OK) {
        Serial.println("Autofocus started successfully!");
        start_millis = millis();
    } else {
        Serial.println("Autofocus failed!");
        return;
    }
    // wait for focus; this takes ~30s (ridiculous...)
    wait_time = camera.waitForReady(25000L);
    if (wait_time) {
        Serial.print("Autofocus finished after ");
        Serial.print(wait_time);
        Serial.println(" milliseconds.");
    } else {
        Serial.println("Autofocus timed out!");
        return;
    }
    Serial.print("New focus point is: ");
    Serial.print(camera.getAutofocusX());
    Serial.print(',');
    Serial.print(camera.getAutofocusY());
    Serial.print(" position ");
    Serial.println(camera.getFocusPosition());
}

// ==========================================================================
//  Arduino Setup Function
// ==========================================================================
void setup() {
    // power pin mode
    pinMode(camera_power_pin, OUTPUT);
    pinMode(adapter_power_pin, OUTPUT);

    // Turn on the "main" serial port for debugging via USB Serial Monitor
    Serial.begin(serialBaud);
    while (!Serial &&
           (millis() < 10000L));  // wait for Arduino Serial Monitor (native USB boards)

    Serial.println("Geolux Camera Demo!");
    Serial.println();

    cameraSerial.begin(serialBaud);
    camera.begin(cameraSerial);
    camera.streamDump();  // dump anything in the stream, just in case


    // see if the card is present and can be initialized:
    if (!sd.begin(customSdConfig)) { Serial.println("Card failed, or not present"); }

    // power the camera
    // for (int i = 0; i < 25; i++) {
    Serial.println(F("Power off for 10s"));
    digitalWrite(camera_power_pin, LOW);
    digitalWrite(adapter_power_pin, LOW);
    delay(10000L);
    start_millis = millis();
    digitalWrite(camera_power_pin, HIGH);
    digitalWrite(adapter_power_pin, HIGH);
    Serial.println(F("Wait up to 5s for power to settle and camera to warm up"));
    // wait until the start up message comes over
    while (cameraSerial.available() < 15 && millis() - start_millis < 5000L) {}
    uint32_t boot_time = millis() - start_millis;
    while (cameraSerial.available()) {
        Serial.write(cameraSerial.read());
        delay(1);
    }
    Serial.print(F("Camera booted after "));
    Serial.print(boot_time);
    Serial.println(F("ms"));
    min_boot_time = min(min_boot_time, boot_time);
    max_boot_time = max(max_boot_time, boot_time);
    Serial.print(F("Minimum boot time is "));
    Serial.print(min_boot_time);
    Serial.println(F("ms"));
    Serial.print(F("Maximum boot time is "));
    Serial.print(max_boot_time);
    Serial.println(F("ms"));
    // }

    camera.streamDump();  // dump anything in the stream, just in case

    Serial.println("Restarting camera");
    start_millis = millis();
    if (camera.restart()) {
        Serial.print("Restarted successfully in ");
        Serial.print(millis() - start_millis);
        Serial.println(F("ms"));
    } else {
        Serial.println("Restart failed!");
    }

    Serial.println("Printing all camera info");
    camera.printCameraInfo(Serial);

    Serial.print("Camera is serial number:");
    Serial.println(camera.getCameraSerialNumber());
    Serial.print("Current camera firmware is: ");
    Serial.println(camera.getCameraFirmware());

    Serial.print("Current image resolution is: ");
    Serial.println(camera.getResolution());
    Serial.print("Current jpg compression quality is: ");
    Serial.println(camera.getQuality());
    Serial.print("Current maximum jpg size is: ");
    Serial.println(camera.getJPEGMaximumSize());

    // Configure jpeg parameters
    start_millis = millis();
    Serial.println("Setting resolution to 2592x1944");
    camera.setResolution("2592x1944");
    Serial.println("Setting jpg compression quality to 90");
    camera.setQuality(90);
    Serial.println("Setting jpg maximum size to 0 to allow resolution and compression "
                   "to apply without further processing.");
    camera.setJPEGMaximumSize(0);

    // wait for camera to be ready
    Serial.println("Waiting for changes to complete");
    wait_time = camera.waitForReady(2500L);
    if (wait_time) {
        Serial.print("Camera ready after ");
        Serial.print(wait_time);
        Serial.println(" milliseconds.");
    } else {
        Serial.print("Camera timed out!");
    }
}

void loop() {
    Serial.println(F("Power off for 10s"));
    digitalWrite(camera_power_pin, LOW);
    digitalWrite(adapter_power_pin, LOW);
    delay(10000L);
    start_millis = millis();
    digitalWrite(camera_power_pin, HIGH);
    digitalWrite(adapter_power_pin, HIGH);
    Serial.println(F("Wait up to 5s for power to settle and camera to warm up"));
    // wait until the start up message comes over
    while (cameraSerial.available() < 15 && millis() - start_millis < 5000L) {}
    uint32_t boot_time = millis() - start_millis;
    while (cameraSerial.available()) {
        Serial.write(cameraSerial.read());
        delay(1);
    }
    Serial.print(F("Camera booted after "));
    Serial.print(boot_time);
    Serial.println(F("ms"));
    min_boot_time = min(min_boot_time, boot_time);
    max_boot_time = max(max_boot_time, boot_time);
    Serial.print(F("Minimum boot time is "));
    Serial.print(min_boot_time);
    Serial.println(F("ms"));
    Serial.print(F("Maximum boot time is "));
    Serial.print(max_boot_time);
    Serial.println(F("ms"));

    // wait for camera to be ready
    start_millis = millis();
    Serial.print("Waiting for camera to be ready for image ");
    Serial.print(image_number);
    Serial.println(" .. ");
    wait_time = camera.waitForReady(30);
    if (wait_time) {
        Serial.print("Camera ready after ");
        Serial.print(wait_time);
        Serial.println(" milliseconds.");
    } else {
        Serial.print("Camera timed out!");
        Serial.println("Retrying in 30s");
        delay(30000L);
        return;
    }
    min_ready_time = min(min_ready_time, wait_time);
    max_ready_time = max(max_ready_time, wait_time);
    Serial.print(F("Minimum ready time is "));
    Serial.print(min_ready_time);
    Serial.println(F("ms"));
    Serial.print(F("Maximum ready time is "));
    Serial.print(max_ready_time);
    Serial.println(F("ms"));

    // Reset resolution
    start_millis = millis();
    Serial.println("Setting resolution to 2592x1944");
    camera.setResolution("2592x1944");
    // wait for camera to be ready
    Serial.println("Waiting for changes to complete");
    wait_time = camera.waitForReady(30);
    if (wait_time) {
        Serial.print("Camera finished changes after ");
        Serial.print(wait_time);
        Serial.println(" milliseconds.");
    } else {
        Serial.print("Camera timed out!");
    }
    min_settings_time = min(min_settings_time, wait_time);
    max_settings_time = max(max_settings_time, wait_time);
    Serial.print(F("Minimum setting change time is "));
    Serial.print(min_settings_time);
    Serial.println(F("ms"));
    Serial.print(F("Maximum setting change time is "));
    Serial.print(max_settings_time);
    Serial.println(F("ms"));

    // dump anything in the camera stream, just in case
    while (cameraSerial.available()) { cameraSerial.read(); }

    if (flip) { autofocus_camera(); }

    // see if the card is present and can be initialized:
    if (!sd.begin(customSdConfig)) {
        Serial.println("Card failed, or not present");
        Serial.println("Retrying in 30s");
        delay(30000L);
        return;
    }

    // generate a new numbered filename
    // If a file with the same number already exists on the SD card, this will increment
    // up the file number until it gets to a unique filename.
    bool file_exits = true;
    do {
        // set a new filename
        char filename[15] = {'\0'};
        if (flip) {
            strcat(filename, "AUTO");
        } else {
            strcat(filename, "SKIP");
        }
        char filenumber[6];
        sprintf(filenumber, "%05d", image_number);
        strcat(filename, filenumber);
        strcat(filename, ".jpg");
        Serial.print("Trying file name ");
        Serial.println(filename);

        file_exits = sd.exists(filename);
        if (file_exits) {
            Serial.print(filename);
            Serial.println(" already exists!");
            image_number++;
        } else {
            file_exits = false;
            // Create and then open the file in write mode
            if (imgFile.open(filename, O_CREAT | O_WRITE | O_AT_END)) {
                Serial.print(F("Created new file: "));
                Serial.println(filename);
            } else {
                Serial.println("Creating a new file failed!");
                Serial.println("Retrying in 30s");
                delay(30000L);
                return;
            }
        }
    } while (file_exits);

    // run the autofocus before each image
    // NOTE: The autofocus takes an *absurd* 30s to complete!
    // autofocus_camera();

    start_millis = millis();
    Serial.print("Requesting that the camera take a picture ... ");
    if (camera.takeSnapshot() == GeoluxCamera::OK) {
        Serial.println("picture started successfully!");
        start_millis = millis();
    } else {
        Serial.println("Snapshot failed!");
        Serial.println("Retrying in 30s");
        delay(30000L);
        return;
    }

    // wait for snapshot to finish - takes a bit over a second at 800x600, less for
    // smaller images, more for bigger ones
    uint32_t snapshot_time = camera.waitForReady();
    if (snapshot_time) {
        Serial.print("Snapshot finished after ");
        Serial.print(millis() - start_millis);
        Serial.println(" milliseconds.");
    } else {
        Serial.print("Snapshot timed out!");
        Serial.println("Retrying in 30s");
        delay(30000L);
        return;
    }
    min_image_time = min(min_image_time, snapshot_time);
    max_image_time = max(max_image_time, snapshot_time);
    Serial.print("Minimum imaging time ");
    Serial.print(min_image_time);
    Serial.println(" milliseconds.");
    Serial.print("Maximum imaging time ");
    Serial.print(max_image_time);
    Serial.println(" milliseconds.");

    int32_t image_size = camera.getImageSize();
    Serial.print("Completed image is ");
    Serial.print(image_size);
    Serial.println(" bytes.");

    // dump anything in the camera stream, just in case
    while (cameraSerial.available()) { cameraSerial.read(); }

    start_millis = millis();
    // transfer the image from the camera to a file on the SD card
    uint32_t bytes_transferred = camera.transferImage(imgFile, image_size);
    // Close the image file
    imgFile.close();
    // See how long it took us
    uint32_t transfer_time = millis() - start_millis;

    Serial.print("Wrote ");
    Serial.print(bytes_transferred, DEC);
    Serial.print(" of expected ");
    Serial.print(image_size, DEC);
    Serial.print(" bytes to the SD card - a difference of ");
    Serial.print(bytes_transferred - image_size, DEC);
    Serial.println(" bytes");
    Serial.print("Total read/write time was ");
    Serial.print(transfer_time);
    Serial.println("ms");

    min_xfer_time = min(min_xfer_time, transfer_time);
    max_xfer_time = max(max_xfer_time, transfer_time);
    Serial.print("Minimum transfer time ");
    Serial.print(min_xfer_time);
    Serial.println(" milliseconds.");
    Serial.print("Maximum transfer time ");
    Serial.print(max_xfer_time);
    Serial.println(" milliseconds.");

    image_number++;
    flip = !flip;
    Serial.print(F("Wait "));
    Serial.print(seconds_between_images);
    Serial.println(F(" seconds before next image"));
    for (uint16_t i = seconds_between_images; i; i--) {
        Serial.print("wait...  ");
        delay(1000L);
    }
    Serial.println();
    Serial.println("-----------------------------------------");
}
