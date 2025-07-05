#pragma once

#include "Std.cpp"

class Socket {
public:
	enum class Mode {
		Server,
		Client
	};

	using MessageHandler = function<void(int, const string&)>;
	using ConnectionHandler = function<void(int)>;

	Socket(filesystem::path path, Mode mode) : path(move(path)), mode(mode) {}

	~Socket() {
		stop();
	}

	void setMessageHandler(MessageHandler handler) {
		messageHandler = move(handler);
	}

	void setConnectionHandler(ConnectionHandler onConnect, ConnectionHandler onDisconnect) {
		connectionHandler = move(onConnect);
		disconnectionHandler = move(onDisconnect);
	}

	void send(int clientFD, const string& message) {
		if(!running) {
			println(getLogPrefix(), "Can't send messages while is not running");

			return;
		}

		u32 size = htonl(static_cast<u32>(message.size()));
		string packet;
		packet.resize(4+message.size());
		memcpy(&packet[0], &size, 4);
		memcpy(&packet[4], message.data(), message.size());
		::send(clientFD, packet.data(), packet.size(), 0);

		#ifndef NDEBUG
			println(getLogPrefix(), "Sent", (mode == Mode::Server ? " to "+to_string(clientFD) : ""), ": ", message);
		#endif
	}

	void send(const string& message) {
		if(mode == Mode::Client) {
			send(socketFD, message);
		} else
		for(int clientFD : clientsFDs) {
			send(clientFD, message);
		}
	}

	void disconnect(int FD) {
		if(mode == Mode::Client) {
			if(FD == socketFD) {
				stop();
			} else {
				println(getLogPrefix(), "Can only disconnect self");
			}

			return;
		}

		shutdown(FD, SHUT_RDWR);
		close(FD);
	}

    void start() {
		if(!running) {
			running = true;
			if(mode == Mode::Server) {
				startServer();
			} else {
				startClient();
			}
		}
    }

	void stop() {
		if(running) {
			running = false;
			if(socketFD != -1) {
				close(socketFD);
				socketFD = -1;
			}
			if(mode == Mode::Server) {
				unlink(path.c_str());
			}
		}
	}

	bool isRunning() const {
		return running;
	}

	unordered_set<int> getClientsFDs() const {
		return clientsFDs;
	}

	string getLogPrefix() const {
		return mode == Mode::Server ? "[Server "+to_string(socketFD)+"] " : "[Client] ";
	}

private:
	filesystem::path path;
	Mode mode;
	int socketFD = -1;
	atomic<bool> running = false;

	MessageHandler messageHandler;
	ConnectionHandler connectionHandler,
					  disconnectionHandler;

	unordered_set<int> clientsFDs;
	mutex clientsFDsMutex;

	void startServer() {
		socketFD = socket(AF_UNIX, SOCK_STREAM, 0);
		if(socketFD == -1) {
			println(getLogPrefix(), "Failed to create socket");
			running = false;
			return;
		}

		sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)-1);
		unlink(path.c_str());  // Remove previous if exists

		if(bind(socketFD, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			println(getLogPrefix(), "Failed to bind socket");
			close(socketFD);
			running = false;
			return;
		}

		if(listen(socketFD, SOMAXCONN) < 0) {
			println(getLogPrefix(), "Failed to listen on socket");
			close(socketFD);
			running = false;
			return;
		}

		prctl(PR_SET_NAME, ("Server "+to_string(socketFD)).c_str(), 0, 0, 0);
		println(getLogPrefix(), "Started at ", path);

		while(running) {
			int clientFD = accept(socketFD, nullptr, nullptr);
			if(clientFD < 0) {
				println(getLogPrefix(), "Accept failed");
				running = false;
				break;
			}

			{
				lock_guard lock(clientsFDsMutex);
				clientsFDs.insert(clientFD);
			}

			println(getLogPrefix(), "Client ", clientFD, " connected");

			if(connectionHandler) {
				connectionHandler(clientFD);
			}

			thread(&Socket::handleMessages, this, clientFD).detach();
		}
	}

	void startClient() {
		socketFD = socket(AF_UNIX, SOCK_STREAM, 0);
		if(socketFD < 0) {
			println(getLogPrefix(), "Failed to create socket");
        	running = false;
			return;
		}

		sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)-1);

		if(connect(socketFD, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			println(getLogPrefix(), "Failed to connect to server");
			close(socketFD);
			running = false;
			return;
		}

		prctl(PR_SET_NAME, ("Client "+to_string(socketFD)).c_str(), 0, 0, 0);
		println(getLogPrefix(), "Started and connected to server at ", path);

		if(connectionHandler) {
			connectionHandler(socketFD);
		}

		handleMessages(socketFD);
	}

	void handleMessages(int FD) {
		if(mode == Mode::Server) {
			prctl(PR_SET_NAME, ("SC "+to_string(FD)).c_str(), 0, 0, 0);  // Server Connection
		}

		string readBuffer;
		constexpr usize maxSize = 10*1024*1024; // 10 MiB

		while(running) {
			char tmp[4096];
			ssize_t bytes = read(FD, tmp, sizeof(tmp));

			if(bytes <= 0) {
				if(bytes == 0) {
					println(getLogPrefix(), "Connection closed at ", FD);
				} else
				if(errno == EINTR || errno == EAGAIN) {
					continue;
				} else {
					println(getLogPrefix(), "Read error on FD ", FD, ": ", strerror(errno));
				}

				if(mode == Mode::Client) {
					running = false;
				}

				break;
			}

			readBuffer.append(tmp, bytes);

			while(readBuffer.size() >= 4) {
				u32 size = 0;
				memcpy(&size, readBuffer.data(), 4);
				size = ntohl(size);

				if(size == 0 || size > maxSize) {
					println(getLogPrefix(), "Invalid message length (", size, "), discarding 4 bytes and resynchronizing.");
					readBuffer.erase(0, 4);

					continue;
				}

				if(readBuffer.size() < 4+size) {
					break;  // Wait for a full message
				}

				string message = readBuffer.substr(4, size);

				#ifndef NDEBUG
					println(getLogPrefix(), "Received", (mode == Mode::Server ? " from "+to_string(FD) : ""), ": ", message);
				#endif

				if(messageHandler) {
					messageHandler(FD, message);
				}

				readBuffer.erase(0, 4+size);
			}
		}

		if(mode == Mode::Server) {
			lock_guard lock(clientsFDsMutex);
			clientsFDs.erase(FD);
		}

		if(disconnectionHandler) {
			disconnectionHandler(FD);
		}

		close(FD);
	}
};

static sp<Socket> sharedServer,
				  sharedClient;