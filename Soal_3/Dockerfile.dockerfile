FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    fuse \
    libfuse-dev \
    pkg-config \
    gcc \
    make \
    openssl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY antink.c .
RUN gcc -Wall antink.c `pkg-config fuse --cflags --libs` -o antink

RUN mkdir -p /it24_host /antink_mount /var/log

CMD ["/app/antink", "/antink_mount"]