#include <iostream>
#include <regex>
#include <set>
#include <string>
#include <variant>
#include <unordered_map>

using namespace std;

class Lexer {
public:
	struct Location {
		int line = 0,
			column = 0;
	};

	struct Token {
		int position = 0;
		shared_ptr<Location> location = nullptr;
		string type = "",
			   value = "";
		bool nonmergeable = false,
			 generated = false;
	};

	struct Rule {
		variant<string, vector<string>, regex> triggers;
		function<bool(string)> actions;
	};

	string code;
	int position;
	vector<shared_ptr<Token>> tokens;
	vector<string> states;
	vector<Rule> rules {
		{"#!", [this](string v) {
			if(atComments() || atString()) {
				helpers["continueString"]();
				token()->value += v;
			} else
			if(position == 0) {
				addToken("commentShebang", v);
				addState("comment");
			} else {
				return true;
			}

			return false;
		}},
		{vector<string> {"/*", "*/"}, [this](string v) {
			if(atComments() || atString()) {
				helpers["continueString"]();
				token()->value += v;
			} else
			if(v == "/*") {
				addToken("commentBlock", v);
			}
			if(token()->type == "commentBlock") {
				if(v == "/*") {
					addState("comment");
				} else {
					removeState("comment");
				}
			}

			return false;
		}},
		{"//", [this](string v) {
			if(atComments() || atString()) {
				helpers["continueString"]();
				token()->value += v;
			} else {
				addToken("commentLine", v);
				addState("comment");
			}

			return false;
		}},
		{vector<string> {"\\\\", "\\'", "\\(", "\\b", "\\f", "\\n", "\\r", "\\t", "\\v", "\\"}, [this](string v) {
			if(atComments()) {
				token()->value += v;

				return false;
			}
			if(!atString()) {
				addToken("unsupported", v);

				return false;
			}
			if(v == "\\(") {
				return true;
			}

			helpers["continueString"]();
	        unordered_map<string, string> escapes = {
	            {"\\\\", "\\"},
	            {"\\'", "'"},
	            {"\\b", "\b"},
	            {"\\f", "\f"},
	            {"\\n", "\n"},
	            {"\\r", "\r"},
	            {"\\t", "\t"},
	            {"\\v", "\v"},
	            {"\\", ""}
	        };

	        auto it = escapes.find(v);
	        if(it != escapes.end()) {
	            token()->value += it->second;
	        }

			return false;
		}},
		{vector<string> {"\\(", ")"}, [this](string v) {
			if(atComments()) {
				token()->value += v;

				return false;
			}

			if(v == "\\(" && atString()) {
				addToken("stringExpressionOpen", v);
				addState("stringExpression");
			}
			if(v == ")" && atStringExpression()) {
				addToken("stringExpressionClosed");
				removeState("stringExpression");
			} else {
				return v == ")";
			}

			return false;
		}},
		/////////////////////////////////
		{"'", [this](string v) {
			if(atComments()) {
				token()->value += v;

				return false;
			}

			if(!atString()) {
				helpers["finalizeOperator"]();
				addToken("stringOpen");
				addState("string");
			} else {
				addToken("stringClosed");
				removeState("string");
			}

			return false;
		}},
		{";", [this](string v) {
			if(atComments() || atString()) {
				helpers["continueString"]();
				token()->value += v;

				return false;
			}

			if(token()->type == "delimiter" && token()->generated) {
				token()->generated = false;
			} else {
				addToken("delimiter");
			}

			return false;
		}},
		//////////////////
		{regex("."), [this](string v) {
			if(atComments() || atString() || token()->type == "unsupported") {
				helpers["continueString"]();
				token()->value += v;

				return false;
			}

			addToken("unsupported", v);

			return false;
		}}
	};

