#pragma once

#include "Scheduler.cpp";
#include "Lexer.cpp";
#include "Grammar.cpp";

struct Parser;

using ParserSP = sp<Parser>;

struct Parser : enable_shared_from_this<Parser> {
	using Location = Lexer::Location;
	using Token = Lexer::Token;

	using NodeRule = Grammar::NodeRule;
	using TokenRule = Grammar::TokenRule;
	using VariantRule = Grammar::VariantRule;
	using SequenceRule = Grammar::SequenceRule;
	using HierarchyRule = Grammar::HierarchyRule;
	using RuleRef = Grammar::RuleRef;
	using Rule = Grammar::Rule;

	using RuleKey = pair<Rule, int>;

	struct RuleHasher {
		usize operator()(const RuleKey& rule) const {
			return 0;  // TODO
		}
	};

	ParserSP parent;
	struct InheritedContext {  // 0 - no inheritance, 1 - inherit by copy, 2 inherit by reference
		u8 tokens = 0,
		   position = 0,
		   results = 0,
		   scheduler = 0;  // Doesn't support 1
	} inheritedContext;
	deque<Token> _tokens;
	int _position = 0;
	unordered_map<RuleKey, NodeValue, RuleHasher> _results;
	Scheduler _scheduler;
	std::mutex mutex;

	Parser(deque<Token> tokens) : _tokens(filter(tokens, [this](auto& t) { return !t.trivia; })) {}

	Parser(ParserSP parent,
		   InheritedContext IC,
		   deque<Token> tokens) : parent(parent),
								  inheritedContext(IC),
								  _tokens(filter(tokens, [this](auto& t) { return !t.trivia; }))
	{
		if(IC.tokens == 1)		_tokens = parent->_tokens;
		if(IC.position == 1)	_position = parent->_position;
		if(IC.results == 1)		_results = parent->_results;
	//	if(IC.scheduler == 1)	_scheduler = parent->_scheduler;
	}

	deque<Token>& tokens() {
		return inheritedContext.tokens == 2 && parent ? parent->tokens() : _tokens;
	}

	int& position() {
		return inheritedContext.position == 2 && parent ? parent->position() : _position;
	}

	unordered_map<RuleKey, NodeValue, RuleHasher>& results() {
		return inheritedContext.results == 2 && parent ? parent->results() : _results;
	}

	Scheduler& scheduler() {
		return inheritedContext.scheduler == 2 && parent ? parent->scheduler() : _scheduler;
	}

	Token& token() {
		static Token dummy;

		return tokens().size() > position()
			 ? tokens().at(position())
			 : dummy = Token();
	}

	usize scheduleParse(const Rule& rule, int pos) {
		return scheduler().schedule([&](usize) {
			auto parser = SP<Parser>(shared_from_this(), InheritedContext(2, 1, 2, 2), deque<Token>());
			NodeValue result = parser->parse(rule);
			lock_guard lock(mutex);

			results()[RuleKey(rule, pos)] = result;
		});
	}

	NodeValue awaitParse(const Rule& rule, int pos) {
		RuleKey key(rule, pos);

		{
			lock_guard lock(mutex);

			if(results().contains(key)) {
				return results().at(key);
			}
		}

		scheduler().await(scheduleParse(rule, pos));

		lock_guard lock(mutex);

		return results().at(key);
	}

	NodeSP parse(const NodeRule& nodeRule) {
		Node node;

		return SP(node);
	}

	NodeValue parse(const TokenRule& tokenRule) {
		if(tokenRule.type || tokenRule.value) {
			return (!tokenRule.type || regex_match(token().type, regex(*tokenRule.type))) &&
				   (!tokenRule.value || regex_match(token().value, regex(*tokenRule.value)));
		} else {
			return token().value;
		}
	}

	NodeValue parse(const VariantRule& variantRule) {
		for(auto& rule : variantRule.rules) {
			if(auto value = parse(rule)) {
				return value;
			}
		}

		return nullptr;
	}

	NodeValue parse(const SequenceRule& sequenceRule) {
		/* TODO */
	}

	NodeSP parse(const HierarchyRule& hierarchyRule) {
		if(NodeSP node = parse(hierarchyRule.subrule)) {
			while(NodeSP node_ = parse(hierarchyRule.superrules)) {
				(*node_)[hierarchyRule.title] = node;
				node = node_;
			}

			return node;
		}

		return nullptr;
	}

	NodeValue parse(const RuleRef& ruleRef) {
		if(Grammar::rules.contains(ruleRef)) {
			return parse(Grammar::rules.at(ruleRef));
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

	NodeSP parse() {
		return parse(RuleRef("module"));
	}
};