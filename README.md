# Simulation Platform

> Unifying repo for depolying all Raspberry Pi services.

## Setup
```shell
git submodule init
git submodule update
sudo chmod +x ./build.sh
```

## Deploy

```shell
./build.sh
docker compose up --build -d # start services
docker compose down          # stop services
```

