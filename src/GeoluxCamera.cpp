/**
 * @file       GeoluxCamera.cpp
 * @author     Sara Damiano
 * @copyright  Stroud Water Research Center
 * @date       December 2024
 */

#include "GeoluxCamera.h"

GeoluxCamera::GeoluxCamera() {}
GeoluxCamera::GeoluxCamera(Stream* stream) {
    _stream = stream;
}
GeoluxCamera::GeoluxCamera(Stream& stream) {
    _stream = &stream;
}
GeoluxCamera::~GeoluxCamera() {}

void GeoluxCamera::begin() {}
void GeoluxCamera::begin(Stream* stream) {
    _stream = stream;
}
void GeoluxCamera::begin(Stream& stream) {
    _stream = &stream;
}
void GeoluxCamera::begin(HardwareSerial* stream) {
    _stream = stream;
    stream->begin(GEOLUX_CAMERA_RS232_BAUD);
}
void GeoluxCamera::begin(HardwareSerial& stream) {
    _stream = &stream;
    stream.begin(GEOLUX_CAMERA_RS232_BAUD);
}


GeoluxCamera::geolux_status GeoluxCamera::takeSnapshot() {
    sendCommand(GF("take_snapshot"));
    return static_cast<geolux_status>(waitResponse());
}

GeoluxCamera::geolux_status GeoluxCamera::getStatus() {
    sendCommand(GF("get_status"));
    // this returns "READY" instead of "OK" and has no new line
    geolux_status resp = static_cast<geolux_status>(
        waitResponse(GF("READY"), GF("ERR"), GF("BUSY"), GF("NONE")));
    streamFind('\n');  // skip to the end of the line
    return resp;
}

int32_t GeoluxCamera::getImageSize() {
    sendCommand(GF("get_status"));
    // this returns "READY" instead of "OK" and has no new line
    waitResponse(GF("READY"), GF("ERR"), GF("BUSY"), GF("NONE"));
    streamFind(',');  // skip the comma
    uint32_t resp = _stream->parseInt();
    streamFind('\n');  // skip to the end of the line
    return static_cast<int32_t>(resp);
}

uint32_t GeoluxCamera::getImageChunk(uint8_t* buf, size_t offset, size_t length) {
    streamDump();
    uint32_t start_time = millis();
    sendCommand(GF("get_image"), '=', offset, ',', length, ',', GF("RAW"));
    while (_stream->available() < 3 && millis() - start_time < 5000L);
    if (!_stream->available()) {
        DBG_GLX("No response!");
        return 0;
    }
    // throw away the first two returned bytes
    for (uint8_t i = 2; i; i--) {
        _stream->read();
        // uint8_t b      = _stream->read();
        // char    zph[3] = {'\0', '\0', '\0'};
        // sprintf(zph, "%02x", b);
        // GEOLUX_DEBUG.print(zph);
    }
    // shorten the stream timeout so we're not waiting forever for a partial chunk
    uint32_t prev_timeout = _stream->getTimeout();
    _stream->setTimeout(15L);
    uint32_t bytes_read = _stream->readBytes(buf, length);
    // reset the stream timeout
    _stream->setTimeout(prev_timeout);
    if (bytes_read != length) {
        DBG_GLX(GF("Unexpected byte count: expected:"), length, GF("read:"),
                bytes_read);
    }
    return bytes_read;
}

