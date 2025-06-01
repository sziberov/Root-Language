#pragma once

#include "Lexer.cpp";
#include "Grammar.cpp";

struct Parser;

using ParserSP = sp<Parser>;

struct Parser /*: enable_shared_from_this<Parser>*/ {
	using Location = Lexer::Location;
	using Token = Lexer::Token;

	using NodeRule = Grammar::NodeRule;
	using TokenRule = Grammar::TokenRule;
	using VariantRule = Grammar::VariantRule;
	using SequenceRule = Grammar::SequenceRule;
	using RuleRef = Grammar::RuleRef;
	using Rule = Grammar::Rule;

	using CacheKey = pair<Rule, usize>;  // Rule, Position
	using CacheValue = pair<NodeValue, bool>;  // NodeValue, Finished?

	struct CacheHasher {
		usize operator()(const CacheKey& k) const {
			return hash<usize>()(k.first.index())^hash<usize>()(k.second);
		}
	};

	using Cache = unordered_map<CacheKey, CacheValue, CacheHasher>;

	ParserSP parent;
	struct InheritedContext {  // 0 - no inheritance, 1 - inherit by copy, 2 inherit by reference
		u8 tokens = 0,
		   position = 0,
		   cache = 0;
	} inheritedContext;
	deque<Token> _tokens;
	usize _position = 0;
	Cache _cache;  // Memoization support

	Parser(deque<Token> tokens) : _tokens(filter(tokens, [this](auto& t) { return !t.trivia; })) {}

	/*
	Parser(ParserSP parent,
		   InheritedContext IC,
		   deque<Token> tokens) : parent(parent),
								  inheritedContext(IC),
								  _tokens(filter(tokens, [this](auto& t) { return !t.trivia; }))
	{
		if(IC.tokens == 1)		_tokens = parent->_tokens;
		if(IC.position == 1)	_position = parent->_position;
		if(IC.cache == 1)		_cache = parent->_cache;
	}
	*/

	deque<Token>& tokens() {
		return inheritedContext.tokens == 2 && parent ? parent->tokens() : _tokens;
	}

	usize& position() {
		return inheritedContext.position == 2 && parent ? parent->position() : _position;
	}

	Cache& cache() {
		return inheritedContext.cache == 2 && parent ? parent->cache() : _cache;
	}

	Token& token() {
		static Token dummy;

		return tokens().size() > position()
			 ? tokens().at(position())
			 : dummy = Token();
	}

	NodeSP parse(const NodeRule& nodeRule, const optional<string>& type) {
		Node node;

		for(auto& field : nodeRule.fields) {
			NodeValue value;

		//	if(field.rule.index() == 1) {
		//		value = parse(get<1>(field.rule), field.title);
		//	} else {
				value = parse(field.rule);
		//	}

			if(!value && !field.optional) {
				return nullptr;
			}

			if(field.title) {
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

		if(type) {
			node["type"] = *type;
		}

		return SP(node);
	}

	NodeValue parse(const TokenRule& tokenRule) {
		Token& token = this->token();

		if(tokenRule.type || tokenRule.value) {
			return (!tokenRule.type || regex_match(token.type, regex(*tokenRule.type))) &&
				   (!tokenRule.value || regex_match(token.value, regex(*tokenRule.value)));
		} else {
			return token.value;
		}
	}

	NodeValue parse(const VariantRule& variantRule) {  // TODO: Child node rules should have type (should they?)
		for(auto& rule : variantRule.rules) {
			if(auto value = parseBranch(rule)) {
				return value;
			}
		}

		return nullptr;
	}

	NodeValue parse(const SequenceRule& sequenceRule) {
		/* TODO */
	}

	NodeValue parse(const RuleRef& ruleRef) {
		if(Grammar::rules.contains(ruleRef)) {
			Rule& rule = Grammar::rules[ruleRef];

			if(rule.index() == 1) {
				return parse(get<1>(rule), ruleRef);
			} else {
				return parse(rule);
			}
		}

		return nullptr;
	}

	NodeValue parse(const Rule& rule) {
		switch(rule.index()) {
			case 0:  return parse(get<0>(rule));
			case 1:  return parse(get<1>(rule));
			case 2:  return parse(get<2>(rule));
			case 3:  return parse(get<3>(rule));
			case 4:  return parse(get<4>(rule));
			case 5:  return parse(get<5>(rule));
		}

		return nullptr;
	}

	/**
	 * Important for alternatives parsing and left-recursion cycles avoidance.
	 */
	NodeValue parseBranch(const Rule& rule) {
		CacheKey cacheKey(rule, position());
		CacheValue& cacheValue = cache()[cacheKey];

		if(!cacheValue.second) {
		//	auto parser = SP<Parser>(shared_from_this(), InheritedContext(2, 1, 2), deque<Token>());

		//	cacheValue = pair(parser->parse(rule), true);

			int position = _position;

			cacheValue = pair(parse(rule), true);
			_position = position;
		}

		return cacheValue.first;
	}

	NodeSP parse() {
		return parse(RuleRef("module"));
	}
};