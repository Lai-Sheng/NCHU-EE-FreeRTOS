# Parking Lot Management System

A FreeRTOS-based embedded system simulating the operations of a parking lot, including car entry, parking, tolling, and exit processes. The system is designed with multitasking, resource synchronization, and modularity for reliability and scalability.
## Demo

You can watch the demo [here on YouTube](https://youtu.be/yUMBTbeqBns).

## Features

- **Multitasking with FreeRTOS**:
  - Independent tasks for entry management, tolling, exit processing, and server communication.
- **Resource Synchronization**:
  - Mutexes to ensure safe access to parking lot data.
  - Event groups to manage and monitor parking slot statuses.
- **Inter-Task Communication**:
  - Queues and message buffers for data transfer between tasks.
- **Randomized Simulations**:
  - Random intervals for car entry and parking slot selection.
- **Dual Display Modes**:
  - Basic mode and advanced mode with animated outputs for user interaction.

## System Architecture

1. **Tasks**:
   - `vEntry_Handler`: Handles car entry and assigns available parking slots.
   - `vTollingHandler`: Manages tolling operations and updates parking status.
   - `vExitHandler`: Processes car exit and clears parking slots.
   - `vServerTask`: Collects and displays exit data for external systems.
2. **Synchronization**:
   - Mutex locks to prevent race conditions on shared resources.
   - Event groups to track the status of all parking slots.
3. **Communication**:
   - Queues to manage car data (ID, entry/exit times).
   - Message buffers to transfer processed data to the server.

## Installation and Usage

1. Clone the repository:
   ```bash
   git clone https://github.com/Lai-Sheng/Parking-Lot-Management-System-Based-On-FreeRTOS.git

## Project Highlights

- **Real-Time Multitasking**: Efficient task division using FreeRTOS.
- **Resource Management**: Reliable synchronization with mutexes and event groups.
- **Modular Design**: Clear separation of tasks for maintainability.
- **Flexible Interaction**: Support for both basic and advanced display modes.

---

## Future Improvements

- Integrate car plate recognition for automated entry and exit.
- Expand the system to support larger parking lots.
- Add database integration for long-term data storage and analytics.

---



## Author

- **Lai Yuh Cherng** (賴育晟)  
  Contact: [a90483@gmail.com]