uint32_t GeoluxCamera::transferImage(Stream* xferStream, int32_t image_size,
                                     int32_t chunk_size) {
    // get the full image size, if not given
    if (image_size == 0) { image_size = getImageSize(); }

    // bool got_start_tag        = false;
    // bool got_end_tag          = false;
    // bool got_matching_bytes   = false;
    // bool hit_zeros            = false;
    // bool all_chunks_succeeded = true;

    uint32_t max_command_response = 0;
    uint32_t max_char_spacing     = 0;

    // Read all the data up to # bytes!
    int32_t total_bytes_read    = 0;  // for the number of bytes read
    int32_t total_bytes_written = 0;  // for the number of bytes written
    int32_t start_data_byte =
        2;  // the first two bytes are header and don't belong in the file
    int32_t extra_read_buff =
        12;  // extra chars to read to ensure we get the closing tag
    int32_t bytes_remaining  = image_size + start_data_byte + extra_read_buff;
    int32_t chunk_number     = 0;
    int32_t start_next_chunk = 0;
    // int32_t chunks_needed    = ceil(image_size / chunk_size);
    ;
    uint8_t prev_bytes[4] = {0, 0, 0, 0};
    bool    eof           = false;

    uint32_t start_xfer_millis = millis();

    while (!eof && millis() - start_xfer_millis < 120000L) {
        int32_t bytesToRead =
            min(chunk_size,
                static_cast<int32_t>(max(bytes_remaining, static_cast<int32_t>(1))));
        int32_t bytes_read    = 0;
        int32_t bytes_written = 0;

        uint32_t start_command_millis = millis();
        sendCommand(GF("get_image"), '=', start_next_chunk, ',', bytesToRead, ',',
                    GF("RAW"));
        while (!_stream->available() &&
               millis() - start_command_millis < 5000L);  // wait for any characters
        if (!_stream->available()) {
            DBG_GLX("\nNo response!");
            continue;
        }
        max_command_response =
            max(max_command_response,
                static_cast<uint32_t>(millis() - start_command_millis));
#ifdef GEOLUX_DEBUG
        // print something to show we're not frozen
        GEOLUX_DEBUG.print('.');
#endif

        for (int32_t i = 0; i < bytesToRead + start_data_byte; i++) {
            uint32_t start_avail_time = millis();
            while (!_stream->available() &&
                   millis() - start_avail_time < 10);  // wait for the next character
            if (!_stream->available()) {
                DBG_GLX("\nNo more characters available!");
                break;
            }
            max_char_spacing = max(max_char_spacing,
                                   static_cast<uint32_t>(millis() - start_avail_time));
            uint8_t b        = _stream->read();
            bytes_read++;
            total_bytes_read++;

            if (total_bytes_written >= image_size && b == 0) {
                if (!eof) { DBG_GLX("\n --Got 0, available data exceeded--\n"); }
                eof = true;
                // hit_zeros = true;
            }
            if (i >= start_data_byte && !eof) {
                xferStream->write(b);
                bytes_written++;
                total_bytes_written++;
            }

            if (total_bytes_read == start_data_byte + 1) {
                DBG_GLX("\n --Start JPG--");
            }
#ifdef GEOLUX_DEBUG
            if ((total_bytes_read < 16 || total_bytes_written >= image_size - 16) &&
                (!eof)) {
                // print zero padded hex of the first and last 16 characters
                char zph[3] = {'\0', '\0', '\0'};
                sprintf(zph, "%02x", b);
                GEOLUX_DEBUG.print(zph);
            }
            if (total_bytes_read == 16) { GEOLUX_DEBUG.print(GFP("...")); }
#endif
            if (millis() - start_xfer_millis > 120000L) {
                DBG_GLX("\n ----Timed out!----\n");
                total_bytes_written = bytes_remaining;
            }

            uint8_t j = total_bytes_read % 4;
            uint8_t k = total_bytes_read % 4 - 1;
            if (k == static_cast<uint8_t>(-1)) { k = 3; }
            prev_bytes[j] = b;
            if ((b == 0xD9) && ((char)prev_bytes[k] == (char)0xFF)) {
                eof = 1;
                // got_end_tag = true;
                DBG_GLX("\n --Got FFD9 EoF tag--\n");
            }
            if ((b == 0xD8) && ((char)prev_bytes[k] == (char)0xFF)) {
                eof = 0;
                // got_start_tag = true;
                DBG_GLX("\n --Got FFD8 start tag--");
            }
        }
        bytes_remaining -= min(bytes_read, bytesToRead);
        start_next_chunk += min(bytes_read, bytesToRead);
        chunk_number++;

        // if (bytes_read == 0) { break; }
        if (eof) { break; }
        if (bytes_read - start_data_byte != bytesToRead ||
            bytes_written != bytesToRead) {
            DBG_GLX(GF("Unexpected byte count: expected:"), bytesToRead, GF("read:"),
                    bytes_read, GF("written:"), bytes_written);
            // all_chunks_succeeded = false;
        }
    }

#if defined GEOLUX_DEBUG
    uint32_t transfer_time = millis() - start_xfer_millis;
#endif

    DBG_GLX(GF("Used"), chunk_number, GF("chunks to read"), total_bytes_read,
            GF("bytes in"), chunk_size, GF("bytes chunks."));
    DBG_GLX(GF("Wrote"), total_bytes_written, GF("of expected"), image_size,
            GF("bytes to the SD card - a difference of"),
            abs(total_bytes_written - image_size), GF("bytes"));
    DBG_GLX(GF("Total transfer time was"), transfer_time, GF("ms"));
    DBG_GLX(GF("The maximum response time after a request was"), max_command_response,
            GF("and the maximum spacing between characters was"), max_char_spacing);


    return static_cast<uint32_t>(total_bytes_written);
}

