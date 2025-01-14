/**
 * @file       GeoluxCamera.h
 * @author     Sara Damiano
 * @copyright  Stroud Water Research Center
 * @date       December 2024
 */

#ifndef SRC_GEOLUXCAMERA_H_
#define SRC_GEOLUXCAMERA_H_

#include <Arduino.h>

/**
 * @def DEFAULT_XFER_CHUNK_SIZE
 * @brief The default chunk size to request when asking for data from the camera.
 *
 * This amount of data is not stored in the processor's memory, this is just how many
 * characters to request at once from the camera. The characters themselves are
 * processed one at a time.
 */
#ifndef DEFAULT_XFER_CHUNK_SIZE
#define DEFAULT_XFER_CHUNK_SIZE 16384
#endif

/// The baud rate of RS232 communication on the HydroCAM; fixed at 115200
#define GEOLUX_CAMERA_RS232_BAUD 115200
/// The character bit configuration on the HydroCAM; fixed as 8N1
#define GEOLUX_CAMERA_RS232_CONFIG SERIAL_8N1

/**
 * @def GEOLUX_PROGMEM
 * @brief Helper define when dealing with multiple processors variants and memory
 * storage
 * @typedef GsmConstStr
 * @brief Helper type def to deal with program memory only available on AVR processors.
 * @def GFP
 * @brief Helper define to use to read from flash memory to a constant string for AVR
 * processors.  An empty define for non-AVR processors.
 * @def GF
 * @brief Helper define to use to write a constant string to flash memory for AVR
 * processors.  An empty define for non-AVR processors.
 */
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && !defined(__AVR_ATmega4809__)
#define GEOLUX_PROGMEM PROGMEM
typedef const __FlashStringHelper* GsmConstStr;
#define GFP(x) (reinterpret_cast<GsmConstStr>(x))
#define GF(x) F(x)
#else
#define GEOLUX_PROGMEM
typedef const char* GsmConstStr;
#define GFP(x) x
#define GF(x) x
#endif


#ifdef GEOLUX_DEBUG
namespace {
/**
 * @brief Template to help print mutiple printables in the same function call.
 *
 * @tparam T A printable type
 */
template <typename T>
/**
 * @brief  Function to print multiple chunks of printable information over the
 * debugging stream
 *
 * @param tail The last item to print
 */
static void DBG_GLX(T last) {
    GEOLUX_DEBUG.println(last);
}

/**
 * @brief Template to help print mutiple printables in the same function call.
 *
 * @tparam T A printable type
 */
template <typename T, typename... Args>
/**
 * @brief  Function to print multiple chunks of printable information over the
 * debugging stream
 *
 * @param head The first item to print
 * @param tail The last item to print
 */
static void DBG_GLX(T head, Args... tail) {
    GEOLUX_DEBUG.print(head);
    GEOLUX_DEBUG.print(' ');
    DBG_GLX(tail...);
}
}  // namespace
#else
/// Empty define for when debugging is not requested
#define DBG_GLX(...)
#endif

/// An "OK" response from the camera
static const char GEOLUX_OK[] GEOLUX_PROGMEM = "OK\r\n";
/// A "READY" response from the camera
static const char GEOLUX_READY[] GEOLUX_PROGMEM = "READY\r\n";
/// An "ERROR" response from the camera
static const char GEOLUX_ERROR[] GEOLUX_PROGMEM = "ERR\r\n";
/// A "BUSY" response from the camera
static const char GEOLUX_BUSY[] GEOLUX_PROGMEM = "BUSY\r\n";
/// A "NONE" response from the camera
static const char GEOLUX_NONE[] GEOLUX_PROGMEM = "NONE\r\n";

/**
 * @brief The class for the Geoluxh HydroCAM
 */
class GeoluxCamera {