	map<string, function<void()>> helpers {
		{"continueString", [this]() {
			if((set<string> {"stringOpen", "stringExpressionClosed"}).contains(token()->type)) {
				addToken("stringSegment", "");
			}
		}},
		{"mergeOperators", [this]() {
			removeState("angle", 2);

			for(int i = tokens.size(); i >= 0; i--) {
				if(
					!token()->type.starts_with("operator") || token()->nonmergeable ||
					!getToken(-1)->type.starts_with("operator") || getToken(-1)->nonmergeable
				) {
					break;
				}

				getToken(-1)->value += token()->value;
				removeToken();
			}
		}},
		{"specifyOperatorType", [this]() {
			if(token()->type == "operator") {
				token()->type = "operatorPrefix";
			} else
			if(token()->type == "operatorPostfix") {
				token()->type = "operatorInfix";
			}
		}},
		{"finalizeOperator", [this]() {
			helpers["mergeOperators"]();
			helpers["specifyOperatorType"]();
		}}
	};

	bool codeEnd() {
		return position >= code.length();
	}

	shared_ptr<Token> token() {
		return getToken();
	}

	Location location() {
		Location location = token()->location != nullptr ? *token()->location : Location();

		for(int position = token()->position; position < this->position; position++) {
			char character = code[position];

			if(character == '\n') {
				location.line++;
				location.column = 0;
			} else {
				location.column += character == '\t' ? 4 : 1;
			}
		}

		return location;
	}

	bool atComments() {
		return atState("comment");
	}

	bool atString() {
		return atState("string", {});
	}

	bool atStringExpression() {
		return atState("stringExpression", {});
	}

	bool atStringExpressions() {
		return atState("stringExpression");
	}

	bool atStatement() {
		return atState("statement", make_shared<set<string>>(initializer_list<string> {"brace"}));
	}

	bool atStatements() {
		return atState("statement");
	}

	bool atStatementBody() {
		return atState("statementBody", {});
	}

	bool atAngle() {
		return atState("angle", {});
	}

	bool ignorable(string t) {
		return t == "whitespace" || t.starts_with("comment");
	}

	shared_ptr<Token> getToken(int offset = 0) {
		return tokens.size() > tokens.size()-1+offset
			 ? tokens[tokens.size()-1+offset]
			 : make_shared<Token>(Token());
	}

	void addToken(string type, optional<string> value = nullopt) {
		tokens.push_back(make_shared<Token>(Token {
			position,
			make_shared<Location>(location()),
			type,
			value.value_or(string(1, code[position]))
		}));
	}

	void removeToken(int offset = 0) {
		tokens.erase(tokens.begin()+tokens.size()-1+offset);
	}

	/*
	 * Check for token(s) starting from rightmost (inclusively if no offset set), using combinations of type and value within predicate.
	 *
	 * Strict by default, additionally whitelist predicate can be set.
	 *
	 * Useful for complex token sequences check.
	 */
	bool atToken(function<bool(string, string)> conforms, optional<function<bool(string, string)>> whitelisted = nullopt, int offset = 0) {
		for(int i = tokens.size()-1+offset; i >= 0; i--) {
			shared_ptr<Token> token = tokens[i];
			string type = token->type,
				   value = token->value;

			if(conforms(type, value)) {
				return true;
			}
			if(whitelisted != nullopt && !(*whitelisted)(type, value)) {
				return false;
			}
		}

		return false;
	}

	/*
	 * Future-time version of atToken(). Rightmost (at the moment) token is not included in a search.
	 */
	bool atFutureToken(function<bool(string, string)> conforms, optional<function<bool(string, string)>> whitelisted = nullopt) {
		Save save = getSave();
		bool result = false;

		position = token()->position+token()->value.length();  // Override allows nested calls

		while(!codeEnd()) {
			nextToken();

			shared_ptr<Token> token = tokens.back();
			string type = token != nullptr ? token->type : "",
				   value = token != nullptr ? token->value : "";

			if(conforms(type, value)) {
				result = true;

				break;
			}
			if(whitelisted != nullopt && !(*whitelisted)(type, value)) {
				break;
			}
		}

		restoreSave(save);

		return result;
	}

	void addState(string type) {
		states.push_back(type);
	}

