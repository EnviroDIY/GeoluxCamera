# Read By Chunk Example<!--!{#example_read_by_chunk}-->

This example asks the camera to take a picture and then retrieves the data and saves it to an SD card using individual chunks that are transferred through the main processor's memory.

Reading in "small" chunks and transferring the as bulk memory reads as done by this program could be faster with an optimized SD card / SPI driver, but in practice is most likely slower and less reliable than the huge chunk 1-character-at-a-time reads used by the transferImage(...) function snapshot example.

> [!TIP]
> Reading in chunk sizes of less than ~512 bytes tends to cause the camera to stall.