 public:
    /// @brief The possible camera statuses
    typedef enum {
        OK = 1,  ///< Status is "OK" or "READY"
        ERROR,   ///< Status is in error
        BUSY,    ///< Status is "BUSY"
        NONE,    ///< Status is "NONE" or unknown
    } geolux_status;
    /// The possible camera IR filter (day/night) modes
    typedef enum {
        DAY = 0,  ///< In day mode, the IR filter in the camera is always active
        NIGHT,    ///< In night mode, the IR filter is disabled and the camera gives a
                  ///< black and white image
        AUTO,  ///< In auto mode, the camera measures the current level of environmental
               ///< illumination and automatically selects the optimal mode for the IR
               ///< filter.
    } geolux_night_mode;
    /// The possible camera IR LED modes
    typedef enum {
        IR_ON = 0,  ///< In on mode, the IR LEDs are on during the night, and off during
                    ///< the day
        IR_OFF,     ///< In off mode, the IR LEDs are always off
        IR_AUTO,    ///< In auto mode, the IR LEDs are active only during image
                    ///< acquisition, autofocus or manual zoom or focus operations.
    } geolux_ir_mode;

    /**
     * @brief Construct a new GeoluxCamera object - no action needed
     */
    GeoluxCamera();
    /**
     * @brief Construct a new GeoluxCamera object with stream attached.
     *
     * @param stream The stream instance the camera is attached to
     */
    GeoluxCamera(Stream* stream);
    /** @copydoc GeoluxCamera::GeoluxCamera(Stream* stream) */
    GeoluxCamera(Stream& stream);

    /**
     * @brief Destroy the GeoluxCamera object - no action needed
     */
    ~GeoluxCamera();

    /**
     * @brief Sets up the camera module
     */
    void begin();
    /**
     * @brief Sets up the camera module
     *
     * @param stream The stream instance the camera is attached to
     */
    void begin(Stream* stream);
    /** @copydoc GeoluxCamera::begin(Stream* stream) */
    void begin(Stream& stream);
    /** @copydoc GeoluxCamera::begin(Stream* stream) */
    void begin(HardwareSerial* stream);
    /** @copydoc GeoluxCamera::begin(Stream* stream) */
    void begin(HardwareSerial& stream);

    /**
     * @brief This command requests that the camera starts taking the next snapshot.
     *
     * Immediately after the command is received, the camera will return the response
     * with the current status, which can be OK, ERR or BUSY. If the response is OK,
     * this means that the camera is now in the process of taking the snapshot and
     * compressing it to JPEG. If the response is BUSY, this means that a previous
     * \#take_snapshot, \#move_zoom, or \#move_focus command has not yet been completed,
     * or the camera is waiting for auto exposure process to determine the best possible
     * exposure for current environment conditions. The ERR response indicates a general
     * error, and the \#take_snapshot command should be sent again to retry the
     * operation. After sending the \#take_snapshot command, the \#get_status command
     * should be periodically sent to check when the snapshot is ready for download.
     *
     * @return The status of the latest snapshot
     */
    geolux_status takeSnapshot();

    /**
     * @brief Get the camera status
     *
     * @return The current camera status
     */
    geolux_status getStatus();

    /**
     * @brief Get the size of any available image
     *
     * @return The size of the image, in bytes, or 0 if none is available
     */
    int32_t getImageSize();

