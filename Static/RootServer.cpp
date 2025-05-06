#include "Interface.cpp"
#include "Lexer.cpp"
#include "Parser.cpp"
#include "Interpreter.cpp"
#include "crow.h"

int main() {
	optional<deque<Lexer::Token>> tokens;
	optional<NodeSP> tree;
	optional<Interpreter::TypeSP> value;

	crow::SimpleApp app;
	mutex interpreterLock;

	CROW_ROUTE(app, "/")([](const crow::request&, crow::response& response) {
		response.set_static_file_info("Resources/Interface.html");
		response.end();
	});
	CROW_WEBSOCKET_ROUTE(app, "/interface")
    .onopen([&](crow::websocket::connection& conn) {
        Interface::register_connection(&conn);
    })
    .onclose([&](crow::websocket::connection& conn, const string& reason, uint16_t) {
        Interface::unregister_connection(&conn);
    })
    .onmessage([&](crow::websocket::connection& conn, const string& data, bool isBinary) {
        if(!isBinary) {
			Interface::handle_message(data);
		}
    });
	CROW_ROUTE(app, "/lex").methods("POST"_method)([&](const crow::request& req) {
		auto lock = lock_guard<mutex>(interpreterLock);

		tokens = sharedLexer.tokenize(req.body);

		return string();
	});
	CROW_ROUTE(app, "/parse")([&]() {
		auto lock = lock_guard<mutex>(interpreterLock);

		if(tokens) {
			tree = sharedParser.parse(*tokens);
		}

		return string();
	});
	CROW_ROUTE(app, "/interpret")([&]() {
		auto lock = lock_guard<mutex>(interpreterLock);

		if(tokens && tree) {
			value = sharedInterpreter.interpret(*tokens, *tree);
		}

		return string();
	});

	app.port(3007).multithreaded().run();

	return 0;
}