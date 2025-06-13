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
		bool initialized = false;
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
			{"start", int(start)},
			{"end", int(start+(max(isize(), payload.size()-1)))}
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

	bool parseDelimiters(Payload& payload, const optional<Rule>& delimiterRule, char policy, bool delimited) {
		const string activePolicy = ".?+*",
					 maxOnePolicy = ".?",
					 minOnePolicy = ".+";

		if(!delimiterRule || !activePolicy.contains(policy)) {
			return true;
		}

		NodeArray& values = payload.value;
		bool found = false;
		int i = 0;

		while(!tokensEnd()) {
			println("cycle 0: ", i++);
			Payload& delimiterPayload = parse(*delimiterRule);

			if(delimiterPayload.empty()) {
				break;
			}

			payload.addSize(delimiterPayload);

			if(delimited) {
				values.push_back(delimiterPayload.value);
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

	Payload parse(const SequenceRule& sequenceRule) {
		Payload payload = { NodeArray() };
		NodeArray& values = payload.value;

		if(sequenceRule.opener) {
			Payload& openerPayload = parse(*sequenceRule.opener);

			if(openerPayload.empty()) {
				return Payload();
			}

			payload.addSize(openerPayload);
		}

		if(!parseDelimiters(payload, sequenceRule.delimiter, sequenceRule.outerDelimit, sequenceRule.delimited)) {
			return Payload();
		}

		usize end = position;
		bool found = false;
		int i = 0;

		while(!tokensEnd()) {
			println("cycle 1: ", i++);
			Payload& rulePayload = parse(sequenceRule.rule);

			if(rulePayload.empty()) {
				position = end;
				println("Position set 2: ", position);

				break;
			}

			payload.addSize(rulePayload);
			values.push_back(rulePayload.value);

			found = true;
			end = position;

			if(!parseDelimiters(payload, sequenceRule.delimiter, sequenceRule.innerDelimit, sequenceRule.delimited)) {
				break;
			}
		}

		if(!found || !parseDelimiters(payload, sequenceRule.delimiter, sequenceRule.outerDelimit, sequenceRule.delimited)) {
			return Payload();
		}

		if(sequenceRule.closer) {
			Payload& closerPayload = parse(*sequenceRule.closer);

			if(closerPayload.empty()) {
				if(tokensEnd()) {
					println("[Parser] Sequence doesn't have the closing token and was decided to be autoclosed at the end of stream");
				} else {
					return Payload();
				}
			}

			payload.addSize(closerPayload);
		}

		if(sequenceRule.single) {
			if(values.size() == 1) {
				payload.value = values.front();

				return payload;
			} else {
				return Payload();
			}
		}

		return payload;
	}

	Payload parse(const RuleRef& ruleRef) {
		println("[Parser] Rule \"", ruleRef, "\"");
		if(!Grammar::rules.contains(ruleRef)) {
			return Payload();
		}

		Rule& rule = Grammar::rules[ruleRef];
		Payload& payload = parse(rule);

		if(payload.value.type() == 5) {
			Node& node = payload.value;

			if(!node.contains("type")) {
				node["type"] = ruleRef;
			}
		}

		println("[Parser] Rule \"", ruleRef, "\": ", to_string(payload.value));

		return payload;
	}

	Payload dispatch(const Rule& rule) {
		switch(rule.index()) {
			case 0:  return parse(get<0>(rule));
			case 1:  return parse(get<1>(rule).get());
			case 2:  return parse(get<2>(rule).get());
			case 3:  return parse(get<3>(rule).get());
			case 4:  return parse(get<4>(rule).get());
			default: return Payload();
		}
	}

	void flush() {
		while(!queue.empty()) {
			Key key = queue.front();
			Value& value = cache[key];

			queue.pop_front();
			position = key.start;

			Payload payload = dispatch(key.rule);

			if(payload > value.payload) {
				value.payload = payload;

				for(const Key& callerKey : value.callers) {
					queue.push_back(callerKey);
				}
			}
		}
	}

	Payload& parse(const Rule& rule) {
		Key key(rule, position);
		Value& value = cache[key];
		Payload& payload = value.payload;

		if(!calls.empty()) {
			value.callers.insert(calls.back());
		}

		if(!value.initialized) {
			value.initialized = true;

			if(tokensEnd()) {
				println("[Parser] Reached the end of stream");

				return payload;
			}

			calls.push_back(key);
			queue.push_back(key);
			flush();
			calls.pop_back();

			println("[Parser] Result at pos ", key.start, " => clean: ", payload.cleanTokens, ", dirty: ", payload.dirtyTokens, ", callers: ", value.callers.size(), ", value: ", to_string(payload.value));
		}

		position = key.start+payload.size();

		return payload;
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