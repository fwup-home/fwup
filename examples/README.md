# fwup-examples Buildroot

## System prep

If you're using Ubuntu, you may need to install some packages to make Buildroot
work. This should be sufficient:

    $ sudo apt-get install git g++ libncurses5-dev bc make unzip zip

## Building

All builds are done in directories that are separate from the source tree. This
allows multiple configurations to be tried simultaneously and keeps the source
tree clean.

Assuming that you've cloned this project already, create a build directory by
running `./create_build.sh <defconfig> <path to build directory>`. Here's an
example invocation:

    $ ./create_build.sh configs/fillmein_defconfig o/fillmein

Then build the firmware:

    $ cd o/fillmein
    $ make

It can take some time to download and build everything so you may need to be
patient. The build products can be found in the `images` directory.

## Good commandlines

$ cat images/vexpress.fw | ssh -p 10022 root@localhost 'fwup -a -U -d /dev/mmcblk0 -t upgrade && /sbin/reboot'
100%

