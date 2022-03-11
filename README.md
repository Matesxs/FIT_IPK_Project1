# Hardware Info HTTP Server

HTTP server that will send back informations about host based on accessed endpoint. \
**This project is created with unix systems, so it will not run on windows!** \
Minimal version of C++ is C++17 for this project.

## Getting Started

### Prerequisites

The things you need before installing the software.

* g++
* make

### Installation

A step by step guide that will tell you how to get the development environment up and running.

```
$ sudo apt install build-essential
$ make build
```

## Usage

### Basic usage

```
$ cd output
$ ./hinfosvc <port>
```

For example

```
$ cd output
$ ./hinfosvc 12345
```

This will start the HTTP server itself and now we can send requests to it. \
For example from curl:

```
curl http://localhost:12345/load
```

## Requests and example reponses

### hostname

```
request: GET http://servername:12345/hostname
response: merlin.fit.vutbr.cz
```

### cpu-name

```
request: GET http://servername:12345/cpu-name
response: Intel(R) Xeon(R) CPU E5-2640 0 @ 2.50GHz
```

### load

```
request: GET http://servername:12345/load
response: 65%
```
