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

	struct NodeField {
		optional<string> title;  // Nil for implicit
		Rule rule;
		bool optional = false;
	};

	struct NodeRule {
		vector<NodeField> fields;
		bool normalize = false;  // Unwrap single field
		function<void(Node&)> post;

		bool operator==(const NodeRule& r) const {
			return fields == r.fields &&
				   normalize == r.normalize &&
				   post.target<void(Node&)>() == r.post.target<void(Node&)>();
		}
	};

	struct TokenRule {  // Returns token value if nothing set, otherwise boolean
		optional<string> type,
						 value;
	//	bool optional = false;

		bool operator==(const TokenRule& r) const = default;
	};

	struct VariantRule {  // ?: ?: ?: ...
		vector<Rule> rules;

		bool operator==(const VariantRule& r) const {
			return rules == r.rules;
		}
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

		bool operator==(const SequenceRule& r) const = default;
	};

	struct HierarchyRule {
		string title;  // Of hierarchy field
		Rule subrule;
		VariantRule superrules;

		bool operator==(const HierarchyRule& r) const {
			return title == r.title &&
				   subrule == r.subrule &&
				   superrules == r.superrules;
		}
	};

	// ----------------------------------------------------------------

	unordered_map<RuleRef, Rule> rules = {
		{"argument", VariantRule({
			NodeRule({
				{"label", "identifier"},
				{nullopt, TokenRule("operator*", ":")},
				{"value", "expressionsSequence", true},
			}),
			NodeRule({
				{"value", "expressionsSequence"}
			})
		})},
		{"arrayLiteral", NodeRule({
			{nullopt, TokenRule("bracketOpen")},
			{"values", SequenceRule {
				.rules = {"expressionsSequence"},
				.separator = TokenRule("operator*", ",")
			}},
			{nullopt, TokenRule("bracketClosed")}
		})},
		{"arrayType", NodeRule({
			{nullopt, TokenRule("bracketOpen")},
			{"value", "type"},
			{nullopt, TokenRule("bracketClosed")}
		})},
		{"asOperator", NodeRule({
			{nullopt, TokenRule("keywordAs")},
			{"type_", "type"}
		})},
		{"asyncExpression", NodeRule({
			{nullopt, TokenRule("keywordAsync")},
			{"value", "expression"}
		})},
		{"awaitExpression", NodeRule({
			{nullopt, TokenRule("keywordAwait")},
			{"value", "expression"}
		})},
		{"body", NodeRule()},
		{"booleanLiteral", NodeRule({
			{nullopt, TokenRule("keywordFalse|keywordTrue")},
			{"value", TokenRule()}
		})},
		{"breakStatement", NodeRule({
			{nullopt, TokenRule("keywordBreak")},
			{"label", "identifier", true}
		})},
		{"callExpression", NodeRule()},
		{"caseDeclaration", NodeRule()},
		{"catchClause", NodeRule()},
		{"chainDeclaration", NodeRule()},
		{"chainExpression", NodeRule({
			{nullopt, TokenRule("operator|operatorInfix", ".")},
			{"member", VariantRule({
				"identifier",
				"stringLiteral"
			})}
		})},
		{"chainIdentifier", NodeRule({
			{nullopt, TokenRule("operatorPrefix|operatorInfix", ".")},
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
			{nullopt, TokenRule("keywordContinue")},
			{"label", "identifier", true}
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
			{nullopt, TokenRule("keywordFallthrough")},
			{"label", "identifier", true}
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
			{"value", TokenRule("identifier")}
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
				.superrules = VariantRule({
					"callExpression",
					"chainExpression",
					"defaultExpression",
					"nillableExpression",
					"subscriptExpression"
				})
			})},
			{"operator", "postfixOperator", true}
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
			{nullopt, TokenRule("keywordReturn")},
			{"value", "expressionsSequence", true}
		})},
		{"statements", NodeRule()},
		{"stringExpression", NodeRule()},
		{"stringLiteral", NodeRule()},
		{"stringSegment", NodeRule()},
		{"structureBody", NodeRule()},
		{"structureDeclaration", NodeRule({
			{nullopt, TokenRule("keywordStruct")},
			{"modifiers", "modifiers", true},
			{"identifier", "identifier"},
			{"genericParameters", "genericParametersClause", true},
			{"inheritedTypes", "inheritedTypesClause", true},
			{"body", "structureBody"}
		})},
		{"structureExpression", NodeRule({
			{nullopt, TokenRule("keywordStruct")},
			{"genericParameters", "genericParametersClause", true},
			{"inheritedTypes", "inheritedTypesClause", true},
			{"body", "structureBody"}
		})},
		{"structureStatements", NodeRule()},
		{"subscriptDeclaration", NodeRule()},
		{"subscriptExpression", NodeRule()},
		{"throwStatement", NodeRule({
			{nullopt, TokenRule("keywordThrow")},
			{"value", "expressionsSequence", true}
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