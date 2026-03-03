FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    net-tools

WORKDIR /app

COPY client.c .

RUN gcc client.c -o client -lpthread

CMD ["tail", "-f", "/dev/null"]
