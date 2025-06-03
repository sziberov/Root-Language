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

	struct CacheKey {
		Rule rule;
		usize start;

		bool operator==(const CacheKey& k) const = default;
	};

	struct CacheValue {
		NodeValue value;
		usize end;
	};

	struct CacheHasher {
		usize operator()(const CacheKey& k) const {
			return hash<usize>()(k.rule.index())^hash<usize>()(k.start);
		}
	};

	using Cache = unordered_map<CacheKey, CacheValue, CacheHasher>;

	deque<Token> tokens;
	usize position = 0;
	Cache cache;  // Memoization support

	Parser(deque<Token> tokens) : tokens(filter(tokens, [](auto& t) { return !t.trivia; })) {}

	Token& token() {
		static Token dummy;

		return tokens.size() > position
			 ? tokens[position]
			 : dummy = Token();
	}

	NodeSP parse(const NodeRule& nodeRule) {
		Node node;
		usize start = position;

		for(auto& field : nodeRule.fields) {
			if(field.title && *field.title == "value") {
				println("checking value");
			}

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

		position++;

		return token.value;
	}

	NodeValue parse(const VariantRule& variantRule) {
		for(auto& rule : variantRule) {
			NodeValue& value = parseBranch(rule);

			if(!value.empty()) {
				return value;
			}
		}

		return nullptr;
	}

	NodeValue parse(const SequenceRule& sequenceRule) {
		return nullptr; /* TODO */
	}

	NodeValue parse(const RuleRef& ruleRef) {
		println("ruleRef ", ruleRef);
		if(!Grammar::rules.contains(ruleRef)) {
			return nullptr;
		}

		Rule& rule = Grammar::rules[ruleRef];
		NodeValue value = parse(rule);
		NodeSP nodeValue = value;

		if(nodeValue && !nodeValue->contains("type")) {
			(*nodeValue)["type"] = ruleRef;
		}

		return value;
	}

	NodeValue parse(const Rule& rule) {
		if(position >= tokens.size()) {
			println("Parsing past the end of stream...");
		}

		switch(rule.index()) {
			case 0:  return parse(get<0>(rule));
			case 1:  return parse(get<1>(rule).get());
			case 2:  return parse(get<2>(rule).get());
			case 3:  return parse(get<3>(rule).get());
			case 4:  return parse(get<4>(rule).get());
		}

		return nullptr;
	}

	/**
	 * Important for alternatives parsing and left-recursion cycles avoidance.
	 */
	NodeValue& parseBranch(const Rule& rule) {
		CacheKey cacheKey(rule, position);

		if(cache.contains(cacheKey)) {
			CacheValue& cacheValue = cache[cacheKey];

			if(!cacheValue.value.empty()) {
				if(position > cacheValue.end) {
					println("[Parser] Position was rollen back while should not");
				}

				position = cacheValue.end;
			}

			return cacheValue.value;
		}

		CacheValue& cacheValue = cache[cacheKey];  // Register before parsing
		usize start = position;

		cacheValue.value = parse(rule);

		if(cacheValue.value.empty()) {
			position = start;
		} else {
			cacheValue.end = position;
		}

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