    /**
     * @brief This command requests the camera to send the image data chunk.
     *
     * The image is sent in JPEG format, and can be sent only after the \#get_status
     * command returns the status READY. Three parameters need to be supplied to the
     * camera. The OFFSET parameter is the starting offset of the chunk in JPEG image
     * data. The LENGTH parameter is the requested length in bytes of the data chunk to
     * be sent while the FORMAT parameter specifies which format the camera should use
     * to transmit the data chunk, and it can be any of the following: RAW, HEX or
     * BASE64. The image data is returned immediately without any trailing `<CR><LF>`
     * pairs. If the format is RAW, then the chunk will not be encoded and raw binary
     * data will be returned. If the format is set to BASE64, the chunk will be BASE64
     * encoded, and if the format is HEX then each byte in the data chunk will be
     * encoded as two ASCII characters representing the HEX code of the byte. If OFFSET
     * and LENGTH parameters are set so that a part of the chunk (or the whole chunk) is
     * beyond the end of the image data, the camera will return the requested number of
     * bytes, but bytes beyond the end of image will be sent as zeroes
     *
     * @note This command always requests "RAW" data. The "HEX" and "BASE64" requests
     * don't work properly on the camera - it is a camera problem not a fault of this
     * library.
     *
     * @warning This function will automatically trim off the first two bytes junk
     * returned by the camera. Every chunk is returned with two bytes of junk at the
     * beginning and the total data returned is two bytes longer than requested to
     * account for it. This is **NOT** documented anywhere in the camera's manual.
     *
     * @warning This function does **NOT** trim any trailing zeros from the request.
     * Don't request more data than available.
     *
     * @warning The offset parameter does **NOT** work as I expected in my testing. The
     * camera appears to return data starting from wherever it left off after the last
     * request. You cannot re-request already sent chunks.
     *
     * @param buf A buffer to store the image data in. The buffer should be **at least**
     * 256 bytes; >= 512 is preferred.
     * @param offset The offset of the chunk
     * @param length The length of data to receive
     * @return The number of bytes returned
     */
    uint32_t getImageChunk(uint8_t* buf, size_t offset, size_t length);


    /**
     * @brief Transfer the image data from the camera stream to a secondary stream (like
     * the print input of an SD card).
     *
     * @param xferStream The stream to transfer data to
     * @param image_size The size of image data to transfer. If not specified, the
     * getImageSize() function is used to query to size from the camera. If the wrong
     * image size is given, the resulting file will not be usable.
     * @param chunk_size The size of chunks to use while talking to the camera; optional
     * with a default value of #DEFAULT_XFER_CHUNK_SIZE.
     * @return
     */
    uint32_t transferImage(Stream* xferStream, int32_t image_size = 0,
                           int32_t chunk_size = DEFAULT_XFER_CHUNK_SIZE);

    /**
     * @copydoc GeoluxCamera::transferImage(Stream* xferStream, int32_t image_size,
     * int32_t chunk_size)
     */
    uint32_t transferImage(Stream& xferStream, int32_t image_size = 0,
                           int32_t chunk_size = DEFAULT_XFER_CHUNK_SIZE);

    /**
     * @brief Restart the module
     *
     * @return True if the module was successfully restarted, otherwise false
     */
    bool restart();

    /**
     * @brief Prints information about the camera the the input stream.
     *
     * @param outStream A stream to print the information to
     */
    void printCameraInfo(Stream* outStream);
    /** @copydoc GeoluxCamera::printCameraInfo(Stream* stream) */
    void printCameraInfo(Stream& outStream);
    /**
     * @brief Get the camera's device type.
     *
     * @return The device type
     */
    String getDeviceType();
    /**
     * @brief Get the camera firmware information
     *
     * @return The firmware version as major.minor.patch
     */
    String getCameraFirmware();
    /**
     * @brief Get the camera serial number.
     *
     * @return The serial number
     */
    uint32_t getCameraSerialNumber();

    /**
     * @brief This command starts the process of moving the lens focus and searching for
     * sharpest image around the center point defined and stored with the
     * \#set_autofocus_point command.
     *
     * The returned status can be OK or BUSY, if the previous autofocus command is not
     * completed yet, zoom command is still active or there is other process that could
     * be disrupted by moving lens focus.
     *
     * @return True if the autofocus started successfully, otherwise false
     */
    bool runAutofocus();