	/*
	 * 0 - Remove states starting from rightmost found (inclusively)
	 * 1 - 0 with subtypes, ignoring nested
	 * 2 - Remove found states globally
	 */
	void removeState(string type, int mode = 0) {
		if(mode == 0) {
			int i = -1;

			for(int j = states.size()-1; j >= 0; --j) {
				if(states[j] == type) {
					i = j;

					break;
				}
			}

			if(i != -1) {
				states.resize(i);
			}
		}
		if(mode == 1) {
			for(int i = states.size()-1; i >= 0; --i) {
				string type_ = states[i];

				if(type_.starts_with(type)) {
					states.erase(states.begin()+i);
				}
				if(type_ == type) {
					break;
				}
			}
		}
		if(mode == 2) {
			states.erase(remove(states.begin(), states.end(), type), states.end());
		}
	}

	/*
	 * Check for state starting from rightmost (inclusively).
	 *
	 * Unstrict by default, additionaly whitelist can be set.
	 */
	bool atState(string type, shared_ptr<set<string>> whitelist = nullptr) {
		for(int i = states.size()-1; i >= 0; i--) {
			if(states[i] == type) {
				return true;
			}
			if(whitelist != nullptr && whitelist->contains(states[i])) {
				return false;
			}
		}

		return false;
	}

	struct Save {
		int position = position;
	//	vector<Token> tokens = JSON.stringify(tokens);
	//	vector<string> states = JSON.stringify(states);
	};

	Save getSave() {
		return Save();
	}

	void restoreSave(Save save) {
		position = save.position;
	//	tokens = JSON.parse(save.tokens);
	//	states = JSON.parse(save.states);
	}

	void reset() {
		code = "";
		position = 0;
		tokens = {};
		states = {};
			// angle - used to distinguish between common operator and generic type's closing >
			// brace - used in statements
			// parenthesis - used in string expressions
	}

	bool atSubstring(string substring) {
		return code.find(substring, position) == position;
	}

    optional<string> atRegex(regex regex) {
        smatch match;
        string search_area = code.substr(position);

        if(regex_search(search_area, match, regex) && match.position() == 0) {
            return match.str(0);
        }

        return nullopt;
    }

	void nextToken() {
		for(Rule rule : rules) {
			auto triggers = rule.triggers;
			auto actions = rule.actions;
			bool plain = holds_alternative<string>(triggers),
				 array = holds_alternative<vector<string>>(triggers),
				 regex_ = holds_alternative<regex>(triggers);
			optional<string> trigger = nullopt;

			if(plain && atSubstring(get<string>(triggers))) {
				trigger = get<string>(triggers);
			}
			if(array) {
				for(string v : get<vector<string>>(triggers)) {
					if(atSubstring(v)) {
						trigger = v;

						break;
					}
				}
			}
			if(regex_) {
				trigger = atRegex(get<regex>(triggers));
			}

			if(trigger != nullopt && !actions(*trigger)) {  // Multiple rules can be executed on same position if some of them return true
				position += (*trigger).length();  // Rules shouldn't explicitly set position

				break;
			}
		}
	}

	struct Result {
		vector<shared_ptr<Token>> rawTokens,
								  tokens;
	};

	Result tokenize(string code) {
		reset();

		this->code = code;

		while(!codeEnd()) {
			nextToken();  // Zero-length position commits will lead to forever loop, rules developer attention is advised
			cout << position << " / " << tokens.size() << " / " << token()->value << endl;
		}

		vector<shared_ptr<Token>> tokens_ = {};

		for(int i = 0; i < tokens.size(); i++)
			if(!ignorable(tokens[i]->type))
				tokens_.push_back(tokens[i]);

		Result result = {
			rawTokens: tokens,
			tokens: tokens_
		};

		reset();

		return result;
	}
};

int main() {
	Lexer lexer = Lexer();
//	Parser parser;
//	Interpreter interpreter;

	lexer.tokenize("/*asd*//*edf*/");
	system("pause");

	return 0;
}
