/** =========================================================================
 * @example{lineno} read_by_chunk.ino
 * @author Sara Damiano <sdamiano@stroudcenter.org>
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 *
 * @brief This example asks the camera to take a picture and then retrieves the data and
 * saves it to an SD card using individual chunks that are transferred through the main
 * processor's memory.
 *
 * Reading in "small" chunks and transferring the as bulk memory reads as done by this
 * program could be faster with an optimized SD card / SPI driver, but in practice is
 * most likely slower and less reliable than the huge chunk 1-character-at-a-time reads
 * used by the transferImage(...) function snapshot example..
 *
 * @m_examplenavigation{example_read_by_chunk,}
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
int16_t camera_power_pin       = 22;  // power pin for the camera
int16_t seconds_between_images = 5;   // how long to wait between snapshot attempts

#define PRINT_PARTIAL_JPEG_HEX

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else   // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

#if (defined(ARDUINO_ARCH_SAMD)) && !defined(__SAMD51__)
// Dispite the 48MHz clock speed, the max SPI speed of a SAMD21 is 12 MHz
// see https://github.com/arduino/ArduinoCore-samd/pull/212
// The Adafruit SAMD core does NOT automatically manage the SPI speed, so
// this needs to be set.
SdSpiConfig customSdConfig(static_cast<SdCsPin_t>(SD_CS_PIN), (uint8_t)(DEDICATED_SPI),
                           SD_SCK_MHZ(12), &SDCARD_SPI);
#else
// The SAMD51 is fast enough to handle SPI_FULL_SPEED=SD_SCK_MHZ(50).
// The SPI library of the Adafruit/Arduino AVR core will automatically
// adjust the full speed of the SPI clock down to whatever the board can
// handle.
SdSpiConfig customSdConfig(static_cast<SdCsPin_t>(SD_CS_PIN), (uint8_t)(DEDICATED_SPI),
                           SPI_FULL_SPEED, &SDCARD_SPI);
#endif


// construct the SD card and file instances
#if SD_FAT_TYPE == 0
SdFat sd;
File  imgFile;
#elif SD_FAT_TYPE == 1
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

uint16_t                    image_number = 1;  // for file naming
uint32_t                    start_millis = 0;  // for tracking timing
GeoluxCamera::geolux_status camera_status;     // for the current status
uint32_t                    wait_time;         // for tracking how long operations take

void print_padded_hex(uint8_t b) {
    // print zero padded hex
    char zph[3] = {'\0', '\0', '\0'};
    sprintf(zph, "%02x", b);
    Serial.print(zph);
}

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

    // Turn on the "main" serial port for debugging via USB Serial Monitor
    Serial.begin(serialBaud);
    while (!Serial &&
           (millis() < 10000L));  // wait for Arduino Serial Monitor (native USB boards)

    Serial.println("Geolux Camera Demo!");
    Serial.println();

    Serial1.begin(serialBaud);
    camera.begin(Serial1);
    camera.streamDump();  // dump anything in the stream, just in case


    // see if the card is present and can be initialized:
    if (!sd.begin(customSdConfig)) { Serial.println("Card failed, or not present"); }

    // power the camera
    start_millis = millis();
    digitalWrite(camera_power_pin, HIGH);
    Serial.println(F("Wait up to 5s for power to settle and camera to warm up"));
    // wait until the start up message comes over
    while (Serial1.available() < 15 && millis() - start_millis < 5000L) {}
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
    Serial.println("Setting resolution to 640x480");
    camera.setResolution("640x480");
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
    while (Serial1.available()) { Serial1.read(); }

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
        char filename[15] = "IMAG";
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

    int32_t image_size = camera.getImageSize();
    Serial.print("Completed image is ");
    Serial.print(image_size);
    Serial.println(" bytes.");

    // dump anything in the camera stream, just in case
    while (Serial1.available()) { Serial1.read(); }

    // Read all the data up to # bytes!
    int32_t total_bytes_read    = 0;  // for the number of bytes read
    int32_t total_bytes_written = 0;  // for the number of bytes written
    int32_t bytes_remaining     = image_size;
    int32_t chunk_number        = 0;
    int32_t chunk_size          = 512;  // don't go below 256
    int32_t start_next_chunk    = 0;
    int32_t chunks_needed       = ceil(image_size / chunk_size);

    start_millis = millis();

    while (total_bytes_read < image_size && millis() - start_millis < 120000L) {
        uint8_t buffer[chunk_size];
        auto*   write_start   = buffer;
        int32_t bytesToRead   = min(chunk_size, max(bytes_remaining, 1));
        int32_t bytes_read    = 0;
        int32_t bytes_written = 0;

        // ask for a chunk of image data
        // NOTE: The getImageChunk(...) function automatically prunes off the first two
        // bytes of the raw response, which are junk.
        bytes_read = camera.getImageChunk(buffer, total_bytes_read, bytesToRead);
        if (bytes_read != bytesToRead) {
            Serial.println("\nGot fewer characters than expected!");
        }
        // write the chunk to the SD card
        bytes_written = imgFile.write(buffer, bytes_read);

#ifdef PRINT_PARTIAL_JPEG_HEX
        // print first and last bits
        if (chunk_number == 0) {
            Serial.print(" ... ");
            for (int16_t i = 0; i < 16; i++) { print_padded_hex(buffer[i]); }
        }
        if (chunk_number == chunks_needed) {
            Serial.print(" ... ");
            for (int16_t i = bytes_read - 16; i < bytes_read; i++) {
                print_padded_hex(buffer[i]);
            }
        }
#elif defined(PRINT_FULL_JPEG_HEX)
        bool eof = false;
        for (int16_t i = 0; i < bytes_read; i++) {
            if (total_bytes_written == 0 && i == start_data_byte) {
                Serial.println("\n --Start JPG--\n");
            } else if (chunk_number == chunks_needed &&
                       i > bytes_read - start_data_byte - extra_read_buff - 1 &&
                       buffer[i] == 0) {
                if (!eof) { Serial.println("\n --Got 0--\n"); }
                eof = true;
            }
            print_padded_hex(buffer[i]);
        }
#endif
        if (millis() - start_millis > 120000L) {
            Serial.println("\n ----Timed out!----\n");
            break;
        }

        bytes_remaining -= bytes_read;
        total_bytes_read += bytes_read;
        total_bytes_written += bytes_written;
        chunk_number++;

        // if (bytes_read == 0) { break; }
    }
    // Close the image file
    imgFile.close();
    // See how long it took us
    uint32_t transfer_time = millis() - start_millis;

    Serial.print("\nUsed ");
    Serial.print(chunk_number, DEC);
    Serial.print(" chunks to read ");
    Serial.print(total_bytes_read, DEC);
    Serial.print(" bytes in ");
    Serial.print(chunk_size, DEC);
    Serial.println(" bytes chunks.");
    Serial.print("Wrote ");
    Serial.print(total_bytes_written, DEC);
    Serial.print(" of expected ");
    Serial.print(image_size, DEC);
    Serial.print(" bytes to the SD card - a difference of ");
    Serial.print(total_bytes_written - image_size, DEC);
    Serial.println(" bytes");
    Serial.print("Total read/write time was ");
    Serial.print(transfer_time);
    Serial.println("ms");

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