    /**
     * @brief This command changes the image resolution.
     *
     * The RESOLUTION parameter must be one of the following:
     *     - "160x120" (4:3, 0.019 megapixel, Quarter-QVGA, QQVGA)
     *     - "320x240" (4:3, 0.077 megapixel, Quarter VGA , QVGA)
     *     - "640x480" (4:3, 0.307 megapixel, VGA)
     *     - "800x600" (4:3, 0.48 megapixel, Super VGA, SVGA)
     *     - "1024x768" (4:3, 0.79 megapixel, XGA)
     *     - "1280x960" (4:3, 1.23 megapixel, QuadVGA)
     *     - "1600x1200" (4:3, 1.92 megapixel, Ultra-XGA, UXGA)
     *     - "1920x1080" (16:9, 2.07 megapixel, 1080p, Full HD, FHD)
     *     - "2048x1536" (4:3, 3.15 megapixel, Quad-XGA, QXGA)
     *     - "2592x1944" (4:3, 5.04 megapixel, 1944p)
     *
     * @param resolution The desired resolution
     *
     * @warning If this command does not succeeded, check your input resolution. It must
     * match one of the strings from the above list exactly.
     *
     * @return True if the resolution was successfully changed, otherwise false
     */
    bool setResolution(const char* resolution);
    /**
     * @brief Get the camera resolution.
     *
     * @return The camera resolution as a string
     */
    String getResolution();

    /**
     * @brief This command changes the JPEG quality parameter, which can be in the range
     * between 1 and 100.
     *
     * 100 corresponds to the best image quality with the biggest file size, and lower
     * numbers will give stronger compression. The recommended setting is in the range
     * of 70-80. The returned STATUS can be OK or ERR, if the given parameter is
     * invalid.
     *
     * @param compression The JPEG quality - 0-100
     * @return True if the quality/compression was successfully changed, otherwise false
     */
    bool setQuality(uint8_t compression);
    /**
     * @brief Get the JPEG quality parameter value.
     *
     * @return The JPEG quality parameter value
     */
    int8_t getQuality();

    /**
     * @brief This command changes the JPEG maximum file size which is a decimal number
     * in kB which specifies the maximum JPEG file size generated after a snapshot is
     * taken.
     *
     * The camera will try to reduce the quality in several steps to get the file size
     * below the specified limit, and it will generate the minimum file possible if the
     * goal could not be reached. The value 0 means no limit in size is required (no
     * quality reduction is done over the one set in the Flash already). Other values
     * represent maximum JPEG file size requirement in kilobytes, but please note that
     * it may not be possible for HydroCam to satisfy this requirement, and in this
     * case, it will start decreasing image quality internally, starting from the
     * maximum possible quality, but without changing the quality parameter as stored in
     * the Flash memory. As soon as the resulting JPEG file is at or below the required
     * file size, the search will terminate and the file will be generated with the
     * quality reached. If no requirement was met, the resulting file size will be
     * whatever was accomplished in the last step.
     *
     * @param size The maximum JPEG image size in kb.
     * @return True if the maximum image size was successfully changed, otherwise false
     */
    bool setJPEGMaximumSize(uint16_t size);
    /**
     * @brief Get the JPEG maximum file size.
     *
     * @return The JPEG maximum file size
     */
    uint32_t getJPEGMaximumSize();

    /**
     * @brief Changes the camera mode according to the given MODE parameter which can be
     * either day, night or auto.
     *
     * In day mode, the IR filter in the camera is always active. In night mode, the IR
     * filter is disabled and the camera gives a black and white image. In auto mode,
     * the camera measures the current level of environmental illumination and
     * automatically selects the optimal mode for the IR filter.
     *
     * @param mode The mode from the GeoluxCamera::geolux_night_mode enum
     * @return True if the night mode was successfully changed, otherwise false
     */
    bool setNightMode(geolux_night_mode mode);

    /**
     * @brief Changes the camera mode according to the given MODE parameter which can be
     * either day, night or auto.
     *
     * In day mode, the IR filter in the camera is always active. In night mode, the IR
     * filter is disabled and the camera gives a black and white image. In auto mode,
     * the camera measures the current level of environmental illumination and
     * automatically selects the optimal mode for the IR filter.
     *
     * @param mode The mode as characters, must be one of "day", "night", or "auto".
     * @return True if the night mode was successfully changed, otherwise false
     */
    bool setNightMode(const char* mode);
    /**
     * @brief Get the current night mode setting.
     *
     * @return the current night mode setting
     */
    String getNightMode();

