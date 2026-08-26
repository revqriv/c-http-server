# Multithreaded C HTTP Server with In-Memory Cache

A lightweight, low-level HTTP/1.1 web server written from scratch in pure C. Built using TCP stream sockets, multithreading, and mutex-protected in-memory caching to handle concurrent requests efficiently without external dependencies.

## Features

* **Socket-Based HTTP Engine:** Listens for incoming TCP connections and parses standard HTTP/1.1 requests.
* **Concurrent Request Handling:** Uses POSIX threads (`pthread`) to handle multiple client connections simultaneously.
* **In-Memory Caching:** Stores recently served web pages in RAM to reduce disk read overhead on repeated requests.
* **Thread Safety:** Protects the cache against race conditions using POSIX Mutexes (`pthread_mutex_t`).
* **Static File Serving:** Dynamically reads and serves local files like `index.html` with appropriate content headers.

## Tech Stack

* **Language:** C (C99 standard)
* **Libraries:** Standard C libraries (`stdio.h`, `stdlib.h`, `string.h`), POSIX sockets (`sys/socket.h`), Threads (`pthread.h`)
* **Environment:** Linux / WSL (Ubuntu)

## Compilation and running

* **1. Compile the server:**
  `gcc -o server1 main.c -pthread`

* **2. Run the server:**
  `./server1`

* **3. Test in browser:**
  Navigate to `http://localhost:8080/`
