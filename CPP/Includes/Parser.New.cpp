#pragma once

#include "Lexer.cpp"
#include "Grammar.cpp"

struct Parser {
	using Location = Lexer::Location;
	using Token = Lexer::Token;

	using NodeRule = Grammar::NodeRule;
	using TokenRule = Grammar::TokenRule;
	using VariantRule = Grammar::VariantRule;
	using SequenceRule = Grammar::SequenceRule;
	using RuleRef = Grammar::RuleRef;
	using Rule = Grammar::Rule;

	struct Key {
		Rule rule;
		usize start;

		bool operator==(const Key& k) const = default;
	};

	struct Hasher {
		usize operator()(const Key& k) const {
			return hash<usize>()(k.rule.index())^hash<usize>()(k.start);
		}
	};

	enum class State {
		Raw,
		Working,
		Done
	};

	struct SignedValue {
		NodeValue value;
		usize order,
			  cleanTokens,
			  dirtyTokens;
	};

	struct Value {
		unordered_set<Key, Hasher> callers;
		State state = State::Raw;

		NodeValue value;
		usize length,
			  unsupported;  // Failed tokens count
	};

	using Cache = unordered_map<Key, Value, Hasher>;

	deque<Token> tokens;
	usize position = 0;
	Cache cache;
	deque<Key> calls,
			   queue;

	Parser(deque<Token> tokens) : tokens(filter(tokens, [](auto& t) { return !t.trivia; })) {}

	Token& token() {
		static Token dummy;

		return tokens.size() > position
			 ? tokens[position]
			 : dummy = Token();
	}

	bool tokensEnd() {
		return position >= tokens.size();
	}

	NodeSP parse(const NodeRule& nodeRule) {
		Node node;
		usize start = position;

		for(auto& field : nodeRule.fields) {
			NodeValue value = parse(field.rule);

			if(value.empty() && !field.optional) {
				return nullptr;
			}

			if(field.title && (!value.empty() || !field.optional)) {
				node[*field.title] = value;
			}
		}

		if(nodeRule.normalize) {
			if(node.empty()) {
				return nullptr;
			}
			if(node.size() == 1) {
				return node.begin()->second;
			}
		}

		node["range"] = {
			{"start", (int)start},
			{"end", (int)(position > 0 ? position-1 : 0)}
		};

		if(nodeRule.post) {
			nodeRule.post(node);
		}

		return SP(node);
	}

	NodeValue parse(const TokenRule& tokenRule) {
		Token& token = this->token();

		println("Token at position ", position, ": ", token.type, ", value: ", token.value);

		if(tokenRule.type) {
			try {
				regex r(*tokenRule.type);

				if(!regex_match(token.type, r)) {
					println("[Parser] Token type (", token.type, ") is not matching \"", *tokenRule.type, "\"");

					return nullptr;
				}
			} catch(const regex_error& e) {
				println("[Parser] Error in token type validator: ", e.what());

				return nullptr;
			}
		}
		if(tokenRule.value) {
			try {
				regex r(*tokenRule.value);

				if(!regex_match(token.value, r)) {
					println("[Parser] Token value (", token.value, ") is not matching \"", *tokenRule.value, "\"");

					return nullptr;
				}
			} catch(const regex_error& e) {
				println("[Parser] Error in token value validator: ", e.what());

				return nullptr;
			}
		}

		println("Position was: ", (int)position, ", set: ", ++position);

		return token.value;
	}

	NodeValue parse(const VariantRule& variantRule) {
		for(auto& rule : variantRule) {
			NodeValue value = parse(rule);

			if(!value.empty()) {
				return value;
			}
		}

		return nullptr;
	}

	bool parseDelimiters(const optional<Rule>& delimiterRule, char policy) {
		const string activePolicy = ".?+*",
					 maxOnePolicy = ".?",
					 minOnePolicy = ".+";

		if(!delimiterRule || !activePolicy.contains(policy)) {
			return true;
		}

		bool found = false;

		while(!tokensEnd()) {
			NodeValue value = parse(*delimiterRule);

			if(value.empty()) {
				break;
			}

			found = true;

			if(maxOnePolicy.contains(policy)) {
				break;
			}
		}

		if(!found && minOnePolicy.contains(policy)) {
			return false;
		}

		return true;
	}

	NodeValue parse(const SequenceRule& sequenceRule) {
		NodeArray values;

		if(sequenceRule.opener && parse(*sequenceRule.opener).empty()) {
			return nullptr;
		}

		if(!parseDelimiters(sequenceRule.delimiter, sequenceRule.outerDelimit)) {
			return nullptr;
		}

		usize end = position;
		bool found = false;

		while(!tokensEnd()) {
			NodeValue value = parse(sequenceRule.rule);

			if(value.empty()) {
				position = end;
				println("Position set 2: ", position);

				break;
			}

			values.push_back(value);

			found = true;
			end = position;

			if(!parseDelimiters(sequenceRule.delimiter, sequenceRule.innerDelimit)) {
				break;
			}
		}

		if(!found || !parseDelimiters(sequenceRule.delimiter, sequenceRule.outerDelimit)) {
			return nullptr;
		}

		if(sequenceRule.closer && parse(*sequenceRule.closer).empty()) {
			if(!tokensEnd()) {
				println("[Parser] Sequence doesn't have the closing token and was decided to be autoclosed at the end of stream");
			} else {
				return nullptr;
			}
		}

		if(sequenceRule.single) {
			if(values.size() == 1) {
				return values.front();
			} else {
				return nullptr;
			}
		}

		return values;
	}

