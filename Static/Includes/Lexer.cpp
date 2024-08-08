#pragma once

#include <iostream>
#include <regex>
#include <set>
#include <string>
#include <variant>
#include <unordered_map>
#include <functional>
#include <optional>
#include <deque>
#include <map>

using namespace std;

template <typename Container, typename Predicate>
bool some(const Container& container, Predicate predicate) {
	return any_of(container.begin(), container.end(), predicate);
}

template<typename Container, typename UnaryPredicate>
Container filter(const Container& container, UnaryPredicate predicate) {
	Container result;
	copy_if(container.begin(), container.end(), back_inserter(result), predicate);
	return result;
}

// ----------------------------------------------------------------

class Lexer {
public:
	struct Location {
		int line,
			column;
	};

	struct Token {
		int position;
		Location location;
		string type,
			   value;
		bool nonmergeable,
			 generated;
	};

	struct Rule {
		variant<string, vector<string>, regex> triggers;
		function<bool(string)> actions;
	};

	string_view code;
	int position;
	deque<Token> tokens;
	deque<string> states;
	vector<Rule> rules {
		{"#!", [this](const string& v) {
			if(atComments() || atString()) {
				helpers_continueString();
				token().value += v;
			} else
			if(position == 0) {
				addToken("commentShebang", v);
				addState("comment");
			} else {
				return true;
			}

			return false;
		}},
		{vector<string> {"/*", "*/"}, [this](const string& v) {
			if(atComments() || atString()) {
				helpers_continueString();
				token().value += v;
			} else
			if(v == "/*") {
				addToken("commentBlock", v);
			}
			if(token().type == "commentBlock") {
				if(v == "/*") {
					addState("comment");
				} else {
					removeState("comment");
				}
			}

			return false;
		}},
		{"//", [this](const string& v) {
			if(atComments() || atString()) {
				helpers_continueString();
				token().value += v;
			} else {
				addToken("commentLine", v);
				addState("comment");
			}

			return false;
		}},
		{vector<string> {"\\\\", "\\'", "\\(", "\\b", "\\f", "\\n", "\\r", "\\t", "\\v", "\\"}, [this](const string& v) {
			if(atComments()) {
				token().value += v;

				return false;
			}
			if(!atString()) {
				addToken("unsupported", v);

				return false;
			}
			if(v == "\\(") {
				return true;
			}

			helpers_continueString();
			token().value +=
				v == "\\\\" ? "\\" :
				v == "\\'" ? "'" :
				v == "\\b" ? "\b" :
				v == "\\f" ? "\f" :
				v == "\\n" ? "\n" :
				v == "\\r" ? "\r" :
				v == "\\t" ? "\t" :
				v == "\\v" ? "\v" : "";

			return false;
		}},
		{vector<string> {"\\(", ")"}, [this](const string& v) {
			if(atComments()) {
				token().value += v;

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
		{vector<string> {"!", "%", "&", "*", "+", ",", "-", ".", "/", ":", "<", "=", ">", "?", "^", "|", "~"}, [this](const string& v) {
			if(atComments() || atString()) {
				helpers_continueString();
				token().value += v;

				return false;
			}

			static set<string> initializers = {",", ".", ":"},								// Create new token if value exists in the list and current (operator) token doesn't initialized with it
							   singletons = {"!", "?"},										// Create new token if current (postfix operator) token matches any value in the list
							   generics = {"!", "&", ",", ".", ":", "<", ">", "?", "|"};	// Only values in the list are allowed for generic types

			bool initializer = initializers.contains(v) && !token().value.starts_with(v),
				 singleton = token().type == "operatorPostfix" && singletons.contains(token().value),
				 generic = generics.contains(v);

			if(!generic) {
				helpers_mergeOperators();
			}

			bool closingAngle = atAngle() && v == ">";

			if(token().type.starts_with("operator") && !initializer && !singleton && !closingAngle) {
				token().value += v;

				return false;
			}

			string type = "operator";

			if(singleton) {
				type = token().type;
			} else {
				if(
					set<string> {"string", "identifier"}.contains(token().type) ||
					token().type.ends_with("Closed") ||
					token().type.starts_with("number") ||
					token().type.starts_with("keyword")
				) {
					type += "Postfix";
				}

				helpers_specifyOperatorType();
			}

			addToken(type);
			if(initializer || singleton) {
				token().nonmergeable = true;
			}

			if(v == "<") addState("angle");
			if(v == ">") removeState("angle");

			return false;
		}},
		{vector<string> {"(", ")", "[", "]", "{", "}"}, [this](const string& v) {
			if(atComments() || atString()) {
				helpers_continueString();
				token().value += v;

				return false;
			}

			string type =
				v == "(" ? "parenthesisOpen" :
				v == ")" ? "parenthesisClosed" :
				v == "[" ? "bracketOpen" :
				v == "]" ? "bracketClosed" :
				v == "{" ? "braceOpen" :
				v == "}" ? "braceClosed" : "";

			if(type.ends_with("Open")) {
				helpers_specifyOperatorType();
			}
			addToken(type);
			removeState("angle", 2);  // Balanced tokens except </> are allowed right after generic types but not inside them

			if(v == "{" && atStatement() && atToken([](string t, string v) { return t == "whitespace" && v.contains("\n"); }, ignorable, -1)) {
				addState("statementBody");

				return false;
			}
			if(v == "}" && atStatementBody() && !atFutureToken([](string t, string v) { return set<string> {"keywordElse", "keywordWhere"}.contains(t); }, ignorable)) {
				addToken("delimiter", ";");
				token().generated = true;
				removeState("statement");

				return false;
			}

			if(atStringExpressions()) {
				if(v == "(") addState("parenthesis");
				if(v == ")") removeState("parenthesis");
			}
			if(atStatements()) {
				if(v == "{") addState("brace");
				if(v == "}") removeState("brace");
			}

			return false;
		}},
		{"'", [this](const string& v) {
			if(atComments()) {
				token().value += v;

				return false;
			}

			if(!atString()) {
				helpers_finalizeOperator();
				addToken("stringOpen");
				addState("string");
			} else {
				addToken("stringClosed");
				removeState("string");
			}

			return false;
		}},
		{";", [this](const string& v) {
			if(atComments() || atString()) {
				helpers_continueString();
				token().value += v;

				return false;
			}

			if(token().type == "delimiter" && token().generated) {
				token().generated = false;
			} else {
				addToken("delimiter");
			}

			return false;
		}},
		{"\n", [this](const string& v) {
			if(atComments() && !set<string> {"commentShebang", "commentLine"}.contains(token().type) || atString()) {
				helpers_continueString();
				token().value += v;

				return false;
			}

			if(token().type != "whitespace") {
				addToken("whitespace", v);

				if(atComments()) {
					removeState("comment");
				}
			} else {
				token().value += v;
			}

			if(atStatement() && count(token().value.begin(), token().value.end(), '\n') == 1) {
				auto braceOpen = [](string t, optional<string> v) { return t == "braceOpen"; };

				if(atToken(braceOpen, ignorable)) {
					addState("statementBody");
				} else
				if(!atState("brace", set<string>()) && !atFutureToken(braceOpen, ignorable)) {
					removeState("statement");
				}
			}

			return false;
		}},
		{regex("[^\\S\\n]+"), [this](const string& v) {
			if(atComments() || atString() || token().type == "whitespace") {
				helpers_continueString();
				token().value += v;

				return false;
			}

			addToken("whitespace", v);

			return false;
		}},
		{regex("[0-9]+"), [this](const string& v) {
			if(atComments() || atString()) {
				helpers_continueString();
				token().value += v;

				return false;
			}
			if(token().type == "operatorPostfix" && token().value == "." && getToken(-1).type == "numberInteger") {
				removeToken();
				token().type = "numberFloat";
				token().value += "."+v;

				return false;
			}

			helpers_finalizeOperator();
			addToken("numberInteger", v);

			return false;
		}},
		{regex("[a-z_$][a-z0-9_$]*", regex_constants::icase), [this](const string& v) {
			if(atComments() || atString()) {
				helpers_continueString();
				token().value += v;

				return false;
			}

			static set<string> keywords = {
				// Flow-related words (async, class, for, return...)
				// Literals (false, nil, true...)
				// Types (Any, bool, _, ...)

				// Words as Self, arguments, metaSelf or self are not allowed because they all
				// have dynamic values and it's logical to distinguish them at the interpretation stage

				"Any",
				"Class",
				"Enumeration",
				"Function",
				"Namespace",
				"Object",
				"Protocol",
				"Structure",
				"any", "async", "await", "awaits",
				"bool", "break",
				"case", "catch", "class", "continue",
				"dict", "do",
				"else", "enum", "extension",
				"fallthrough", "false", "final", "float", "for", "func",
				"if", "import", "in", "infix", "inout", "int", "is",
				"lazy",
				"namespace", "nil",
				"operator",
				"postfix", "prefix", "private", "protected", "protocol", "public",
				"return",
				"static", "string", "struct",
				"throw", "throws", "true", "try", "type",
				"var", "virtual", "void",
				"when", "where", "while",
				"_"
			};

			helpers_specifyOperatorType();

			string type = "identifier";
			bool chain = atToken([](string t, string v) { return t.starts_with("operator") && !t.ends_with("Postfix") && v == "."; }, ignorable);

			if(keywords.contains(v) && !chain) {  // Disable keywords in a chains
				type = "keyword";

				if(v == "_") {
					type += "Underscore";
				} else {
					string capitalizedV = v;
					capitalizedV[0] = toupper(capitalizedV[0]);

					if(v[0] == capitalizedV[0]) {
						type += "Capital";
					}

					type += capitalizedV;
				}
			}

			if(atAngle() && type.starts_with("keyword")) {  // No keywords are allowed in generic types
				helpers_mergeOperators();
			}
			addToken(type, v);

			// Blocks in some statements can be treated by parser like expressions first, which can lead to false inclusion of futher tokens into that expression and late closing
			// This can be fixed by tracking and closing them beforehand at lexing stage
			if(some(set<string> {"For", "If", "When", "While"}, [&type](string v) { return type.ends_with(v); })) {
				addState("statement");
			}

			return false;
		}},
		{regex("."), [this](const string& v) {
			if(atComments() || atString() || token().type == "unsupported") {
				helpers_continueString();
				token().value += v;

				return false;
			}

			addToken("unsupported", v);

			return false;
		}}
	};

	void helpers_continueString() {
		if((set<string> {"stringOpen", "stringExpressionClosed"}).contains(token().type)) {
			addToken("stringSegment", "");
		}
	}

	void helpers_mergeOperators() {
		removeState("angle", 2);

		for(int i = tokens.size(); i >= 0; i--) {
			if(
				!token().type.starts_with("operator") || token().nonmergeable ||
				!getToken(-1).type.starts_with("operator") || getToken(-1).nonmergeable
			) {
				break;
			}

			getToken(-1).value += token().value;
			removeToken();
		}
	}

	void helpers_specifyOperatorType() {
		if(token().type == "operator") {
			token().type = "operatorPrefix";
		} else
		if(token().type == "operatorPostfix") {
			token().type = "operatorInfix";
		}
	}

	void helpers_finalizeOperator() {
		helpers_mergeOperators();
		helpers_specifyOperatorType();
	}

	inline bool codeEnd() {
		return position >= code.length();
	}

	inline Token& token() {
		return getToken();
	}

	Location location() {
		Location location = token().location;

		for(int position = token().position; position < this->position; position++) {
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

	inline bool atComments() {
		return atState("comment");
	}

	inline bool atString() {
		return atState("string", set<string>());
	}

	inline bool atStringExpression() {
		return atState("stringExpression", set<string>());
	}

	inline bool atStringExpressions() {
		return atState("stringExpression");
	}

	inline bool atStatement() {
		return atState("statement", set<string> {"brace"});
	}

	inline bool atStatements() {
		return atState("statement");
	}

	inline bool atStatementBody() {
		return atState("statementBody", set<string>());
	}

	inline bool atAngle() {
		return atState("angle", set<string>());
	}

	function<bool(string, optional<string>)> ignorable = [](string t, optional<string> v = nullopt) {
		return t == "whitespace" || t.starts_with("comment");
	};

	Token& getToken(int offset = 0) {
		static Token dummy;

		return tokens.size() > tokens.size()-1+offset
			 ? tokens[tokens.size()-1+offset]
			 : dummy = Token();
	}

	void addToken(const string& type, optional<string> value = nullopt) {
		tokens.push_back(Token {
			position,
			location(),
			type,
			value.value_or(string(1, code[position]))
		});
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
			Token& token = tokens[i];
			string type = token.type,
				   value = token.value;

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

		position = token().position+token().value.length();  // Override allows nested calls

		while(!codeEnd()) {
			nextToken();

			Token& token = getToken();
			string type = token.type,
				   value = token.value;

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

	void addState(const string& type) {
		states.push_back(type);
	}

	/*
	 * 0 - Remove states starting from rightmost found (inclusively)
	 * 1 - 0 with subtypes, ignoring nested
	 * 2 - Remove found states globally
	 */
	void removeState(const string& type, int mode = 0) {
		if(mode == 0) {
			int i = -1;

			for(int j = states.size()-1; j >= 0; j--) {
				if(states[j] == type) {
					i = j;

					break;
				}
			}

			if(i > -1) {
				states.resize(i);
			}
		}
		if(mode == 1) {
			for(int i = states.size()-1; i >= 0; i--) {
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
			erase(states, type);
		}
	}

	/*
	 * Check for state starting from rightmost (inclusively).
	 *
	 * Unstrict by default, additionaly whitelist can be set.
	 */
	bool atState(const string& type, optional<set<string>> whitelist = nullopt) {
		for(int i = states.size()-1; i >= 0; i--) {
			if(states[i] == type) {
				return true;
			}
			if(whitelist != nullopt && !whitelist->contains(states[i])) {
				return false;
			}
		}

		return false;
	}

	struct Save {
		int position;
		deque<Token> tokens;
		deque<string> states;
	};

	Save getSave() {
		return {
			position,
			tokens,
			states
		};
	}

	void restoreSave(Save save) {
		position = save.position;
		tokens = save.tokens;
		states = save.states;
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

	bool atSubstring(const string& substring) {
	    if(position+substring.size() > code.size()) {
	        return false;
	    }

	    return equal(substring.begin(), substring.end(), code.begin()+position);
	}

	optional<string> atRegex(const regex& regex) {
		smatch match;
		string searchArea = string(code.substr(position));

		if(regex_search(searchArea, match, regex) && match.position() == 0) {
			return match.str(0);
		}

		return nullopt;
	}

	void nextToken() {
		for(Rule& rule : rules) {
			auto& triggers = rule.triggers;
			auto actions = rule.actions;
			bool plain = triggers.index() == 0,
				 array = triggers.index() == 1,
				 regex = triggers.index() == 2;
			optional<string> trigger = nullopt;

			if(plain && atSubstring(get<string>(triggers))) {
				trigger = get<string>(triggers);
			}
			if(array) {
				for(string& v : get<vector<string>>(triggers)) {
					if(atSubstring(v)) {
						trigger = v;

						break;
					}
				}
			}
			if(regex) {
				trigger = atRegex(get<::regex>(triggers));
			}

			if(trigger != nullopt && !actions(*trigger)) {  // Multiple rules can be executed on same position if some of them return true
				position += trigger->length();  // Rules shouldn't explicitly set position

				break;
			}
		}
	}

	struct Result {
		deque<Token> rawTokens,
					 tokens;
	};

	Result tokenize(string_view code) {
		Result result;

		reset();

		this->code = code;

		while(!codeEnd()) {
			nextToken();  // Zero-length position commits will lead to forever loop, rules developer attention is advised
		}

		result.rawTokens = move(tokens);
		result.tokens = filter(result.rawTokens, [this](auto& v) { return !ignorable(v.type, nullopt); });

		reset();

		return result;
	}
};