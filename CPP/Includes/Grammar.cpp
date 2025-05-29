#pragma once

#include "Node.cpp";

namespace Grammar {
	struct NodeRule;
	struct TokenRule;
	struct VariantRule;
	struct SequenceRule;
	struct HierarchyRule;

	using RuleRef = string;
	using Rule = variant<RuleRef,
						 NodeRule,
						 TokenRule,
						 VariantRule,
						 SequenceRule,
						 HierarchyRule>;

	// ----------------------------------------------------------------

	struct NodeRule {
		struct Field {
			optional<string> title;  // Nil for implicit
			Rule rule;
			vector<string> requiredBy;  // Empty for non-optional, strings for conditionally non-optional, zero-length string for optional
		};

		vector<Field> fields;
		bool normalize = false;  // Unwrap single field
		function<void(Node&)> post;
	};

	struct TokenRule {  // Returns token value if nothing set, otherwise boolean
		optional<regex> type,
						value;
	//	bool optional = false;
	};

	struct VariantRule {  // ?: ?: ?: ...
		vector<Rule> rules;
	};

	struct SequenceRule {  // Skippable enclosed sequential node(s)
		vector<RuleRef> rules,
						subsequentRules;  // Disable strict order index impact (e.g. allow [1, 2, 2, 3] instead of [1, 2, 3, 1])
						  				  // May be useful to handle some child rule types
		optional<TokenRule> opener,
							separator,
							closer;
		bool strict = false,  // Require defined order
			 normalize = false;  // Unwrap single element
		usize minCount = 0,  // 1+ for non-optional
			  maxCount = 0;
	};

	struct HierarchyRule {
		string title;  // Of hierarchy field
		RuleRef subrule;
		vector<RuleRef> superrules;
	};

	// ----------------------------------------------------------------