	/*
	NodeValue parse(const SequenceRule& sequenceRule) {
		if(sequenceRule.opener && parse(*sequenceRule.opener).empty()) {
			return nullptr;
		}

		NodeArray values;
		NodeSP unsupportedNode;
		int scopeLevel = 0;

		while(!tokensEnd()) {
			NodeValue value = parse(sequenceRule.rule);

			if(!value.empty()) {
				values.push_back(value);
				unsupportedNode = nullptr;

				if(sequenceRule.single) {
					break;
				}
			}

			if(sequenceRule.opener && !parse(*sequenceRule.opener).empty()) {
				scopeLevel++;
			} else
			if(sequenceRule.closer && !parse(*sequenceRule.closer).empty()) {
				scopeLevel--;
			}

			if(scopeLevel == 0 || sequenceRule.single || tokensEnd()) {
				break;
			}

			if(value.empty()) {
				if(sequenceRule.unsupported) {
					if(!unsupportedNode) {
						unsupportedNode = SP<Node>({
							{"type", "unsupported"},
							{"range", {
								{"start", (int)position},
								{"end", (int)position}
							}},
							{"tokens", NodeArray {}}
						});
					}

					position++;
				} else {
					break;
				}
			}
		}

		if(sequenceRule.closer && parse(*sequenceRule.closer).empty()) {
			if(!tokensEnd()) {
				println("[Parser] Sequence doesn't have the closing token and was decided to be autoclosed at the end of stream");
			} else {
				return nullptr;
			}
		}

		if(sequenceRule.single) {
			if(values.empty()) {
				return nullptr;
			}
			if(values.size() == 1) {
				return values.front();
			}
		}

		return values;
	}
	*/

	NodeValue parse(const RuleRef& ruleRef) {
		println("[Parser] Rule \"", ruleRef, "\"");
		if(!Grammar::rules.contains(ruleRef)) {
			return nullptr;
		}

		Rule& rule = Grammar::rules[ruleRef];
		NodeValue value = parse(rule);

		if(value.type() == 5) {
			Node& nodeValue = value;

			if(!nodeValue.contains("type")) {
				nodeValue["type"] = ruleRef;
			}
		}

		println("[Parser] Rule \"", ruleRef, "\": ", to_string(value));

		return value;
	}

	NodeValue parseDispatch(const Rule& rule) {
		switch(rule.index()) {
			case 0:  return parse(get<0>(rule));
			case 1:  return parse(get<1>(rule).get());
			case 2:  return parse(get<2>(rule).get());
			case 3:  return parse(get<3>(rule).get());
			case 4:  return parse(get<4>(rule).get());
			default: return nullptr;
		}
	}

	NodeValue parse(const Rule& rule) {
		Key key(rule, position);
		Value& cacheValue = cache[key];

		if(!calls.empty()) {
			cacheValue.callers.insert(calls.back());
		}

		calls.push_back(key);

		switch(cacheValue.state) {
			case State::Raw:
				if(tokensEnd()) {
					println("[Parser] Reached the end of stream");

					return nullptr;
				}

				cacheValue.state = State::Working;

				if(!contains(queue, key)) {
					queue.push_back(key);
				}

				while(!queue.empty()) {
					Key queueKey = queue.front();
					Value& queueValue = cache[queueKey];

					queue.pop_front();
					position = queueKey.start;

					NodeValue queueValueValue = parseDispatch(queueKey.rule);
					usize queueValueLength = position-queueKey.start;

					if(queueValueLength > queueValue.length) {
						queueValue.value = queueValueValue;
						queueValue.length = queueValueLength;

						for(const Key& callerKey : queueValue.callers) {
							if(!contains(queue, callerKey)) {
								queue.push_back(callerKey);
							}
						}
					}

					queueValue.state = State::Done;
				}

				position = key.start+cacheValue.length;
			break;
			case State::Working:
				println("working");
			break;
			case State::Done:
				position = key.start+cacheValue.length;
			break;
		}

		calls.pop_back();

		println("Parsed rule at pos ", key.start, " => length: ", cacheValue.length, ", callers: ", cacheValue.callers.size(), ", value: ", to_string(cacheValue.value));

		return cacheValue.value;
	}

	NodeSP parse() {
		Interface::sendToClients({
			{"type", "notification"},
			{"source", "parser"},
			{"action", "removeAll"},
			{"moduleID", -1}
		});

		NodeSP tree = parse(RuleRef("module"));

		Interface::sendToClients({
			{"type", "notification"},
			{"source", "parser"},
			{"action", "parsed"},
			{"tree", tree}
		});

		return tree;
	}
};