uint32_t GeoluxCamera::transferImage(Stream& xferStream, int32_t image_size,
                                     int32_t chunk_size) {
    return transferImage(&xferStream, image_size, chunk_size);
}

bool GeoluxCamera::restart() {
    sendCommand(GF("reset"));
    bool resp = waitResponse() == 1;
    if (resp) {
        waitResponse(10000L,
                     GF("Geolux HydroCAM"));  // wait for a print out after restart
        streamFind('\n');                     // skip to the end of the line
    }
    return resp;
}

void GeoluxCamera::printCameraInfo(Stream* outStream) {
    uint32_t start_time = millis();
    sendCommand(GF("get_info"));
    // wait for response
    while (!_stream->available() && millis() - start_time < 5000L);
    while (_stream->available()) {
        outStream->println(_stream->readStringUntil('\n'));
        delay(2);
    }
}

void GeoluxCamera::printCameraInfo(Stream& outStream) {
    printCameraInfo(&outStream);
}

String GeoluxCamera::getDeviceType() {
    return getCameraInfoString("#device_type:");
}

String GeoluxCamera::getCameraFirmware() {
    return getCameraInfoString("#firmware:");
}

uint32_t GeoluxCamera::getCameraSerialNumber() {
    uint32_t serial_number = getCameraInfoInt("#serial_id:");
    if (serial_number != static_cast<uint32_t>(-1)) { return serial_number; }
    return 0;
}

bool GeoluxCamera::runAutofocus() {
    sendCommand(GF("run_autofocus"));
    return waitResponse() == 1;
}

bool GeoluxCamera::setResolution(const char* resolution) {
    sendCommand(GF("set_resolution"), '=', resolution);
    return waitResponse() == 1;
}

String GeoluxCamera::getResolution() {
    return getCameraInfoString("#resolution:");
}

bool GeoluxCamera::setQuality(uint8_t compression) {
    sendCommand(GF("set_quality"), '=', compression);
    return waitResponse() == 1;
}

int8_t GeoluxCamera::getQuality() {
    return static_cast<int8_t>(getCameraInfoInt("#quality:"));
}

bool GeoluxCamera::setJPEGMaximumSize(uint16_t size) {
    sendCommand(GF("set_jpeg_maximum_size"), '=', size);
    return waitResponse() == 1;
}

uint32_t GeoluxCamera::getJPEGMaximumSize() {
    return getCameraInfoInt("#jpeg_maximum_size:");
}

bool GeoluxCamera::setNightMode(geolux_night_mode mode) {
    switch (mode) {
        case DAY: {
            sendCommand(GF("set_quality"), '=', GF("day"));
            break;
        }
        case NIGHT: {
            sendCommand(GF("set_quality"), '=', GF("night"));
            break;
        }
        case AUTO:
        default: {
            sendCommand(GF("set_quality"), '=', GF("auto"));
            break;
        }
    }
    return waitResponse() == 1;
}

bool GeoluxCamera::setNightMode(const char* mode) {
    sendCommand(GF("set_resolution"), '=', mode);
    return waitResponse() == 1;
}

String GeoluxCamera::getNightMode() {
    return getCameraInfoString("#night_mode:");
}

bool GeoluxCamera::setIRLEDMode(geolux_ir_mode mode) {
    switch (mode) {
        case IR_ON: {
            sendCommand(GF("set_ir_led_mode"), '=', GF("on"));
            break;
        }
        case IR_OFF: {
            sendCommand(GF("set_ir_led_mode"), '=', GF("off"));
            break;
        }
        case IR_AUTO:
        default: {
            sendCommand(GF("set_ir_led_mode"), '=', GF("auto"));
            break;
        }
    }
    return waitResponse() == 1;
}

bool GeoluxCamera::setIRLEDMode(const char* mode) {
    sendCommand(GF("set_resolution"), '=', mode);
    return waitResponse() == 1;
}

String GeoluxCamera::getIRLEDMode() {
    return getCameraInfoString("#ir_led_mode:");
}

bool GeoluxCamera::getIRFilterStatus() {
    return getCameraInfoString("#ir_filter:") == "night";
}

bool GeoluxCamera::setAutofocusPoint(int8_t x, int8_t y) {
    sendCommand(GF("set_autofocus_point"), '=', x, ',', y);
    return waitResponse() == 1;
}