	unordered_map<RuleRef, Rule> rules = {
		{"argument", VariantRule({
			NodeRule({
				{"label", "identifier"},
				{nullopt, TokenRule(regex("operator*"), regex(":"))},
				{"value", "expressionsSequence", {""}},
			}),
			NodeRule({
				{"value", "expressionsSequence"}
			})
		})},
		{"arrayLiteral", NodeRule({
			{nullopt, TokenRule(regex("bracketOpen"))},
			{"values", SequenceRule {
				.rules = {"expressionsSequence"},
				.separator = TokenRule(regex("operator*"), regex(","))
			}},
			{nullopt, TokenRule(regex("bracketClosed"))}
		})},
		{"arrayType", NodeRule({
			{nullopt, TokenRule(regex("bracketOpen"))},
			{"value", "type"},
			{nullopt, TokenRule(regex("bracketClosed"))}
		})},
		{"asOperator", NodeRule({
			{nullopt, TokenRule(regex("keywordAs"))},
			{"type_", "type"}
		})},
		{"asyncExpression", NodeRule({
			{nullopt, TokenRule(regex("keywordAsync"))},
			{"value", "expression"}
		})},
		{"awaitExpression", NodeRule({
			{nullopt, TokenRule(regex("keywordAwait"))},
			{"value", "expression"}
		})},
		{"body", NodeRule()},
		{"booleanLiteral", NodeRule({
			{nullopt, TokenRule(regex("keywordFalse|keywordTrue"))},
			{"value", TokenRule()}
		})},
		{"breakStatement", NodeRule({
			{nullopt, TokenRule(regex("keywordBreak"))},
			{"label", "identifier", {""}}
		})},
		{"callExpression", NodeRule()},
		{"caseDeclaration", NodeRule()},
		{"catchClause", NodeRule()},
		{"chainDeclaration", NodeRule()},
		{"chainExpression", NodeRule({
			{nullopt, TokenRule(regex("operator|operatorInfix"), regex("."))},
			{"member", VariantRule({
				"identifier",
				"stringLiteral"
			})}
		})},
		{"chainIdentifier", NodeRule({
			{nullopt, TokenRule(regex("operatorPrefix|operatorInfix"), regex("."))},
			{"value", "identifier"}
		})},
		{"chainStatements", NodeRule()},
		{"classBody", NodeRule()},
		{"classDeclaration", NodeRule()},
		{"classExpression", NodeRule()},
		{"classStatements", NodeRule()},
		{"closureExpression", NodeRule()},
		{"conditionalOperator", NodeRule()},
		{"continueStatement", NodeRule({
			{nullopt, TokenRule(regex("keywordContinue"))},
			{"label", "identifier", {""}}
		})},
		{"controlTransferStatement", VariantRule({
			"breakStatement",
			"continueStatement",
			"fallthroughStatement",
			"returnStatement",
			"throwStatement"
		})},
		{"declaration", NodeRule()},
		{"declarator", NodeRule()},
		{"defaultExpression", NodeRule()},
		{"defaultType", NodeRule()},
		{"deinitializerDeclaration", NodeRule()},
		{"deleteExpression", NodeRule()},
		{"dictionaryLiteral", NodeRule()},
		{"dictionaryType", NodeRule()},
		{"doStatement", NodeRule()},
		{"elseClause", NodeRule()},
		{"entry", NodeRule()},
		{"enumerationBody", NodeRule()},
		{"enumerationDeclaration", NodeRule()},
		{"enumerationExpression", NodeRule()},
		{"enumerationStatements", NodeRule()},
		{"expression", NodeRule()},
		{"expressionsSequence", NodeRule()},
		{"fallthroughStatement", NodeRule({
			{nullopt, TokenRule(regex("keywordFallthrough"))},
			{"label", "identifier", {""}}
		})},
		{"floatLiteral", NodeRule()},
		{"forStatement", NodeRule()},
		{"functionBody", NodeRule()},
		{"functionDeclaration", NodeRule()},
		{"functionExpression", NodeRule()},
		{"functionSignature", NodeRule()},
		{"functionStatement", NodeRule()},
		{"functionStatements", NodeRule()},
		{"functionType", NodeRule()},
		{"genericParameter", NodeRule()},
		{"genericParametersClause", NodeRule()},
		{"identifier", NodeRule({
			{"value", TokenRule(regex("identifier"))}
		})},
		{"ifStatement", NodeRule()},
		{"implicitChainExpression", NodeRule()},
		{"implicitChainIdentifier", NodeRule()},
		{"importDeclaration", NodeRule()},
		{"infixExpression", NodeRule()},
		{"infixOperator", NodeRule()},
		{"inheritedTypesClause", NodeRule()},
		{"initializerClause", NodeRule()},
		{"initializerDeclaration", NodeRule()},
		{"inOperator", NodeRule()},
		{"inoutExpression", NodeRule()},
		{"inoutType", NodeRule()},
		{"integerLiteral", NodeRule()},
		{"intersectionType", NodeRule()},
		{"isOperator", NodeRule()},
		{"literalExpression", NodeRule()},
		{"modifiers", NodeRule()},
		{"module", NodeRule()},
		{"namespaceBody", NodeRule()},
		{"namespaceDeclaration", NodeRule()},
		{"namespaceExpression", NodeRule()},
		{"namespaceStatements", NodeRule()},
		{"nillableExpression", NodeRule()},
		{"nillableType", NodeRule()},
		{"nilLiteral", NodeRule()},
		{"observerDeclaration", NodeRule()},
		{"observersBody", NodeRule()},
		{"observersStatements", NodeRule()},
		{"operator", NodeRule()},
		{"operatorBody", NodeRule()},
		{"operatorDeclaration", NodeRule()},
		{"operatorStatements", NodeRule()},
		{"parameter", NodeRule()},
		{"parenthesizedExpression", NodeRule()},
		{"parenthesizedType", NodeRule()},
		{"postfixExpression", NodeRule({
			{"value", HierarchyRule({
				.title = "",
				.subrule = "primaryExpression",
				.superrules = {
					"callExpression",
					"chainExpression",
					"defaultExpression",
					"nillableExpression",
					"subscriptExpression"
				}
			})},
			{"operator", "postfixOperator", {""}}
		}, true)},
		{"postfixOperator", NodeRule()},
		{"postfixType", NodeRule()},
		{"predefinedType", NodeRule()},
		{"prefixExpression", NodeRule()},
		{"prefixOperator", NodeRule()},
		{"primaryExpression", NodeRule()},
		{"primaryType", NodeRule()},
		{"protocolBody", NodeRule()},
		{"protocolDeclaration", NodeRule()},
		{"protocolExpression", NodeRule()},
		{"protocolStatements", NodeRule()},
		{"protocolType", NodeRule()},
		{"returnStatement", NodeRule({
			{nullopt, TokenRule(regex("keywordReturn"))},
			{"value", "expressionsSequence", {""}}
		})},
		{"statements", NodeRule()},
		{"stringExpression", NodeRule()},
		{"stringLiteral", NodeRule()},
		{"stringSegment", NodeRule()},
		{"structureBody", NodeRule()},
		{"structureDeclaration", NodeRule({
			{nullopt, TokenRule(regex("keywordStruct"))},
			{"modifiers", "modifiers", {""}},
			{"identifier", "identifier"},
			{"genericParameters", "genericParametersClause", {""}},
			{"inheritedTypes", "inheritedTypesClause", {""}},
			{"body", "structureBody"}
		})},
		{"structureExpression", NodeRule({
			{nullopt, TokenRule(regex("keywordStruct"))},
			{"genericParameters", "genericParametersClause", {""}},
			{"inheritedTypes", "inheritedTypesClause", {""}},
			{"body", "structureBody"}
		})},
		{"structureStatements", NodeRule()},
		{"subscriptDeclaration", NodeRule()},
		{"subscriptExpression", NodeRule()},
		{"throwStatement", NodeRule({
			{nullopt, TokenRule(regex("keywordThrow"))},
			{"value", "expressionsSequence", {""}}
		})},
		{"tryExpression", NodeRule()},
		{"type", NodeRule()},
		{"typeClause", NodeRule()},
		{"typeExpression", NodeRule()},
		{"typeIdentifier", NodeRule()},
		{"unionType", NodeRule()},
		{"variableDeclaration", NodeRule()},
		{"variadicType", NodeRule()},
		{"whileStatement", NodeRule()}
	};
};