#include "Lexer.cpp"
#include "Parser.cpp"
#include "Interpreter.cpp"

int main() {
	Lexer lexer = Lexer();
//	Parser parser;
//	Interpreter interpreter;

	auto lexerResult = lexer.tokenize("if a in b in c in d { e }\n\nif key in self + 1 {\n\treturn .'\\(key)'\n}\n\nreturn");

	for(auto token : lexerResult.rawTokens) {
		cout << "P: " << token->position << " T: " << token->type << " V: " << token->value << endl;
	}

	system("pause");

	return 0;
}