int8_t GeoluxCamera::getAutofocusX() {
    return static_cast<int8_t>(getCameraInfoInt("#autofocus_point:", ','));
}

int8_t GeoluxCamera::getAutofocusY() {
    return static_cast<int8_t>(getCameraInfoInt("#autofocus_point:", '\r', 1, ","));
}

bool GeoluxCamera::setAutoexposureRegion(int8_t x, int8_t y, int8_t width,
                                         int8_t height) {
    sendCommand(GF("set_autoexposure_region"), '=', x, ',', y, ',', width, ',', height);
    return waitResponse() == 1;
}

int8_t GeoluxCamera::getAutoexposureX() {
    return static_cast<int8_t>(getCameraInfoInt("#autoexposure_region:", ','));
}

int8_t GeoluxCamera::getAutoexposureY() {
    return static_cast<int8_t>(getCameraInfoInt("#autoexposure_region:", ',', 1, ","));
}

int8_t GeoluxCamera::getAutoexposureWidth() {
    return static_cast<int8_t>(getCameraInfoInt("#autoexposure_region:", ',', 2, ","));
}

int8_t GeoluxCamera::getAutoexposureHeight() {
    return static_cast<int8_t>(getCameraInfoInt("#autoexposure_region:", '\r', 3, ","));
}

uint32_t GeoluxCamera::getExposureTime() {
    return getCameraInfoInt("#exposure:");
}

uint32_t GeoluxCamera::getImageBrightness() {
    return getCameraInfoInt("#image_brightness:");
}

bool GeoluxCamera::setWhiteBalanceOffset(int8_t red, int8_t green, int8_t blue) {
    sendCommand(GF("set_wb_offset"), '=', red, ',', green, ',', blue);
    return waitResponse() == 1;
}

int8_t GeoluxCamera::getWhiteBalanceOffsetRed() {
    return static_cast<int8_t>(getCameraInfoInt("#wb_offset:"), ',');
}

int8_t GeoluxCamera::getWhiteBalanceOffsetGreen() {
    return static_cast<int8_t>(getCameraInfoInt("#wb_offset:", ',', 1, ","));
}

int8_t GeoluxCamera::getWhiteBalanceOffsetBlue() {
    return static_cast<int8_t>(getCameraInfoInt("#wb_offset:", '\r', 2, ","));
}

bool GeoluxCamera::setColorCorrectionMode(int8_t mode) {
    sendCommand(GF("set_color_correction_mod"), '=', mode);
    return waitResponse() == 1;
}

bool GeoluxCamera::getColorCorrectionMode() {
    return getCameraInfoString("#color_correction_mode:") == "on";
}

bool GeoluxCamera::setAutoSnapshotInterval(uint32_t mode) {
    sendCommand(GF("set_auto_snapshot_interval"), '=', mode);
    return waitResponse() == 1;
}

uint32_t GeoluxCamera::getAutoSnapshotInterval() {
    String snapInterval = getCameraInfoString("#auto_snapshot_interval:");
    if (snapInterval == "off") return 0;
    return snapInterval.toInt();
}

bool GeoluxCamera::moveFocus(int8_t offset) {
    sendCommand(GF("move_focus"), '=', offset);
    return waitResponse() == 1;
}

int16_t GeoluxCamera::getFocusPosition() {
    return static_cast<int16_t>(getCameraInfoInt("#focus_position:"));
}

bool GeoluxCamera::moveZoom(int8_t offset) {
    sendCommand(GF("move_zoom"), '=', offset);
    return waitResponse() == 1;
}

int8_t GeoluxCamera::getZoomPosition() {
    return static_cast<int8_t>(getCameraInfoInt("#zoom_position:"));
}

bool GeoluxCamera::sleep(uint32_t sleepTimeout) {
    sendCommand(GF("sleep"), '=', sleepTimeout);
    return waitResponse() == 1;
}

uint32_t GeoluxCamera::waitForReady(uint32_t initial_delay, uint32_t timeout) {
    geolux_status camera_status = geolux_status::NO_RESPONSE;
    uint32_t      start_millis = millis();
    delay(initial_delay);
    while (camera_status != geolux_status::OK && camera_status != geolux_status::NONE &&
           millis() - start_millis < timeout) {
        camera_status = getStatus();
        // delay to avoid pounding the camera too hard
        if (camera_status != geolux_status::OK &&
            camera_status != geolux_status::NONE) {
            delay(100);
        }
    }
    if (camera_status == GeoluxCamera::OK || camera_status == GeoluxCamera::NONE) {
        return millis() - start_millis;
    } else {
        return 0;
    }
}