    /**
     * @brief Changes the camera’s IR LED mode according to the given parameter which
     * can be either off, on or auto.
     *
     * In off mode, the IR LEDs are always off. In on mode, the IR LEDs are on during
     * the night, and off during the day. In auto mode, the IR LEDs are active only
     * during image acquisition, autofocus or manual zoom or focus operations.
     *
     * @param mode The mode from the GeoluxCamera::geolux_ir_mode enum.
     * @return True if the IR LED mode was successfully changed, otherwise false
     */
    bool setIRLEDMode(geolux_ir_mode mode);


    /**
     * @brief Changes the camera’s IR LED mode according to the given parameter which
     * can be either off, on or auto.
     *
     * In off mode, the IR LEDs are always off. In on mode, the IR LEDs are on during
     * the night, and off during the day. In auto mode, the IR LEDs are active only
     * during image acquisition, autofocus or manual zoom or focus operations.
     *
     * @param mode The mode as characters, must be one of "day", "night", or "auto".
     * @return True if the IR LED mode was successfully changed, otherwise false
     */
    bool setIRLEDMode(const char* mode);
    /**
     * @brief Get the camera’s IR LED mode.
     *
     * @return the camera’s IR LED mode
     */
    String getIRLEDMode();
    /**
     * @brief Check if the IR filter is currently in place on the camera.
     *
     * @return True if the IR filter is currently on (in night mode), otherwise false
     * (in day mode)
     */
    bool getIRFilterStatus();

    /**
     * @brief Configures the point used for the autofocus operation.
     *
     * The x and y coordinates are specified as a percentage of the image size with
     * (0,0) being at the bottom left. The values are in the 0 to 100 range. Change of
     * autofocus coordinates does not apply until a new autofocus request is made using
     * the command \#run_autofocus. Please note that autofocus is also performed in the
     * background of the zoom operation, but this autofocus does not use these autofocus
     * point coordinates, as focus in that case is always in the middle of the image,
     * i.e. the autofocus point is (50,50).
     *
     * @param x The position of the focus on the x axis, in percent starting on the
     * left
     * @param y The position of the focus on the y axis, in percent starting on the
     * bottom
     * @return True if the autofocus point was successfully changed, otherwise false
     */
    bool setAutofocusPoint(int8_t x, int8_t y);
    /**
     * @brief Get the position of the focus on the x axis.
     *
     * @return The position of the focus on the x axis, in percent starting on the left.
     */
    int8_t getAutofocusX();
    /**
     * @brief Get the position of the focus on the y axis.
     *
     * @return The position of the focus on the y axis, in percent starting on the
     * bottom
     */
    int8_t getAutofocusY();

    /**
     * @brief Configures the area used to measure brightness for the auto-exposure
     * operation.
     *
     * The x and y coordinates of the center of the area, as well as the width and
     * height of the area are all specified as a percentage of the image size with (0,0)
     * being at the bottom left. The values are in the 0 to 100 range. Please note that
     * if there was a conflict in the values specified, while individual values were
     * still within valid range, the HydroCam will accept and store values specified,
     * but will prioritize the size of the region when calculating image brightness and
     * determine exposure. For example, if the coordinates of the center of the region
     * were x=20 and y=30, while the size of the area was determined by width=200 and
     * height=100, the camera would conclude it was not possible to comply with both
     * requirements and would consider the center to be at x=100 and y=50 when
     * calculating the brightness.
     *
     * @param x The left edge of the auto exposure measurement region, in percent
     * starting from the left edge of the image
     * @param y The bottom edge of the auto exposure measurement region, in percent
     * starting from the bottom edge of the image
     * @param width The width of the auto exposure measurement region, in percent
     * @param height The height of the auto exposure measurement region, in percent
     * @return True if the auto-exposure region was successfully changed, otherwise
     * false
     */
    bool setAutoexposureRegion(int8_t x, int8_t y, int8_t width, int8_t height);
    /**
     * @brief Get the left edge of the auto exposure measurement region.
     *
     * @return The left edge of the auto exposure measurement region, in percent
     * starting from the left edge of the image
     */
    int8_t getAutoexposureX();
    /**
     * @brief Get the bottom edge of the auto exposure measurement region.
     *
     * @return The bottom edge of the auto exposure measurement region, in percent
     * starting from the bottom edge of the image
     */
    int8_t getAutoexposureY();
    /**
     * @brief Get the width of the auto exposure measurement region.
     *
     * @return The width of the auto exposure measurement region, in percent
     */
    int8_t getAutoexposureWidth();
    /**
     * @brief Get the height of the auto exposure measurement region.
     *
     * @return The height of the auto exposure measurement region, in percent
     */
    int8_t getAutoexposureHeight();
    /**
     * @brief Get the current exposure time (shutter width).
     *
     * @note The exposure time **CANNOT** be manually set. Only auto exposure is
     * supported. You can set the region over which the auto exposure value is
     * determined.
     *
     * @warning I do not know the units of the return value!
     *
     * @return The current exposure time
     */
    uint32_t getExposureTime();
    /**
     * @brief Get the mean image brightness.
     *
     * @return The mean image brightness
     */
    uint32_t getImageBrightness();

