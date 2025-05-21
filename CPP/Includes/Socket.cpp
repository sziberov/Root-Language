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

	// Позволяет запускать экземпляр класса как функцию (в std::thread)
	void operator()() {
		start();
	}

	void send(int clientFD, const string& message) {
		if(!running) {
			println(getLogPrefix(), "Can't send messages while is not running");

			return;
		}

		u32 size = htonl(static_cast<u32>(message.size()));  // Длина в сетевом порядке
		string packet;
		packet.resize(4+message.size());  // Выделяем место под длину и сообщение
		memcpy(&packet[0], &size, 4);  // Копируем длину
		memcpy(&packet[4], message.data(), message.size());  // Копируем сообщение
		::send(clientFD, packet.data(), packet.size(), 0);  // Отправка полного пакета
		println(getLogPrefix(), "Sent to ", clientFD, ": ", message);
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

		{
			lock_guard lock(clientsFDsMutex);

			if(clientsFDs.find(FD) == clientsFDs.end()) {
				return;
			}

			clientsFDs.erase(FD);
		}

		if(disconnectionHandler) {
			disconnectionHandler(FD);
		}

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
		return (mode == Mode::Client ? "[Client: " : "[Server: ")+to_string(socketFD)+"] ";
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

	/**
     * Запуск сервера: ожидание подключений и приём сообщений от клиентов
     */
	void startServer() {
		socketFD = socket(AF_UNIX, SOCK_STREAM, 0);  // Создание UNIX-сокета
		if(socketFD == -1) {
			println(getLogPrefix(), "Failed to create socket");
			running = false;
			return;
		}

		sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)-1);
		unlink(path.c_str());  // Удалить старый файл сокета, если существует

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

		println(getLogPrefix(), "Started at ", path);

		// Цикл обработки новых подключений
		while(running) {
			int clientFD = accept(socketFD, nullptr, nullptr);  // Ожидание подключения
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

			thread(&Socket::handleMessages, this, clientFD).detach();  // Отдельный поток для клиента
		}
	}

	/**
     * Запуск клиента: подключение к серверу и чтение сообщений
     */
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

		println(getLogPrefix(), "Started and connected to server at ", path);

		if(connectionHandler) {
			connectionHandler(socketFD);
		}

		handleMessages(socketFD);
	}

	/**
     * Обработка соединения: чтение, разбор и вызов обработчика
     */
	void handleMessages(int FD) {
		string readBuffer;
		constexpr usize maxSize = 10*1024*1024; // 10 MiB — максимум разумного сообщения

		while(running) {
			char tmp[4096];  // Временный буфер
			ssize_t bytes = read(FD, tmp, sizeof(tmp));

			if(bytes <= 0) {
				if(bytes == 0) {
					println(getLogPrefix(), "Connection closed at ", FD);
				} else
				if(errno == EINTR || errno == EAGAIN) {
					continue; // временная ошибка, продолжаем чтение
				} else {
					println(getLogPrefix(), "Read error on FD ", FD, ": ", strerror(errno));
				}

				if(mode == Mode::Client) {
					running = false;
				}

				break;
			}

			readBuffer.append(tmp, bytes);  // Добавляем прочитанные данные в буфер

			// Обрабатываем все доступные сообщения из буфера
			while(readBuffer.size() >= 4) {
				u32 size = 0;
				memcpy(&size, readBuffer.data(), 4);  // Считываем длину сообщения (первые 4 байта)
				size = ntohl(size);  // Преобразуем из сетевого порядка в хостовый

				if(size == 0 || size > maxSize) {
					println(getLogPrefix(), "Invalid message length (", size, "), discarding 4 bytes and resynchronizing.");
					readBuffer.erase(0, 4);  // Попытка восстановиться
					continue;
				}

				if(readBuffer.size() < 4+size) break;  // Проверка, достаточно ли данных для полного сообщения (ждём, пока всё сообщение не придёт)

				string message = readBuffer.substr(4, size);

				println(getLogPrefix(), "Received from ", FD, ": ", message);

				if(messageHandler) {
					messageHandler(FD, message);
				}

				readBuffer.erase(0, 4+size);  // Удаление обработанных данных
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