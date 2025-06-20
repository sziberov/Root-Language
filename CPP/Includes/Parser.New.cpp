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
		usize start = 0;

		bool operator==(const Key& k) const = default;
	};

	struct Hasher {
		usize operator()(const Key& k) const {
			return hash<usize>()(k.rule.index())^hash<usize>()(k.start);
		}
	};

	struct Frame {
		Frame() {}
		Frame(const Rule& rule,
			  usize start = 0,
			  usize triviality = 0,
			  bool permitsDirty = false) : key(Key(rule, start)),
			  							   triviality(triviality),
			  							   permitsDirty(permitsDirty) {}

		Key key;
		unordered_set<Key, Hasher> callers;
		NodeValue value;
		usize triviality = 0,
			  cleanTokens = 0,
			  dirtyTokens = 0;
		bool permitsDirty = false,  // Is not same as "contains" (dirtyTokens > 0)
			 isInitialized = false;

		usize size() const {
			return cleanTokens+dirtyTokens;
		}

		void addSize(const Frame& other) {
			cleanTokens += other.cleanTokens;
			dirtyTokens += other.dirtyTokens;
		}

		void removeSize(const Frame& other) {
			if(other.cleanTokens > cleanTokens) {
				cleanTokens = 0;
			} else {
				cleanTokens -= other.cleanTokens;
			}

			if(other.dirtyTokens > dirtyTokens) {
				dirtyTokens = 0;
			} else {
				dirtyTokens -= other.dirtyTokens;
			}
		}

		usize start() const {
			return key.start;
		}

		usize end() const {
			return start()+size();
		}

		// Should be used for non-proxy rules with own values and accumulative sizes before parsing.
		// Rules that do just _set_ their value/size behave fine without that, but other can unintentionally mislead the "greater" check.
		void clearResult() {
			value = nullptr;
			cleanTokens =
			dirtyTokens = 0;
		}

		void apply(const Frame& other) {
			value = other.value;
			triviality = other.triviality;
			cleanTokens = other.cleanTokens;
			dirtyTokens = other.dirtyTokens;
			permitsDirty = other.permitsDirty;
		}

		bool empty() const {
			return value.empty() || cleanTokens == 0 && (!permitsDirty || dirtyTokens == 0);
		}

		bool greater(const Frame& other) const {
			if(empty() != other.empty())			return !empty();
			if(triviality != other.triviality)		return triviality < other.triviality;
			if(cleanTokens != other.cleanTokens)	return cleanTokens > other.cleanTokens;
			if(dirtyTokens != other.dirtyTokens)	return dirtyTokens < other.dirtyTokens;

			return false;
		}
	};

	deque<Token> tokens;
	unordered_map<Key, Frame, Hasher> cache;
	deque<Key> calls,
			   queue;

	Parser(deque<Token> tokens) : tokens(filter(tokens, [](auto& t) { return !t.trivia; })) {}

	// ----------------------------------------------------------------

	optional<Frame> parseReference(Frame& refFrame) {
		RuleRef& ruleRef = get<0>(refFrame.key.rule);

		println("[Parser] Rule \"", ruleRef, "\"");
		if(!Grammar::rules.contains(ruleRef)) {
			return nullopt;
		}

		Rule& rule = Grammar::rules[ruleRef];
		Frame& ruleFrame = parse({rule, refFrame.start()});

		if(ruleFrame.value.type() == 5) {
			Node& node = ruleFrame.value;

			if(!node.contains("type")) {
				node["type"] = ruleRef;
			}
		}

		println("[Parser] Rule \"", ruleRef, "\": ", to_string(ruleFrame.value));

		return ruleFrame;
	}

	optional<Frame> parseNode(Frame& nodeFrame) {
		NodeRule& nodeRule = get<1>(nodeFrame.key.rule).get();
		Node& node = nodeFrame.value = Node();

		for(auto& field : nodeRule.fields) {
			Frame& fieldFrame = parse({field.rule, nodeFrame.end()});

			if(fieldFrame.empty() && !field.optional) {
				return nullopt;
			}

			nodeFrame.addSize(fieldFrame);

			if(field.title && (!fieldFrame.empty() || !field.optional)) {
				node[*field.title] = fieldFrame.value;
			}
		}

		if(nodeRule.normalize && node.size() <= 1) {
			if(node.empty()) {
				return nullopt;
			}

			nodeFrame.value = node.begin()->second;

			return nodeFrame;
		}

		int start = nodeFrame.start(),
			end = start+max(usize(), nodeFrame.size() ? nodeFrame.size()-1 : 0);  // TODO: Check if max is needed

		node["range"] = {
			{"start", start},
			{"end", end}
		};

		if(nodeRule.post) {
			nodeRule.post(node);
		}

		return nodeFrame;
	}

	optional<Frame> parseToken(Frame& tokenFrame) {
		TokenRule& tokenRule = get<2>(tokenFrame.key.rule).get();
		static Token endOfFileToken = { .type = "endOfFile" };
		usize position = tokenFrame.start();
		Token& token = position < tokens.size() ? tokens[position] : endOfFileToken;

		tokenFrame.value = token.value;

		println("Token at position ", position, ": ", token.type, ", value: ", token.value);

		if(tokenRule.type) {
			try {
				regex r(*tokenRule.type);

				if(!regex_match(token.type, r)) {
					println("[Parser] Token type (", token.type, ") is not matching \"", *tokenRule.type, "\"");
					tokenFrame.dirtyTokens++;

					return tokenFrame;
				}
			} catch(const regex_error& e) {
				println("[Parser] Error in token type validator: ", e.what());
				tokenFrame.dirtyTokens++;

				return tokenFrame;
			}
		}
		if(tokenRule.value) {
			try {
				regex r(*tokenRule.value);

				if(!regex_match(token.value, r)) {
					println("[Parser] Token value (", token.value, ") is not matching \"", *tokenRule.value, "\"");
					tokenFrame.dirtyTokens++;

					return tokenFrame;
				}
			} catch(const regex_error& e) {
				println("[Parser] Error in token value validator: ", e.what());
				tokenFrame.dirtyTokens++;

				return tokenFrame;
			}
		}

		tokenFrame.cleanTokens++;
	//	tokenFrame.cleanTokens += position < tokens.size();  // EOF token is virtual and therefore implicit

		return tokenFrame;
	}

	optional<Frame> parseVariant(Frame& variantFrame) {
		VariantRule& variantRule = get<3>(variantFrame.key.rule).get();
		usize position = variantFrame.start();

		for(usize i = 0; i < variantRule.size(); i++) {
			Frame& ruleFrame = parse({variantRule[i], position, i});

			if(!ruleFrame.empty()) {
				return ruleFrame;
			}
		}

		return nullopt;
	}

	void rollbackSequenceFrames(Frame& sequenceFrame, vector<Frame>& frames, usize fromIndex) {
		for(usize i = frames.size(); i > fromIndex; i--) {
			Frame& frame = frames.back();

			sequenceFrame.removeSize(frame);
			frames.pop_back();
		}
	}

	bool parseSubsequence(Frame& sequenceFrame, vector<Frame>& frames, usize range[2], u8 mode) {
		SequenceRule& sequenceRule = get<4>(sequenceFrame.key.rule).get();
		optional<Rule> rule;

		if(mode == 0) {
			rule = sequenceRule.rule;

			if(!parseSubsequence(sequenceFrame, frames, sequenceRule.outerDelimitRange, 1)) {
				return false;
			}
		} else
		if(mode == 1) {
			if(!sequenceRule.delimiter) {
				return true;
			}

			rule = *sequenceRule.delimiter;
		}

		usize count = 0,
			  minCount = range[0],
			  maxCount = range[1],
			  framesSize = frames.size(),  // After last successful rule / before last delimiter(s)
			  scopeLevel = 1;

		while(sequenceFrame.end() < tokens.size() && count < maxCount) {
			Frame& frame = parse({*rule, sequenceFrame.end()});

			if(frame.empty()) {
				if(mode == 0) {
					rollbackSequenceFrames(sequenceFrame, frames, framesSize);

					/*
					if(sequenceRule.ascender) {
						Frame& openerFrame = parse({*sequenceRule.ascender, sequenceFrame.end()});

						if(!openerFrame.empty()) {
							sequenceFrame.addSize(openerFrame);
							scopeLevel++;

							continue;
						}
					}
					if(sequenceRule.descender) {
						Frame& closerFrame = parse({*sequenceRule.descender, sequenceFrame.end()});

						if(!closerFrame.empty()) {
							sequenceFrame.addSize(closerFrame);
							scopeLevel--;
						} else
						if(sequenceFrame.end() < tokens.size()) {
							sequenceFrame.dirtyTokens++;
						}

						if(scopeLevel > 0) {
							continue;
						} else {
							break;
						}
					}
					*/
				}

				break;
			}

			sequenceFrame.addSize(frame);
			count++;

			if(mode == 0 || sequenceRule.delimited) {
				frames.push_back(frame);
			}
			if(mode == 0) {
				framesSize = frames.size();

				if(!parseSubsequence(sequenceFrame, frames, sequenceRule.innerDelimitRange, 1)) {
					break;
				}
			}
		}

		if(count < minCount) {
			if(mode == 1) {
				rollbackSequenceFrames(sequenceFrame, frames, framesSize);
			}

			return false;
		}

		if(mode == 0) {
			if(!parseSubsequence(sequenceFrame, frames, sequenceRule.outerDelimitRange, 1)) {
				return false;
			}
		}

		return true;
	}

	optional<Frame> parseSequence(Frame& sequenceFrame) {
		SequenceRule& sequenceRule = get<4>(sequenceFrame.key.rule).get();
		vector<Frame> frames;

		sequenceFrame.permitsDirty = sequenceRule.descender.has_value();

		if(!parseSubsequence(sequenceFrame, frames, sequenceRule.range, 0)) {
			return nullopt;
		}

		if(sequenceRule.normalize && frames.size() <= 1) {
			if(frames.empty()) {
				return nullopt;
			}

			sequenceFrame.value = frames.front().value;

			return sequenceFrame;
		}

		NodeArray& values = sequenceFrame.value = NodeArray();

		for(Frame& f : frames) {
			values.push_back(f.value);
		}

		return sequenceFrame;
	}

	optional<Frame> dispatch(Frame frame) {
		frame.clearResult();

		switch(frame.key.rule.index()) {
			case 0:  return parseReference(frame);
			case 1:  return parseNode(frame);
			case 2:  return parseToken(frame);
			case 3:  return parseVariant(frame);
			case 4:  return parseSequence(frame);
			default: return nullopt;
		}
	}

	void flush() {
		while(!queue.empty()) {
			Key key = queue.front();
			Frame& frame = cache[key];

			queue.pop_front();

			optional<Frame> newFrame = dispatch(frame);

			if(newFrame && newFrame->greater(frame)) {
				frame.apply(*newFrame);

				for(const Key& callerKey : frame.callers) {
					queue.push_back(callerKey);
				}
			}
		}
	}

	Frame& parse(const Frame& templateFrame) {
		Frame& frame = cache[templateFrame.key];

		if(!calls.empty()) {
			frame.callers.insert(calls.back());
		}

		if(!frame.isInitialized) {
			frame.key = templateFrame.key;
			frame.isInitialized = true;
			frame.apply(templateFrame);

			if(frame.start() >= tokens.size()) {
				println("[Parser] Reached the end of file");

				return frame;
			}

			calls.push_back(frame.key);
			queue.push_back(frame.key);
			flush();
			calls.pop_back();

			println("[Parser] Result at pos ", frame.key.start, " => clean: ", frame.cleanTokens, ", dirty: ", frame.dirtyTokens, ", callers: ", frame.callers.size(), ", value: ", to_string(frame.value));
		}

		return frame;
	}

	NodeSP parse() {
		Interface::sendToClients({
			{"type", "notification"},
			{"source", "parser"},
			{"action", "removeAll"},
			{"moduleID", -1}
		});

		NodeSP tree = parse({"module"}).value;

		Interface::sendToClients({
			{"type", "notification"},
			{"source", "parser"},
			{"action", "parsed"},
			{"tree", tree}
		});

		return tree;
	}
};