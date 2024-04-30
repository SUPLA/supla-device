# Readme
This docker image contains a `supla-device-4-linux` instance. \
There is a sample configuration file for this instance in the local `conf` directory. It should be adapted to your needs by configuring the necessary channels, connection to the Supla server and the name of the device that this instance will represent.\
The `data` directory should contain files from which channels with a File source type will read data sent to the Supla server.\
In the `auth` directory, the instance will create a file with the GUID and Auth required to identify the device on the Supla server and a log with the latest status as well as a file with data transferred from the Supla server.
## Build and run without `docker compose`
Build\
`docker build --rm -t supla/supla-device-4-linux:local .`\
Run\
`docker run --rm --name sd4l -v .:/supla-device --restart unless-stopped supla/supla-device-4-linux:local`

## Build and run with `docker compose`
`docker compose up --build -d`


