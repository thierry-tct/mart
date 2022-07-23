# Getting pre-compiled Mart

## Docker image
Use _Mart_ [docker](https://docs.docker.com) image, available on [Docker Hub](https://hub.docker.com/r/thierrytct/mart), to run.

### Get the image Ready
Pull the docker image or build the docker image locally
1. Pulling the image 
```
docker pull thierrytct/mart
```

2. Building the image locally
```
git clone --recursive https://github.com/thierry-tct/mart.git mart/src
docker build --tag thierrytct/mart mart/src
```

### Run Docker container
run docker container for demo
```
docker run --rm thierrytct/mart
```
or interactively
```
docker run --rm -it thierrytct/mart /bin/bash
```