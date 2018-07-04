# P2P live streaming system

### Motive

Live streaming is one of the most indispensable technologies in our lives today.
Recently, the genre of Vtuber has been established, and because I like to watch its live streaming, even a little I wanted to create a core system that would alleviate the service.

### Overview

It is a live streaming system using a P2P method that reduces the intensive burden on the server,
It is intended for raw delivery in the WEBM format, which is compressed in VP9.

Specifically, by using P2P networks, we use the resources of peers to mitigate scalability problems.
In addition, we determine the compatibility between peers from the amount of cache and route them to ensure that the quality of service is maintained for the nodes.

### Characteristics

- Supports the delivery of the VP9-compressed WEBM format, which is often used in the near future.
- UDP communication.
- Structure a P2P network based on live streaming cache.

### Compile

- on the originsource side

```sh

cd src/originsource
make

```

- on the peer side

```sh

cd src/peer
make

```

### Usage

***To encode with VP9, it is necessary to install the encoding software such as FFmpeg.***


- How to start on the originsource side

```sh

>$ program <gateway-ip-address> <file name>

```

- Using FFmpeg

```sh

ffmpeg -threads 8 -i /dev/video0 -c:v libvpx -g 25 -cpu-used 8 <file-name>

```

- How to start on the peer side

```sh

>$ program < gateway-ip-address > <originsource-global-ip-address> <src-port> < file-name >

```



### Example
TODO



### License
Mit license.

### References
***

* [Encode vp9 by using FFmpeg](https://trac.ffmpeg.org/wiki/Encode/VP9)
* [Matroska Specifications](https://www.matroska.org/technical/specs/index.html)
