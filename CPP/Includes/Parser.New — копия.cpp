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
		Frame(const Rule& rule, usize start = 0, bool permitsDirt = false) : key(Key(rule, start)), permitsDirt(permitsDirt) {}

		Key key;
		NodeValue value;
		usize cleanTokens = 0,
			  dirtyTokens = 0,
			  applications = 0;
		bool permitsDirt = false,  // Allow the frame parser to add a dirt into the value
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
			cleanTokens = 0;
			dirtyTokens = 0;
		}

		void apply(const Frame& other) {
			value = other.value;
			cleanTokens = other.cleanTokens;
			dirtyTokens = other.dirtyTokens;
			permitsDirt = other.permitsDirt;
		}

		bool empty() const {
			return value.empty() || cleanTokens == 0 && (!permitsDirt || dirtyTokens == 0);
		}

		bool greater(const Frame& other) const {
			if(empty() != other.empty())			return !empty();
			if(cleanTokens != other.cleanTokens)	return cleanTokens > other.cleanTokens;
			if(dirtyTokens != other.dirtyTokens)	return dirtyTokens < other.dirtyTokens;

			return false;
		}
	};

	deque<Token> tokens;
	unordered_map<Key, Frame, Hasher> cache;
	usize titledCalls = 0;

	Parser(deque<Token> tokens) : tokens(filter(tokens, [](auto& t) { return !t.trivia; })) {}

	// ----------------------------------------------------------------

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

		println("Token at position ", position, ": ", token.type, ", value: ", token.value);

		if(tokenRule.type) {
			try {
				regex r(*tokenRule.type);

				if(!regex_match(token.type, r)) {
					println("[Parser] Token type (", token.type, ") is not matching \"", *tokenRule.type, "\"");
					if(tokenFrame.permitsDirt) {
						tokenFrame.value = token.value;
						tokenFrame.dirtyTokens++;
					}

					return tokenFrame;
				}
			} catch(const regex_error& e) {
				println("[Parser] Error in token type validator: ", e.what());
				if(tokenFrame.permitsDirt) {
					tokenFrame.value = token.value;
					tokenFrame.dirtyTokens++;
				}

				return tokenFrame;
			}
		}
		if(tokenRule.value) {
			try {
				regex r(*tokenRule.value);

				if(!regex_match(token.value, r)) {
					println("[Parser] Token value (", token.value, ") is not matching \"", *tokenRule.value, "\"");
					if(tokenFrame.permitsDirt) {
						tokenFrame.value = token.value;
						tokenFrame.dirtyTokens++;
					}

					return tokenFrame;
				}
			} catch(const regex_error& e) {
				println("[Parser] Error in token value validator: ", e.what());
				if(tokenFrame.permitsDirt) {
					tokenFrame.value = token.value;
					tokenFrame.dirtyTokens++;
				}

				return tokenFrame;
			}
		}

		tokenFrame.value = token.value;
		tokenFrame.cleanTokens++;

		return tokenFrame;
	}

	optional<Frame> parseVariant(Frame& variantFrame) {
		VariantRule& variantRule = get<3>(variantFrame.key.rule).get();
		usize position = variantFrame.start();
		optional<Frame> greatestFrame;

		for(Rule& rule : variantRule) {
			Frame& ruleFrame = parse({rule, position});

			if(!greatestFrame || ruleFrame.greater(*greatestFrame)) {
				greatestFrame = ruleFrame;
			}
		}

		return greatestFrame;
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

					if(!sequenceFrame.permitsDirt) {
						break;
					}

					if(sequenceRule.ascender) {
						Frame& openerFrame = parse({*sequenceRule.ascender, sequenceFrame.end()});

						if(!openerFrame.empty()) {
							scopeLevel++;
							sequenceFrame.addSize(openerFrame);

							continue;
						}
					}
					if(sequenceRule.descender) {
						Frame& closerFrame = parse({*sequenceRule.descender, sequenceFrame.end()});

						if(!closerFrame.empty()) {
							scopeLevel--;

							if(scopeLevel > 0) {
								sequenceFrame.addSize(closerFrame);

								continue;
							} else {
								break;
							}
						}
					}

					Frame& tokenFrame = parse({TokenRule(), sequenceFrame.end(), true});

					sequenceFrame.addSize(tokenFrame);

					continue;
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

		sequenceFrame.permitsDirt = sequenceRule.descender.has_value();

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
			case 1:  return parseNode(frame);
			case 2:  return parseToken(frame);
			case 3:  return parseVariant(frame);
			case 4:  return parseSequence(frame);
			default: return nullopt;
		}
	}

	Frame& parse(const Frame& templateFrame) {
		if(templateFrame.key.rule.index() == 0) {
			const RuleRef& ruleRef = get<0>(templateFrame.key.rule);

			if(!Grammar::rules.contains(ruleRef)) {
				return cache[{ruleRef, templateFrame.start()}];
			}

			Rule& rule = Grammar::rules[ruleRef];
			Key key = {rule, templateFrame.start()};
			Frame& ruleFrame = parse({rule, templateFrame.start()});

			if(ruleFrame.value.type() == 5) {
				Node& node = ruleFrame.value;

				if(!node.contains("type")) {
					node["type"] = ruleRef;
				}
			}

			return ruleFrame;
		}

		Frame& frame = cache[templateFrame.key];

		if(!frame.isInitialized) {
			frame.key = templateFrame.key;
			frame.isInitialized = true;
			frame.apply(templateFrame);

			string title = frame.key.rule.index() == 0 ? get<0>(frame.key.rule) : "";
			usize pass = frame.applications;

			if(!title.empty()) {
				println("[Parser] # Start ", repeat("| ", titledCalls++), title, " at ", frame.key.start, ", pass ", pass);
			}

			while(optional<Frame> newFrame = dispatch(frame)) {
				if(!newFrame->greater(frame)) {
					break;
				}

				frame.apply(*newFrame);
				frame.applications++;

				Interface::sendToClients({
					{"type", "notification"},
					{"source", "parser"},
					{"action", "parsed"},
					{"tree", frame.value}
				});
			}

			if(!title.empty()) {
				println("[Parser] #   End ", repeat("| ", --titledCalls), title, " at ", frame.key.start, ", pass ", pass, " => clean: ", frame.cleanTokens, ", dirty: ", frame.dirtyTokens, ", applications: ", frame.applications, ", value: ", to_string(frame.value));
			}
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