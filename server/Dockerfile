FROM server_image

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    net-tools

WORKDIR /app

COPY server.c .

RUN gcc server.c -o server -lpthread

CMD ["tail", "-f", "/dev/null"]
