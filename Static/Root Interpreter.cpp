#include "Lexer.cpp"
#include "Parser.cpp"
#include "Interpreter.Micro.cpp"

#include "crow_all.h"
#include "glaze/glaze.hpp"

int main() {
	Lexer lexer;
	Parser parser;
	Interpreter interpreter;

	optional<Lexer::Result> lexerResult;
	optional<Parser::Result> parserResult;
	optional<Interpreter::Result> interpreterResult;

	crow::SimpleApp app;
	mutex interpreterLock;

	CROW_ROUTE(app, "/")([](const crow::request&, crow::response& response) {
		response.set_static_file_info("Resources/Interface.html");
		response.end();
	});
	CROW_ROUTE(app, "/lex").methods("POST"_method)([&](const crow::request& req) {
		auto lock = lock_guard<mutex>(interpreterLock);
		char* iterationsString = req.url_params.get("iterations");
		int iterations = iterationsString != nullptr ? stoi(iterationsString) : 1;
		string lexerResultString;
		chrono::milliseconds duration_0(0),
							 duration_1(0);

		if(iterations < 1) {
			iterations = 1;
		}

		for(int i = 0; i < iterations; i++) {
			auto start = chrono::high_resolution_clock::now();
			lexerResult = lexer.tokenize(req.body);
			auto stop_0 = chrono::high_resolution_clock::now();
			lexerResultString = glz::write_json(lexerResult).value_or("error");
			auto stop_1 = chrono::high_resolution_clock::now();

			duration_0 += chrono::duration_cast<chrono::milliseconds>(stop_0-start);
			duration_1 += chrono::duration_cast<chrono::milliseconds>(stop_1-start);
		}

		cout << "                   [   Lexer   ] Taken ";

		if(iterations == 1) {
			cout << duration_0.count() << " (" << duration_1.count() << " with serialization) ms by string(" << req.body.length() << ")";
		} else {
			auto averageDuration_0 = duration_0.count()/iterations,
				 averageDuration_1 = duration_1.count()/iterations;

			cout << averageDuration_0 << " (" << averageDuration_1 << " with serialization) ms by string(" << req.body.length() << ")[" << iterations << "]";
		}

		cout << endl;

		return lexerResultString;
	});
	CROW_ROUTE(app, "/parse")([&]() {
		auto lock = lock_guard<mutex>(interpreterLock);
		if(lexerResult == nullopt) return string();
		auto start = chrono::high_resolution_clock::now();
		parserResult = parser.parse(*lexerResult);
		auto stop_0 = chrono::high_resolution_clock::now();
		string parserResultString = glz::write_json(parserResult).value_or("error");
		auto stop_1 = chrono::high_resolution_clock::now();
		auto duration_0 = chrono::duration_cast<chrono::milliseconds>(stop_0-start),
			 duration_1 = chrono::duration_cast<chrono::milliseconds>(stop_1-start);

		cout << "                   [   Parser  ] Taken " << duration_0.count() << " (" << duration_1.count() << " with serialization) ms by rawTokens(" << lexerResult->rawTokens.size() << ")" << endl;

		return parserResultString;
	});
	CROW_ROUTE(app, "/interpret")([&]() {
		auto lock = lock_guard<mutex>(interpreterLock);
		if(lexerResult == nullopt || parserResult == nullopt) return string();
		auto start = chrono::high_resolution_clock::now();
		interpreterResult = interpreter.interpret(*lexerResult, *parserResult);
		auto stop_0 = chrono::high_resolution_clock::now();
		string interpreterResultString = glz::write_json(interpreterResult).value_or("error");
		auto stop_1 = chrono::high_resolution_clock::now();
		auto duration_0 = chrono::duration_cast<chrono::milliseconds>(stop_0-start),
			 duration_1 = chrono::duration_cast<chrono::milliseconds>(stop_1-start);

		cout << "                   [Interpreter] Taken " << duration_0.count() << " (" << duration_1.count() << " with serialization) ms by rawTokens(" << lexerResult->rawTokens.size() << ")" << endl;

		return interpreterResultString;
	});

	app.port(3007)/*.multithreaded()*/.run();

	return 0;
}