    /**
     * @brief Configures the white balance offset parameters for the red, green, and
     * blue color components.
     *
     * The values are numbers in the range of 8 to 48. Each parameter is used for its
     * color respectively, internally divided by 10, and used as a multiplier for the
     * default sensor analog gain for that particular color.
     *
     * @param red The white balance offset in the red color channel.
     * @param green The white balance offset in the green color channel.
     * @param blue The white balance offset in the blue color channel.
     * @return True if the white balance offset was successfully changed, otherwise
     * false
     */
    bool setWhiteBalanceOffset(int8_t red, int8_t green, int8_t blue);
    /**
     * @brief Get the white balance offset in the red color channel
     *
     * @return The white balance offset in the red color channel
     */
    int8_t getWhiteBalanceOffsetRed();
    /**
     * @brief Get the white balance offset in the green color channel
     *
     * @return The white balance offset in the green color channel
     */
    int8_t getWhiteBalanceOffsetGreen();
    /**
     * @brief Get the white balance offset in the blue color channel
     *
     * @return The white balance offset in the blue color channel
     */
    int8_t getWhiteBalanceOffsetBlue();

    /**
     * @brief Sets the color correction mode. Valid values are integers between 0 and 3.
     *
     * The resulting effect of value 0 is that white balance correction is turned off,
     * and \#get_info command will report the color correction as off. Values 1 to 3
     * result in the white balance algorithm running with different set of parameters
     * defined for each mode. Please contact the Geolux support for more information
     * about different color correction modes.
     *
     * @param mode The integer color correction mode.
     * @return True if the color correction mode was successfully changed, otherwise
     * false
     */
    bool setColorCorrectionMode(int8_t mode);
    /**
     * @brief Check whether color correction is currently being applied
     *
     * @return True if color correction is being applied, otherwise false
     */
    bool getColorCorrectionMode();

    /**
     * @brief Sets the time interval in minutes for autonomous periodic snapshot
     * operation.
     *
     * A valid argument is the number of minutes between 0 and 65535, with 0 meaning
     * there is no autonomous snapshot operation. If the interval is set to some valid
     * non-zero value, the user should take care that a new snapshot is not requested
     * before the previous one was transferred. This command does not make much sense if
     * only UART is used transfer images as the transfer would not happen automatically,
     * but if FTP upload is enabled, the image will also be transferred to the FTP
     * server automatically.
     *
     * @note Please note that the camera needs to be powered on in order to take
     * periodic snapshots, as there is no battery-operated clock, and the time interval
     * is measured from the power up.
     *
     * @param interval The number of minutes between snapshots.
     * @return True if the auto snapshot inteval was successfully changed, otherwise
     * false
     */
    bool setAutoSnapshotInterval(uint32_t interval);
    /**
     * @brief Get the auto snapshot interval.
     *
     * @return The auto snapshot interval
     */
    uint32_t getAutoSnapshotInterval();

