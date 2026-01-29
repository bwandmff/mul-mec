# MEC (Multi-access Edge Computing) System

A high-performance multi-sensor fusion system for edge computing applications, featuring video and radar data integration with real-time processing capabilities.

## Features

- **Asynchronous Processing**: Producer-consumer architecture with message queues for efficient multi-threaded operation
- **Multi-Sensor Fusion**: Integration of video and radar sensor data for comprehensive target tracking
- **Real-time Performance Monitoring**: FPS, latency, and memory usage tracking
- **Configurable Parameters**: Runtime configuration via config files
- **Signal Handling**: Graceful shutdown and configuration reload (SIGHUP)
- **Custom Memory Management**: Leak tracking and optimized allocation
- **Comprehensive Logging**: Multi-level logging with timestamps
- **V2X Communication**: Standardized message encoding for vehicle-to-everything communication
- **Unix Socket Monitor**: Real-time system monitoring interface

## Architecture

The system consists of several key components:

- **Video Processor**: Handles video stream processing and object detection
- **Radar Processor**: Manages radar data input and preprocessing
- **Fusion Engine**: Integrates data from multiple sensors to create unified target tracks
- **Simulator**: Playback functionality for testing scenarios
- **Monitor Service**: Unix socket-based monitoring and control

## Building

To build the system:

```bash
mkdir build
cd build
gcc -I../include -I../src/common -Wall -Wextra -O2 -g -pthread -D_DEFAULT_SOURCE \
    ../src/common/*.c ../src/video/*.c ../src/radar/*.c ../src/fusion/*.c \
    ../src/main.c -o mec_system -lm
```

## Usage

Run in simulation mode:
```bash
./mec_system --sim
```

Run with custom configuration:
```bash
./mec_system -c /path/to/config.conf
```

## Requirements

- Linux system with POSIX threading support
- GCC compiler
- Math library (libm)
- Threads library (pthread)

## License

MIT License