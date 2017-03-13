# ctxh264_pi

--- Rebuild on RPi 2/3 boards:

- clone repos:
git clone https://github.com/raspberrypi/firmware.git firmware 
git clone https://github.com/Gibbio/ctxh264_pi.git ctxh264_pi

- install prerequisite:
sudo apt-get update && sudo apt-get install libx11-dev libxfixes-dev libxext-dev

- build RPi dependencies:
cp -Rp firmware/hardfp/opt /
make -C /opt/vc/src/hello_pi/libs/ilclient/
make -C /opt/vc/src/hello_pi/libs/vgfont/

- build ctxh264 lib:
make -C ctxh264_pi/bcm_init/
make -C ctxh264_pi/H264_Pi_sample/

- install new libs:
cp H264_Pi_sample/ctxh264.so /opt/Citrix/ICAClient/lib/
cp bcm_init/bcm_init.so /usr/lib/

- lib jpeg turbo:
remove /opt/Citrix/ICAClient/lib/ctxjpeg_fb*.so
or rebuild ctxjpeg_fb with use_turbo = TRUE; (default)

- suggested config params:
/boot/config.txt:
gpu_mem=256
framebuffer_depth=32
framebuffer_ignore_alpha=1

/opt/Citrix/ICAClient/config/module.ini:
;H264Enabled=False (comment out)






Thanks Muhammad Dawood write this plugin.  
http://blogs.citrix.com/2014/03/17/raspberry-pi-xendesktop-pt-3-download-receiver-here/  
Source code from citrix platform optimization sdk

What modified?  
disable watermark  
add Makefile  
change egl_render to video_render

Must use with xorg which support hwcursor.

Download:  
https://github.com/luyi1888/ctxh264_pi/releases/tag/v0.1  

Known Issues:  
Audio may have lag.

Guide:  
http://www.martinrowan.co.uk/2015/08/citrix-receiver-h-264-hardware-acceleration-on-raspberry-pi-2/  
written by martin