    /**
     * @brief This command forces the camera to move the focus of the lens for a given
     * number of steps.
     *
     * The OFFSET parameter can be either a positive or a negative integer number. The
     * focus movement is relative to the starting focus position. The returned STATUS
     * can be either OK or ERR, if the OFFSET is outside of the allowed range (-100 to
     * +100).
     *
     * @param offset The focus offset relative to the starting position.
     * @return True if the focus point was successfully changed, otherwise false
     */
    bool moveFocus(int8_t offset);
    /**
     * @brief Get the current focus position.
     *
     * @return The current focus position
     */
    int16_t getFocusPosition();

    /**
     * @brief This command forces the camera to change the lens zoom for a given number
     * of steps.
     *
     * The OFFSET parameter can be either a positive or a negative integer number. The
     * zoom movement is relative to the starting zoom position. The returned STATUS can
     * be either OK or ERR, if the OFFSET is outside of the allowed range (-100 to +100)
     *
     * @param offset The zoom offset relative to the starting zoom position
     * @return True if the zoom point was successfully changed, otherwise false
     */
    bool moveZoom(int8_t offset);
    /**
     * @brief Get the current zoom position.
     *
     * @return The current zoom position
     */
    int8_t getZoomPosition();

    /**
     * @brief Put the module to sleep, starting from the time the command is
     * issued and ending when sleep timer expires.
     *
     * @note There is no way to wake the camera up early! There is also no way to return
     * the sleep timeout after sending the sleep command.
     *
     * @param sleepTimeout The time for the module to sleep, in seconds.
     * @return True if the module accepted the timeout has will go to sleep, otherwise
     * false
     */
    bool sleep(uint32_t sleepTimeout);

    /*
     Utilities
     */


    /**
     * @brief Recursive variadic template to send commands
     *
     * @tparam Args
     * @param cmd The commands to send
     */
    template <typename... Args>
    inline void sendCommand(Args... cmd) {
        streamWrite("#", cmd..., "\r\n");
        _stream->flush();
    }

    /**
     * @brief **Blocking** delay until the camera status returns a status of "OK",
     * "READY" or "NONE."
     *
     * We accept "NONE" as ready because the camera sometimes returns "NONE" when it's
     * not currently doing anything.
     *
     * @param initial_delay The amount of time to wait before asking the camera for
     * status; optional with a default value of 0. The inital delay is useful to avoid
     * hammering the camera with status requests after starting an operation known to be
     * slow - like autofocus.
     * @param timeout The maximum number of milliseconds to wait; optional with a
     * default of 60,000 (1 minute)
     * @return The number of milliseconds waited, or 0 if the operation timed out
     */
    uint32_t waitForReady(uint32_t initial_delay = 0, uint32_t timeout = 60000L);

    /**
     * @brief Listen for responses to commands and handle URCs
     *
     * @param timeout_ms The time to wait for a response
     * @param data A string of data to fill in with response results
     * @param r1 The first output to test against, optional with a default value
     * of "OK"
     * @param r2 The second output to test against, optional with a default value
     * of "ERROR"
     * @param r3 The third output to test against, optional with a default value
     * of "BUSY"
     * @param r4 The fourth output to test against, optional with a default value
     * of "NONE"
     * @return *int8_t* the index of the response input
     */
    int8_t waitResponse(uint32_t timeout_ms, String& data,
                        GsmConstStr r1 = GFP(GEOLUX_OK),
                        GsmConstStr r2 = GFP(GEOLUX_ERROR),
                        GsmConstStr r3 = GFP(GEOLUX_BUSY),
                        GsmConstStr r4 = GFP(GEOLUX_NONE));

    /**
     * @brief Listen for responses to commands and handle URCs
     *
     * @param timeout_ms The time to wait for a response
     * @param r1 The first output to test against, optional with a default value
     * of "OK"
     * @param r2 The second output to test against, optional with a default value
     * of "ERROR"
     * @param r3 The third output to test against, optional with a default value
     * of NULL
     * @param r4 The fourth output to test against, optional with a default value
     * of NULL
     * @return *int8_t* the index of the response input
     */
    int8_t waitResponse(uint32_t timeout_ms, GsmConstStr r1 = GFP(GEOLUX_OK),
                        GsmConstStr r2 = GFP(GEOLUX_ERROR),
                        GsmConstStr r3 = GFP(GEOLUX_BUSY),
                        GsmConstStr r4 = GFP(GEOLUX_NONE));

