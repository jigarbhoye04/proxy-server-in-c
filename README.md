# Proxy Server in C

This repository implements a multi-threaded HTTP proxy server in C, with and without caching functionality. The project demonstrates network programming, socket handling, HTTP request parsing, and thread management using POSIX threads (pthreads).

## Project Structure

```
proxy-server/
├── Makefile
├── README.md
├── include/
│   ├── proxy_parse.h
│   └── pthread.h
├── src/
│   ├── proxy_server_with_cache.c
│   └── proxy_server_without_cache.c
├── tests/
```

### Key Files

- **src/proxy_server_with_cache.c**: Main implementation of the proxy server with caching support. Handles client requests, forwards them to the destination server, caches responses, and serves cached content when appropriate.
- **src/proxy_server_without_cache.c**: Implementation of the proxy server without caching. Forwards client requests to the destination server and relays responses without storing them.
- **include/proxy_parse.h**: Header file for HTTP request parsing utilities used by the proxy server.
- **include/pthread.h**: Header file for POSIX thread (pthreads) support.
- **Makefile**: Build instructions for compiling the proxy server. Supports building, cleaning, and packaging the project.

## Features

- **Multi-threaded**: Handles multiple client connections concurrently using pthreads.
- **HTTP Request Parsing**: Parses and validates HTTP requests from clients.
- **Caching (with_cache version)**: Stores server responses and serves cached content to improve performance.
- **No Caching (without_cache version)**: Forwards requests and responses without storing data.
- **POSIX Threads**: Utilizes pthreads for concurrency.

## Build Instructions

Ensure you have a C/C++ compiler (e.g., `g++`) and POSIX thread support.

1. **Clone the repository**
	```sh
	git clone https://github.com/jigarbhoye04/proxy-server-in-c.git
	cd proxy-server-in-c
	```
2. **Build the proxy server**
	```sh
	make
	```
	This will compile the source files and generate the `proxy` executable.

3. **Clean build artifacts**
	```sh
	make clean
	```

4. **Package the project**
	```sh
	make tar
	```
	This creates a `ass1.tgz` archive with the main source and build files.

## Usage

Run the compiled proxy server executable. You may need to specify the port and other options depending on your implementation.

Example:
```sh
./proxy <port>
```

## Testing

- Place your test scripts or files in the `tests/` directory.
- You can use tools like `curl` or a web browser configured to use the proxy for manual testing.

## Notes

- The project is intended for educational purposes and may not be production-ready.
- Ensure that the required header files are present in the `include/` directory.
- The Makefile assumes a Unix-like environment. On Windows, use a compatible shell or adapt the commands as needed.

## License

This project is open-source and available under the MIT License.

## Author

[Jigar Bhoye](https://github.com/jigarbhoye04)