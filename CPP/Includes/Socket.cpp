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
			cout << getLogPrefix() << "Can't send messages while is not running" << endl;

			return;
		}

		u32 size = htonl(static_cast<u32>(message.size()));  // Длина в сетевом порядке
		string packet;
		packet.resize(4+message.size());  // Выделяем место под длину и сообщение
		memcpy(&packet[0], &size, 4);  // Копируем длину
		memcpy(&packet[4], message.data(), message.size());  // Копируем сообщение
		::send(clientFD, packet.data(), packet.size(), 0);  // Отправка полного пакета

		cout << getLogPrefix() << "Sent to " << clientFD << ": " << message << endl;
	}

	void send(const string& message) {
		if(mode == Mode::Client) {
			send(socketFD, message);
		} else
		for(int clientFD : clientsFDs) {
			send(clientFD, message);
		}
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

	bool isRunning() {
		return running;
	}

	unordered_set<int> getClientsFDs() {
		return clientsFDs;
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

	/**
     * Запуск сервера: ожидание подключений и приём сообщений от клиентов
     */
	void startServer() {
		socketFD = socket(AF_UNIX, SOCK_STREAM, 0);  // Создание UNIX-сокета
		if(socketFD == -1) {
			perror("socket");
			return;
		}

		sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)-1);
		unlink(path.c_str());  // Удалить старый файл сокета, если существует

		if(bind(socketFD, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
			perror("bind");
			close(socketFD);
			return;
		}

		if(listen(socketFD, SOMAXCONN) == -1) {
			perror("listen");
			close(socketFD);
			return;
		}

		cout << getLogPrefix() << "Started (" << socketFD << ") at " << path << "\n";

		// Цикл обработки новых подключений
		while(running) {
			int clientFD = accept(socketFD, nullptr, nullptr);  // Ожидание подключения
			if(clientFD == -1) continue;

			clientsFDs.insert(clientFD);

			cout << getLogPrefix() << "Client " << socketFD << " connected\n";

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
		if(socketFD == -1) {
			perror("socket");
			return;
		}

		sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path)-1);

		if(connect(socketFD, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
			perror("connect");
			close(socketFD);
			return;
		}

		cout << getLogPrefix() << "Started (" << socketFD << ") and connected to server at " << path << "\n";

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

			if(bytes == 0) {
				cout << getLogPrefix() << "Client " << FD << " closed connection" << endl;
				break;
			} else
			if(bytes < 0) {
				if(errno == EINTR || errno == EAGAIN) {
					continue; // временная ошибка, продолжаем чтение
				}
				cout << getLogPrefix() << "Read error on FD " << FD << ": " << strerror(errno) << endl;
				break;
			}

			readBuffer.append(tmp, bytes);  // Добавляем прочитанные данные в буфер

			// Обрабатываем все доступные сообщения из буфера
			while(readBuffer.size() >= 4) {
				u32 size = 0;
				memcpy(&size, readBuffer.data(), 4);  // Считываем длину сообщения (первые 4 байта)
				size = ntohl(size);  // Преобразуем из сетевого порядка в хостовый

				if(size == 0 || size > maxSize) {
					cout << getLogPrefix() << "Invalid message length (" << size << "), discarding 4 bytes and resynchronizing." << endl;
					readBuffer.erase(0, 4);  // Попытка восстановиться
					continue;
				}

				if(readBuffer.size() < 4+size) break;  // Проверка, достаточно ли данных для полного сообщения (ждём, пока всё сообщение не придёт)

				string message = readBuffer.substr(4, size);

				if(messageHandler) {
					messageHandler(FD, message);
				}

				readBuffer.erase(0, 4+size);  // Удаление обработанных данных
			}
		}

		if(mode == Mode::Server) {
			clientsFDs.erase(FD);
		}

		if(disconnectionHandler) {
			disconnectionHandler(FD);
		}

		close(FD);
	}

	string getLogPrefix() {
		return mode == Mode::Client ? "[Client] " : "[Server] ";
	}
};

static sp<Socket> sharedServer,
				  sharedClient;