    /**
     * @brief Listen for responses to commands and handle URCs; listening for 1
     * second.
     *
     * @param r1 The first output to test against, optional with a default value
     * of "OK"
     * @param r2 The second output to test against, optional with a default value
     * of "ERROR"
     * @param r3 The third output to test against, optional with a default value
     * of NULL
     * @param r4 The fourth output to test against, optional with a default value
     * of NULL
     * @return *int8_t* the index of the response input
     */
    int8_t waitResponse(GsmConstStr r1 = GFP(GEOLUX_OK),
                        GsmConstStr r2 = GFP(GEOLUX_ERROR),
                        GsmConstStr r3 = GFP(GEOLUX_BUSY),
                        GsmConstStr r4 = GFP(GEOLUX_NONE));

    /**
     * @brief Utility template for writing on a stream
     *
     * @tparam T A printable type
     */
    template <typename T>
    /**
     * @brief Function to print text over the camera stream
     *
     * @param last The last item to print
     */
    inline void streamWrite(T last) {
        _stream->print(last);
    }

    /**
     * @brief Utility template for writing on a stream
     *
     * @tparam T A printable type
     */
    template <typename T, typename... Args>
    /**
     * @brief  Function to print multiple chunks of printable information over the
     * camera stream
     *
     * @param head The first item to print
     * @param tail The last item to print
     */
    inline void streamWrite(T head, Args... tail) {
        _stream->print(head);
        streamWrite(tail...);
    }

    /**
     * @brief Read a throw away any characters left in the camera stream.
     */
    inline void streamDump() {
        if (!_stream->available()) { delay(25); }
        while (_stream->available()) {
            _stream->read();
            delay(1);
        }
    }

 protected:
    /**
     * @brief Find a target character within a stream.
     *
     * A pass through to the stream function.
     *
     * @param target The character to find
     * @return True if the target is found, false if the search times out
     */
    inline bool streamFind(char target) {
        return _stream->find(const_cast<char*>(&target), 1);
    }

    /**
     * @brief Get a string within the camera info output.
     *
     * @param searchStartTag The tag/characters preceding the desired info - this should
     * be the text string for the desired information.
     * @param searchEndTag The tag/characters proceeding the desired info - optional
     * with a default of '\\r'.
     * @param numberSkips The number of times to skip the skip character. This is used
     * for fields where multiple bits of information follow the same data tag. Optional
     * with a default of 0.
     * @param searchSkipTag The search tag to skip before returning a value. This is
     * used for fields where multiple values follow the same search tag separated by a
     * comma or other delimeter.  Optional with a default value of ','.
     * @return The string between the start and end tags.
     */
    String getCameraInfoString(const char* searchStartTag, char searchEndTag = '\r',
                               int8_t numberSkips = 0, const char* searchSkipTag = ",");
    /**
     * @brief Get integer within the camera info output.
     *
     * @param searchStartTag The tag/characters preceding the desired info - this should
     * be the text string for the desired information.
     * @param searchEndTag The tag/characters proceeding the desired info - optional
     * with a default of '\\r'.
     * @param numberSkips The number of times to skip the skip character. This is used
     * for fields where multiple bits of information follow the same data tag. Optional
     * with a default of 0.
     * @param searchSkipTag The search tag to skip before returning a value. This is
     * used for fields where multiple values follow the same search tag separated by a
     * comma or other delimeter.  Optional with a default value of ','.
     * @return The integer between the start and end tags.
     */
    long getCameraInfoInt(const char* searchStartTag, char searchEndTag = '\r',
                          int8_t numberSkips = 0, const char* searchSkipTag = ",");

    /**
     * @brief The stream instance (serial port) for communication with the Modbus slave
     * (usually over RS485)
     */
    Stream* _stream;
};

#endif  // SRC_GEOLUXCAMERA_H_