int8_t GeoluxCamera::waitResponse(uint32_t timeout_ms, String& data, GsmConstStr r1,
                                  GsmConstStr r2, GsmConstStr r3, GsmConstStr r4)

{
    data.reserve(32);
    uint8_t  index       = 0;
    uint32_t startMillis = millis();
    do {
        while (_stream->available() > 0) {
            int8_t a = _stream->read();
            if (a <= 0) continue;  // Skip 0x00 bytes, just in case
            data += static_cast<char>(a);
            if (r1 && data.endsWith(r1)) {
                index = 1;
                goto finish;
            } else if (r2 && data.endsWith(r2)) {
                index = 2;
                goto finish;
            } else if (r3 && data.endsWith(r3)) {
                index = 3;
                goto finish;
            } else if (r4 && data.endsWith(r4)) {
                index = 4;
                goto finish;
            }
#if defined GEOLUX_DEBUG
            else if (data.endsWith(GF("Geolux HydroCAM"))) {
                data = "";
                DBG_GLX("### Unexpected module reset!");
                init();
                return true;
            }
#endif
        }
    } while (millis() - startMillis < timeout_ms);
finish:
    if (!index) {
        data.trim();
        if (data.length()) {}
        data = "";
    } else {
    }
    return index;
}

int8_t GeoluxCamera::waitResponse(uint32_t timeout_ms, GsmConstStr r1, GsmConstStr r2,
                                  GsmConstStr r3, GsmConstStr r4) {
    String data;
    return waitResponse(timeout_ms, data, r1, r2, r3, r4);
}

int8_t GeoluxCamera::waitResponse(GsmConstStr r1, GsmConstStr r2, GsmConstStr r3,
                                  GsmConstStr r4) {
    return waitResponse(5000L, r1, r2, r3, r4);
}

String GeoluxCamera::getCameraInfoString(const char* searchStartTag, char searchEndTag,
                                         int8_t      numberSkips,
                                         const char* searchSkipTag) {
    // send the get_info command
    uint32_t start_time = millis();
    sendCommand(GF("get_info"));
    // wait for response
    while (!_stream->available() && millis() - start_time < 5000L);

    // find the start string
    if (!_stream->find(const_cast<char*>(searchStartTag), strlen(searchStartTag))) {
        return "";
    }
    // after we've found the first string, shorten the timeout
    uint32_t prev_timeout = _stream->getTimeout();
    _stream->setTimeout(15L);
    // skip as many times as requested
    for (uint8_t skips = numberSkips; skips; skips--) {
        _stream->find(const_cast<char*>(searchSkipTag), strlen(searchSkipTag));
    }
    String resp = _stream->readStringUntil(searchEndTag);
    // read out and dump the rest of the lines
    while (_stream->find('#')) { _stream->readStringUntil('\n'); }
    // reset the stream timeout
    _stream->setTimeout(prev_timeout);
    return resp;
}
long GeoluxCamera::getCameraInfoInt(const char* searchStartTag, char searchEndTag,
                                    int8_t numberSkips, const char* searchSkipTag) {
    // send the get_info command
    uint32_t start_time = millis();
    sendCommand(GF("get_info"));
    // wait for response
    while (!_stream->available() && millis() - start_time < 5000L);

    uint32_t resp = -1;
    // find the start string
    if (!_stream->find(const_cast<char*>(searchStartTag), strlen(searchStartTag))) {
        return resp;
    }
    // after we've found the first string, shorten the timeout
    uint32_t prev_timeout = _stream->getTimeout();
    _stream->setTimeout(15L);
    // skip as many times as requested
    for (uint8_t skips = numberSkips; skips; skips--) {
        _stream->find(const_cast<char*>(searchSkipTag), strlen(searchSkipTag));
    }
    // read the character into a buffer
    char   buf[11];
    size_t bytes_read = _stream->readBytesUntil(searchEndTag, buf,
                                                static_cast<size_t>(11));
    // if we read 11 or more bytes, it's an overflow
    if (bytes_read && bytes_read < 11) {
        buf[bytes_read] = '\0';
        resp            = atol(buf);
    }
    // read out and dump the rest of the lines
    while (_stream->find('#')) { _stream->readStringUntil('\n'); }
    // reset the stream timeout
    _stream->setTimeout(prev_timeout);
    return resp;
}
