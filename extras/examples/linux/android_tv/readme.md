# Android TV integration

This example shows how to use the Android TV integration with Supla.

# Instalation

## sd4linux

Follow the standard instructions for sd4linux (supla-device for Linux) installation
from (here)[https://github.com/SUPLA/supla-device/tree/main/extras/examples/linux].

## adb setup

`adb` allows to connect to the Android TV device and it will be used by our script
to fetch data from TV.

For Debian based distributions, you can install it with

    sudo apt install adb

Next, get IP address of your Android TV (you can read it from TV's settings or you
or from router). Here we assume that it is 192.168.0.10. Make sure that you use static IP
allocation on DHCP server for your TV. Otherwise this IP will change over time.

    adb connect 192.168.0.10

First execution of this command will fail, however on TV's screen you will see
prompt about connection attempt. Follow screen instructions and always allow connections
from your PC.

Then try again:

    adb connect 192.168.0.10

You can check if everything works with following command:

   adb shell dumpsys media_session

It should produce some output on the console.

## Folder for storing data and log

sd4linux requires a folder to store data. We will also store state of TV in a
file.
I assume that your user name is: supla_user with home home directory set to /home/supla_user.

First we'll create folder for storing data:

    mkdir -p /home/supla_user/android_tv

Make sure that it it is owned by supla_user and has write permissions:

    chown -R supla_user:supla_user /home/supla_user/android_tv
    chmod -R 755 /home/supla_user/android_tv

Now copy `media_state.sh` and `android_tv.yaml` to the folder. Those files are
in the same folder as this readme file.

## `media_state.sh` script and service

`media_state.sh` script is used to fetch data from TV and store it in the folder
`/home/supla_user/android_tv`.

Edit `media_state.sh` to change the following parameters:

    # Put your Android TV IP address
    android_tv_ip="192.168.0.10"

    # Adjust atv.state file name and location. Use absolute path without ~
    filename="/home/supla_user/android_tv/atv.state"

In case of any issues with the script, you can turn on logs in this file by changing comments here:

    logfile="/dev/null"
    # To enable logs, uncomment the following line and comment the line above.
    #logfile="/home/supla_user/android_tv/atv.log"

It will create detailed logs in the file `/home/supla_user/android_tv/atv.log`. Please
remember to disable it if script is working fine for you.

You can check if everything works by running script manually:

    ./media_state.sh

Nothing should be printed on the console. You can check if atv.state file exists
in the folder `/home/supla_user/android_tv` and if it contains data:

    cat /home/supla_user/android_tv/atv.state

There should be some number in this file.

Use CTRL+C to interupt the script.

Now, we'll configure the service to run `media_state.sh`.
We'll use `systemctl`.

Prepare service file: `/etc/systemd/system/android_tv_integration.service`:

    [Unit]
    Description=Android TV integration
    After=network-online.target

    [Service]
    User=supla_user
    ExecStart=/home/supla_user/android_tv/media_state.sh

    [Install]
    WantedBy=multi-user.target

Then call:

    sudo systemctl enable android_tv_integration.service
    sudo systemctl start android_tv_integration.service

And check if it works:

    sudo systemctl status android_tv_integration.service

## sd4linux configuration

Please follow instructions in [https://github.com/SUPLA/supla-device/tree/main/extras/examples/linux/README.md]
to install sd4linux as a service (it's at the end of that file).
Adjust it to use `android_tv.yaml` file from the folder `/home/supla_user/android_tv`.

Please also adjust `android_tv.yaml` and provide your server address, email, etc.


