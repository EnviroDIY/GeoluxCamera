/** =========================================================================
 * @example{lineno} dated_image.ino
 * @author Sara Damiano <sdamiano@stroudcenter.org>
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 *
 * @brief This example asks the camera to take a picture and then retrieves the data and
 * saves it to an SD card using an attached RV-8803 RTC to name and date the files with
 * the current date/time.
 *
 * This example also write some metadata about the image process to a metadata csv file.
 *
 * @note This example does **NOT** set the time on the RTC, it assumes the RTC time has
 * been correctly set in advance.
 *
 * @m_examplenavigation{example_dated_image,}
 * ======================================================================= */

// ---------------------------------------------------------------------------
// Include the base required libraries
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <GeoluxCamera.h>
#include <SdFat.h>
#include <SparkFun_RV8803.h>

// Real time clock - for timing and file naming
RV8803 rtc;

// Construct the camera instance
GeoluxCamera  camera;
const int32_t serialBaud = 115200;  // Baud rate for serial monitor
// int16_t       camera_power_pin = 65;      // power pin for the camera
int16_t camera_power_pin       = 22;     // power pin for the camera
int16_t seconds_between_images = 300;    // how long to wait between snapshot attempts
bool    test_power             = false;  // whether to test the boot-up time

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
const char*                 metadataFilename = "image_metadata.csv";

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

// This function is used to automatically mark files as created/accessed/modified when
// operations are done by the SdFat library.
// User provided date time callback function.
// See SdFile::dateTimeCallback() for usage.
void dateTime(uint16_t* date, uint16_t* time) {
    // User gets date and time from GPS or real-time
    // clock in real callback function
    rtc.updateTime();

    // return date using FAT_DATE macro to format fields
    *date = FAT_DATE(rtc.getYear(), rtc.getMonth(), rtc.getDate());

    // return time using FAT_TIME macro to format fields
    *time = FAT_TIME(rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
}

// ==========================================================================
//  Arduino Setup Function
// ==========================================================================
void setup() {
    // power pin mode
    pinMode(camera_power_pin, OUTPUT);

    // Turn on the "main" serial port for debugging via USB Serial Monitor
    Serial.begin(serialBaud);
    while (!Serial &&
           (millis() < 10000L));  // wait for Arduino Serial Monitor (native USB boards)

    Serial.println("Geolux Camera Demo!");
    Serial.println();

    cameraSerial.begin(serialBaud);
    camera.begin(cameraSerial);
    camera.streamDump();  // dump anything in the stream, just in case

    // Get the time from the clock
    Serial.println("Starting RV8803 RTC");
    Wire.begin();  // the SparkFun RV-8803 library doesn't begin Wire, we need to
    rtc.begin();
    rtc.set24Hour();
    // void setTimeZoneQuarterHours(int8_t quarterHours);
    // Write the time zone to RV8803_RAM as int8_t (signed) in 15 minute
    // increments
    // This must happen here in the begin, not when setting the internal
    // timezone variable because this requires communication with the RTC which
    // can only happen during the run, not during compilation.
    rtc.setTimeZoneQuarterHours(-5 * 4);

    Serial.println("Pull new time from RTC buffers..");
    rtc.updateTime();
    Serial.print("Current time is: ");
    Serial.println(rtc.stringTime8601());

    // set date time callback function for files on the SD Card
    SdFile::dateTimeCallback(dateTime);


    // see if the card is present and can be initialized:
    if (!sd.begin(customSdConfig)) { Serial.println("Card failed, or not present"); }
    // set up a metadata file on the SD card
    metadataFile.open(metadataFilename, O_CREAT | O_WRITE | O_AT_END);
    metadataFile.println("ImageName, Snapshot Time, Image Size, Total Bytes Written, "
                         "Write Size Error, Transfer Time");
    metadataFile.close();

    // power the camera
    if (test_power && camera_power_pin >= 0) {
        digitalWrite(camera_power_pin, LOW);
        Serial.println(F("Wait 15s with power off"));
        for (uint8_t i = 15; i; i--) {
            Serial.print("wait...  ");
            delay(1000L);
        }
        Serial.println();
    }
    start_millis = millis();
    digitalWrite(camera_power_pin, HIGH);
    Serial.println(F("Wait up to 5s for power to settle and camera to warm up"));
    // wait until the start up message comes over
    while (cameraSerial.available() < 15 && millis() - start_millis < 5000L) {}
    Serial.print(F("Camera booted after "));
    Serial.print(millis() - start_millis);
    Serial.println(F("ms"));

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
    Serial.println("Setting resolution to 1600x1200");
    camera.setResolution("1600x1200");
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

    autofocus_camera();
}

void loop() {
    // wait for camera to be ready
    start_millis = millis();
    Serial.print("Waiting for camera to be ready for image ");
    Serial.print(image_number);
    Serial.println(" .. ");
    wait_time = camera.waitForReady();
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
    // dump anything in the camera stream, just in case
    while (cameraSerial.available()) { cameraSerial.read(); }

    // see if the card is present and can be initialized:
    if (!sd.begin(customSdConfig)) {
        Serial.println("Card failed, or not present");
        Serial.println("Retrying in 30s");
        delay(30000L);
        return;
    }

    rtc.updateTime();
    Serial.print("Current time is: ");
    Serial.println(rtc.stringTime8601());

    // set a new filename based on the current RTC time
    char filename[20] = {'\0'};
    char temp_buff[6] = {'\0'};
    sprintf(temp_buff, "%02d", rtc.getYear());
    strcat(filename, temp_buff);
    sprintf(temp_buff, "%02d", rtc.getMonth());
    strcat(filename, temp_buff);
    sprintf(temp_buff, "%02d", rtc.getDate());
    strcat(filename, temp_buff);
    strcat(filename, "_");
    sprintf(temp_buff, "%02d", rtc.getHours());
    strcat(filename, temp_buff);
    sprintf(temp_buff, "%02d", rtc.getMinutes());
    strcat(filename, temp_buff);
    sprintf(temp_buff, "%02d", rtc.getSeconds());
    strcat(filename, temp_buff);
    strcat(filename, ".jpg");
    Serial.print(F("Attempting to create the file: "));
    Serial.println(filename);
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

    int32_t image_size = camera.getImageSize();
    Serial.print("Completed image is ");
    Serial.print(image_size);
    Serial.println(" bytes.");

    // dump anything in the camera stream, just in case
    while (cameraSerial.available()) { cameraSerial.read(); }

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

    // write to the metadata file
    metadataFile.open(metadataFilename, O_CREAT | O_WRITE | O_AT_END);
    metadataFile.print(filename);
    metadataFile.print(',');
    metadataFile.print(snapshot_time);
    metadataFile.print(',');
    metadataFile.print(image_size);
    metadataFile.print(',');
    metadataFile.print(bytes_transferred);
    metadataFile.print(',');
    metadataFile.print(bytes_transferred - image_size);
    metadataFile.print(',');
    metadataFile.println(transfer_time);
    metadataFile.close();

    image_number++;
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
