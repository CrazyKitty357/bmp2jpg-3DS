# bmp2jpg-3DS
a 3ds homebrew application for converting bitmaps to jpgs  
**New3DS systems are currently untested so milage may vary**

# Usage
1. pick the bmp you want to convert to a jpg on the bottom screen
2. wait till the application closes (this means it's done)
3. go to where you picked the bmp and you should see an extra .jpg file with the same name in that exact directory

# Building
- devkitpro-3ds
- make

1. download and setup [devkitpro](https://devkitpro.org/wiki/Getting_Started) (you want the 3ds-dev packages)
2. git clone this repo
3. run `make`
4. move the bmp2jpg.3dsx file to 3ds/ on your SD card or run `3ds-link -a <3ds IP Addr> bmp2jpg.3dsx` after you pressed y on the homebrew launcher while connected to the internet

# known bugs
- [ ] the screen being very dim  
    workaround: turn brightness to max
