#include "Lexer.cpp"
#include "Parser.cpp"
#include "Interpreter.cpp"

#include "crow_all.h"
#include "glaze/glaze.hpp"

int main() {
	Lexer lexer = Lexer();
//	Parser parser;
//	Interpreter interpreter;

	crow::SimpleApp app;
	mutex lexerLock;

	CROW_ROUTE(app, "/")([](const crow::request&, crow::response& response) {
		response.set_static_file_info("Resources/Interface.html");
		response.end();
	});
	CROW_ROUTE(app, "/lex").methods("POST"_method)([&lexer, &lexerLock](const crow::request& req) {
		auto lock = lock_guard<mutex>(lexerLock);
		auto start = chrono::high_resolution_clock::now();
		string lexerResult = glz::write_json(lexer.tokenize(req.body)).value_or("error");
		auto stop = chrono::high_resolution_clock::now();
		auto duration = chrono::duration_cast<chrono::milliseconds>(stop-start);

		cout << "                      [Lexer   ] Taken " << duration.count() << "ms by string(" << req.body.length() << ")" << endl;

		return lexerResult;
	});

	app.port(3007).multithreaded().run();

	return 0;
}