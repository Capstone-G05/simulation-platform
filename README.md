# Simulation Platform

> Unifying repo for depolying all Raspberry Pi services.

## Setup

**TODO:** hotspot setup 

**TODO:** ssh-keygen configuration


```shell
git submodule init
git submodule update
sudo chmod +x ./build.sh
```

## Deploy

```shell
git pull && git submodule foreach git pull
sudi ./build.sh -h 10.42.0.1 -p 8000
```

## Additional Commands

### Docker

```shell
docker compose up --build -d     # start services
docker compose down              # stop services
docker ps                        # view active services
docker logs <service>            # check service logs
docker exec -it redis redis-cli  # connect to redis-cli
```
