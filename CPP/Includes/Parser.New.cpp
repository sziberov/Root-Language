#pragma once

#include "Grammar.cpp";

struct Parser {
    using NodeRule = Grammar::NodeRule;
    using TokenRule = Grammar::TokenRule;
	using VariantRule = Grammar::VariantRule;
	using SequenceRule = Grammar::SequenceRule;
	using HierarchyRule = Grammar::HierarchyRule;
	using RuleRef = Grammar::RuleRef;
	using Rule = Grammar::Rule;

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

    }

    NodeSP parse(const HierarchyRule& hierarchyRule) {

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

    NodeSP parse() {}
};