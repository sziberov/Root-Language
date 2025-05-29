#pragma once

#include "Lexer.cpp";
#include "Grammar.cpp";

struct Parser {
	using Location = Lexer::Location;
	using Token = Lexer::Token;

	using NodeRule = Grammar::NodeRule;
	using TokenRule = Grammar::TokenRule;
	using VariantRule = Grammar::VariantRule;
	using SequenceRule = Grammar::SequenceRule;
	using HierarchyRule = Grammar::HierarchyRule;
	using RuleRef = Grammar::RuleRef;
	using Rule = Grammar::Rule;

	deque<Token> tokens;
	int position = 0;

	Parser(deque<Token> tokens) : tokens(filter(tokens, [this](auto& t) { return !t.trivia; })) {}

	Token& token() {
		static Token dummy;

		return tokens.size() > position
			 ? tokens[position]
			 : dummy = Token();
	}

	NodeSP parse(const NodeRule& nodeRule) {
		Node node;

		return SP(node);
	}

	NodeValue parse(const TokenRule& tokenRule) {
		if(tokenRule.type || tokenRule.value) {
			return (!tokenRule.type || regex_match(token().type, *tokenRule.type)) &&
				   (!tokenRule.value || regex_match(token().value, *tokenRule.value));
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
			// Эта часть не проверялась и я не уверен в её теории, но мне кажется что это правильно:
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