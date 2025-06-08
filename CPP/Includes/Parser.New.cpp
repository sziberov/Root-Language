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
		usize length,
			  unsupported;  // Failed tokens count
		u8 state = 0;  // 0 - Raw, 1 - In progress, 2 - Finished
		bool recursive = false;
		string title;
		usize limit;
		bool seeded = false;
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
	vector<CacheKey> path;

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

	CacheValue& cacheValue() {
		static CacheValue dummy;

		return !path.empty()
			 ? cache[path.back()]
			 : dummy = CacheValue();
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

		println("Position was: ", position, ", set: ", ++position);

		return token.value;
	}

	NodeValue parse(const VariantRule& variantRule) {
		usize start = position;
		CacheValue& cacheValue = this->cacheValue();
		vector<CacheKey> defferedCacheKeys;

		for(auto& rule : variantRule) {
			cacheValue.value = parse(rule);

			if(!cacheValue.value.empty()) {
				break;
			}

			CacheKey cacheKey(rule, start);

			if(cache[cacheKey].recursive) {
				defferedCacheKeys.push_back(cacheKey);
			}
		}

		if(cacheValue.value.empty()) {
			return nullptr;
		}

		cacheValue.seeded = true;

		for(auto& defferedCacheKey : defferedCacheKeys) {
			CacheValue& defferedCacheValue = cache[defferedCacheKey];
			NodeValue value;
			usize end = position;

			while(true) {
				println("Parsing deffered left recursion: ");
				position = start;
				println("Position set 3: ", position);
				parse(defferedCacheKey.rule);
				println("Parsed value from deferred alt: len=", cache[defferedCacheKey].length, ", pos=", position);

				usize previousLength = cacheValue.length-cacheValue.unsupported,
					  currentLength = defferedCacheValue.length-defferedCacheValue.unsupported;

				if(currentLength <= previousLength) {
					println("not improved");
					break;
				}

				println("improved");
				value =
				cacheValue.value = defferedCacheValue.value;
				cacheValue.length = defferedCacheValue.length;
				cacheValue.unsupported = defferedCacheValue.unsupported;
			}

			if(!value.empty()) {
				break;
			}

			position = end;
		}

		return cacheValue.value;
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

		cacheValue().title = ruleRef;
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

	NodeValue parse(const Rule& rule) {
		if(tokensEnd()) {
			println("[Parser] Reached the end of stream");

			return nullptr;
		}

		CacheKey cacheKey(rule, position);
		CacheValue& cacheValue = cache[cacheKey];

		bool inGrowPass = false;

		for(auto it = path.rbegin(); it != path.rend(); it++) {
			if(cache[*it].seeded) {
				inGrowPass = true;

				break;
			}
		}

		cacheValue.limit++;

		println("In grow pass (", cacheValue.title, "): ", inGrowPass ? "true" : "false");

		if(cacheValue.state >= 1 && !inGrowPass || cacheValue.limit >= 50) {
			if(cacheValue.limit >= 50) {
				println("Limit exceeded");
			}
			if(cacheValue.state == 2) {
				if(position > cacheValue.length) {
					println("[Parser] Position was rollen back while should not");
				}

				position = cacheKey.start+cacheValue.length;
				println("Position set 0: ", position);
			}

			if(!cacheValue.recursive) {
				println("Marking as recursive");
				for(auto it = path.rbegin(); it != path.rend(); it++) {
					cache[*it].recursive = true;

					if(*it == cacheKey) {
						break;
					}
				}
			} else {
				println("Re:Marking as recursive");
			}

			return cacheValue.value;
		}

		cacheValue.state = 1;  // Register before parsing to avoid left-recursion cycles

		path.push_back(cacheKey);

		println("Path size: ", path.size(), ", position: ", position);

		usize start = position;

		switch(rule.index()) {
			case 0: cacheValue.value = parse(get<0>(rule));       break;
			case 1: cacheValue.value = parse(get<1>(rule).get()); break;
			case 2: cacheValue.value = parse(get<2>(rule).get()); break;
			case 3: cacheValue.value = parse(get<3>(rule).get()); break;
			case 4: cacheValue.value = parse(get<4>(rule).get()); break;
		}

		if(cacheValue.value.empty()) {
			position = start;
			println("Position set 1: ", position);
			cacheValue.length = 0;
		} else {
			cacheValue.length = position-start;
		}

		cacheValue.state = 2;

		path.pop_back();

		println("Parsed rule at pos ", cacheKey.start, " => length: ", cacheValue.length, ", recursive: ", cacheValue.recursive ? "true" : "false", ", value.empty: ", cacheValue.value.empty() ? "true" : "false");

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