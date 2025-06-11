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

	enum class State { Raw, Working, Done };

	struct Payload {
		NodeValue value;
		isize triviality,
			  cleanTokens,
			  dirtyTokens;

		isize size() const {
			return cleanTokens+dirtyTokens;
		}

		void addSize(const Payload& other) {
			cleanTokens += other.cleanTokens;
			dirtyTokens += other.dirtyTokens;
		}

		bool empty() const {
			return cleanTokens == 0 && (dirtyTokens > 0 || value.empty());
		}

		bool operator>(const Payload& other) const {
			if(empty() != other.empty())			return !empty();
			if(triviality != other.triviality)		return triviality < other.triviality;
			if(cleanTokens != other.cleanTokens)	return cleanTokens > other.cleanTokens;
			if(dirtyTokens != other.dirtyTokens)	return dirtyTokens < other.dirtyTokens;

			return false;
		}
	};

	struct Value {
		unordered_set<Key, Hasher> callers;
		State state = State::Raw;
		Payload payload;
	};

	using Cache = unordered_map<Key, Value, Hasher>;

	deque<Token> tokens;
	usize position = 0;
	Cache cache;
	deque<Key> calls,
			   queue;

	Parser(deque<Token> tokens) : tokens(filter(tokens, [](auto& t) { return !t.trivia; })) {}

	// ----------------------------------------------------------------

	Token& token() {
		static Token dummy;

		return tokens.size() > position
			 ? tokens[position]
			 : dummy = Token();
	}

	bool tokensEnd() {
		return position >= tokens.size();
	}

	Payload parse(const NodeRule& nodeRule) {
		Payload payload = { Node() };
		Node& node = payload.value;
		usize start = position;

		for(auto& field : nodeRule.fields) {
			Payload fieldPayload = parse(field.rule);

			if(fieldPayload.empty() && !field.optional) {
				return Payload();
			}

			payload.addSize(fieldPayload);

			if(field.title && (!fieldPayload.empty() || !field.optional)) {
				node[*field.title] = fieldPayload.value;
			}
		}

		if(nodeRule.normalize && node.size() <= 1) {
			if(node.empty()) {
				return Payload();
			}

			payload.value = node.begin()->second;

			return payload;
		}

		node["range"] = {
			{"start", (int)start},
			{"end", (int)(start+payload.size())}
		};

		if(nodeRule.post) {
			nodeRule.post(node);
		}

		return payload;
	}

	Payload parse(const TokenRule& tokenRule) {
		Token& token = this->token();
		Payload payload = { token.value };

		println("Token at position ", position, ": ", token.type, ", value: ", token.value);

		if(tokenRule.type) {
			try {
				regex r(*tokenRule.type);

				if(!regex_match(token.type, r)) {
					println("[Parser] Token type (", token.type, ") is not matching \"", *tokenRule.type, "\"");
					payload.dirtyTokens++;

					return payload;
				}
			} catch(const regex_error& e) {
				println("[Parser] Error in token type validator: ", e.what());
				payload.dirtyTokens++;

				return payload;
			}
		}
		if(tokenRule.value) {
			try {
				regex r(*tokenRule.value);

				if(!regex_match(token.value, r)) {
					println("[Parser] Token value (", token.value, ") is not matching \"", *tokenRule.value, "\"");
					payload.dirtyTokens++;

					return payload;
				}
			} catch(const regex_error& e) {
				println("[Parser] Error in token value validator: ", e.what());
				payload.dirtyTokens++;

				return payload;
			}
		}

		payload.cleanTokens++;

		return payload;
	}

	Payload parse(const VariantRule& variantRule) {
		for(int i = 0; i < variantRule.size(); i++) {
			Payload payload = parse(variantRule[i]);

			if(!payload.value.empty()) {
				payload.triviality = i;

				return payload;
			}
		}

		return Payload();
	}

	Payload parseDelimiters(const optional<Rule>& delimiterRule, char policy) {
		Payload payload = { NodeArray() };
		NodeArray& values = payload.value;
		const string activePolicy = ".?+*",
					 maxOnePolicy = ".?",
					 minOnePolicy = ".+";

		if(!delimiterRule || !activePolicy.contains(policy)) {
			return payload;
		}

		bool found = false;

		while(!tokensEnd()) {
			Payload delimiterPayload = parse(*delimiterRule);

			if(delimiterPayload.empty()) {
				position -= delimiterPayload.size();

				break;
			} else {
				payload.addSize(delimiterPayload);
				values.push_back(delimiterPayload.value);
			}

			found = true;

			if(maxOnePolicy.contains(policy)) {
				break;
			}
		}

		if(!found && minOnePolicy.contains(policy)) {
			return Payload();
		}

		return payload;
	}

	Payload parse(const SequenceRule& sequenceRule) {
		Payload payload = { NodeArray() };
		NodeArray& values = payload.value;

		/*
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
		*/

		return payload;
	}

	Payload parse(const RuleRef& ruleRef) {
		println("[Parser] Rule \"", ruleRef, "\"");
		if(!Grammar::rules.contains(ruleRef)) {
			return Payload();
		}

		Rule& rule = Grammar::rules[ruleRef];
		Payload payload = parse(rule);

		if(payload.value.type() == 5) {
			Node& node = payload.value;

			if(!node.contains("type")) {
				node["type"] = ruleRef;
			}
		}

		println("[Parser] Rule \"", ruleRef, "\": ", to_string(payload.value));

		return payload;
	}

	Payload parseDispatch(const Rule& rule) {
		switch(rule.index()) {
			case 0:  return parse(get<0>(rule));
			case 1:  return parse(get<1>(rule).get());
			case 2:  return parse(get<2>(rule).get());
			case 3:  return parse(get<3>(rule).get());
			case 4:  return parse(get<4>(rule).get());
			default: return Payload();
		}
	}

	Payload parse(const Rule& rule) {
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

					return Payload();
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

					Payload payload = parseDispatch(queueKey.rule);

					if(payload > queueValue.payload) {
						queueValue.payload = payload;

						for(const Key& callerKey : queueValue.callers) {
							if(!contains(queue, callerKey)) {
								queue.push_back(callerKey);
							}
						}
					}

					queueValue.state = State::Done;
				}

				position = key.start+cacheValue.payload.size();
			break;
			case State::Working:
				if(!cacheValue.payload.empty()) {
					position = key.start+cacheValue.payload.size();
				}
			break;
			case State::Done:
				position = key.start+cacheValue.payload.size();
			break;
		}

		calls.pop_back();

		println("Parsed rule at pos ", key.start, " => clean: ", cacheValue.payload.cleanTokens, ", dirty: ", cacheValue.payload.dirtyTokens, ", callers: ", cacheValue.callers.size(), ", value: ", to_string(cacheValue.payload.value));

		return cacheValue.payload;
	}

	NodeSP parse() {
		Interface::sendToClients({
			{"type", "notification"},
			{"source", "parser"},
			{"action", "removeAll"},
			{"moduleID", -1}
		});

		NodeSP tree = parse(RuleRef("module")).value;

		Interface::sendToClients({
			{"type", "notification"},
			{"source", "parser"},
			{"action", "parsed"},
			{"tree", tree}
		});

		return tree;
	}
};