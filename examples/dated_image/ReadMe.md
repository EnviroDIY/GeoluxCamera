# Dated Image Example<!--!{#example_dated_image}-->

This example asks the camera to take a picture and then retrieves the data and saves it to an SD card using an attached RV-8803 RTC to name and date the files with the current date/time.
This example also write some metadata about the image process to a metadata csv file.

> [!NOTE]
> This example does **NOT** set the time on the RTC, it assumes the RTC time has been correctly set in advance.
