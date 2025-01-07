# Geolux camera library for Arduino<!--!{#mainpage}-->

An Arduino library for communication with the [Geolux HydroCam](https://www.geolux-radars.com/hydrocam).

[//]: # ( @tableofcontents )

[//]: # ( @m_footernavigation )

[//]: # ( Start GitHub Only )

- [Geolux camera library for Arduino](#geolux-camera-library-for-arduino)
  - [Geolux HydroCam](#geolux-hydrocam)
  - [Contributing](#contributing)
  - [License](#license)
  - [Acknowledgments](#acknowledgments)

[//]: # ( End GitHub Only )

## Geolux HydroCam

This uses the RS232 interface to communicate with and control the [Geolux HydroCam](https://www.geolux-radars.com/hydrocam).
The RS232 connection is on the green and yellow pins of the HydroCam.
An RS232-to-TTL adapter is needed between the HydroCam and an Arduino processor.

The camera requires a 9-27V DC power supply.
The specification state that the current consumption is 75 mA (Typical), 150 mA (Maximal).
I have not measured consumption to verify.
The camera boots very quickly after being powered on - generally a few hundred milliseconds or less.
After booting, the camera takes a few seconds to be ready to take a picture.
Changing the image settings can also take a few seconds.
Running autofocus takes almost 30s.

The camera supports multiple resolutions between 160x120 and 2592x1944 (5 Megapixel).
Larger images take longer to snap.
Images from the camera *must* be offloaded using RS232 at 115200 baud/8N1.
It takes almost a minute to transfer a 5mpx image from the camera to an SD card using a 120MHz SAMD51 processor with an SPI SD card.
Smaller images take much less time to transfer.

NOTE: The commands for changing Modbus, FTP, and ethernet connection parameters are **_not_** implemented in this library.

## Contributing<!--!{#mainpage_contributing}-->

Open an [issue](https://github.com/EnviroDIY/ModularSensors/issues) to suggest and discuss potential changes/additions.

## License<!--!{#mainpage_license}-->

Software sketches and code are released under the BSD 3-Clause License -- See [LICENSE.md](https://github.com/EnviroDIY/GeoluxCamera/blob/master/LICENSE.md) file for details.

Documentation is licensed as [Creative Commons Attribution-ShareAlike 4.0](https://creativecommons.org/licenses/by-sa/4.0/) (CC-BY-SA) copyright.

## Acknowledgments<!--!{#mainpage_acknowledgments}-->

[EnviroDIY](http://envirodiy.org/)™ is presented by the Stroud Water Research Center, with contributions from a community of enthusiasts sharing do-it-yourself ideas for environmental science and monitoring.

[Sara Damiano](https://github.com/SRGDamia1) is the primary developer of the Geolux HydroCam library, with input from [other contributors](https://github.com/EnviroDIY/ModularSensors/graphs/contributors).

This project has benefited from the support from the following funders:

- USGS Next Generation Water Observing System Rapid Deployment
- Stroud Water Research Center endowment
