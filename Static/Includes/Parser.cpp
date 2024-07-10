#pragma once

#include "Lexer.cpp"

using Token = Lexer::Token;

class Parser {
public:
	struct Report {};

	struct Rule {
		variant<string, vector<string>, regex> triggers;
		function<bool(string)> actions;
	};

	deque<shared_ptr<Token>> tokens;
	int _position;
	deque<shared_ptr<Report>> reports;

	vector<Rule> rules {};
};