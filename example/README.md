# Example

## Usage

### Client

- Install libcurl

```sh
  $ sudo apt install libcurl4-openssl-dev  # install libcurl
```

- Compile main.cpp with files under src/ into an excutable file.

#### Linux

```sh
  $ g++ main.cpp ../src/WebSocketClientImplCurl.cpp -I../src/ -fpermissive -pthread -lcurl -o example
  $ ./example
```

### Server (python is required)

- Install tornado 5.0.2 package. (other version may be incompatible)

```sh
  $ pip install tornado==5.0.2
```

- Start test-server with the follow command. This server will listen on port 8000 by default.

```sh
  $ python test-server.py
```
