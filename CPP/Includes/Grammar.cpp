#pragma once

#include "Node.cpp";

struct Grammar {
	struct Rule;

	using RuleReference = string;

	struct TokenRule {
		optional<regex> type,
						value;
	};

	struct VariantRule {  // ?: ?: ?: ...
		vector<RuleReference> rules;
	};

	struct SequenceRule {  // Skippable enclosed sequential node(s)
		vector<RuleReference> rules,
							  subsequentRules;  // Disable strict order index impact (e.g. allow [1, 2, 2, 3] instead of [1, 2, 3, 1])
							  					// May be useful to handle some child rule types
		optional<TokenRule> opener,
							separator,
							closer;
		bool strict = false;  // Require defined order
		usize minCount = 0,  // 1+ for non-optional
			  maxCount = 0;  // 1 for non-list result
	};

	struct HierarchyRule {
		string title;  // Of subbest field
		RuleReference subrule;
		vector<RuleReference> superrules;
	};

	using LogicRule = variant<RuleReference,
							  TokenRule,
							  VariantRule,
							  SequenceRule,
							  HierarchyRule>;

	struct Field {
		string title;  // Empty for implicit
		LogicRule rule;
		bool optional;
	};

	struct Rule {
		string type;
		vector<Field> fields;
		function<void(Node&)> post;
	};

	unordered_map<string, Rule> rules = {
		{"argument", {
			.type = "argument"
		}},
		{"arrayLiteral", {
			.type = "arrayLiteral"
		}},
		{"arrayType", {
			.type = "arrayType"
		}},
		{"asOperator", {
			.type = "asOperator"
		}},
		{"asyncExpression", {
			.type = "asyncExpression"
		}},
		{"awaitExpression", {
			.type = "awaitExpression"
		}},
		{"body", {
			.type = "body"
		}},
		{"booleanLiteral", {
			.type = "booleanLiteral"
		}},
		{"breakStatement", {
			.type = "breakStatement"
		}},
		{"callExpression", {
			.type = "callExpression"
		}},
		{"caseDeclaration", {
			.type = "caseDeclaration"
		}},
		{"catchClause", {
			.type = "catchClause"
		}},
		{"chainDeclaration", {
			.type = "chainDeclaration"
		}},
		{"chainExpression", {
			.type = "chainExpression"
		}},
		{"chainIdentifier", {
			.type = "chainIdentifier"
		}},
		{"chainStatements", {
			.type = "chainStatements"
		}},
		{"classBody", {
			.type = "classBody"
		}},
		{"classDeclaration", {
			.type = "classDeclaration"
		}},
		{"classExpression", {
			.type = "classExpression"
		}},
		{"classStatements", {
			.type = "classStatements"
		}},
		{"closureExpression", {
			.type = "closureExpression"
		}},
		{"conditionalOperator", {
			.type = "conditionalOperator"
		}},
		{"continueStatement", {
			.type = "continueStatement"
		}},
		{"controlTransferStatement", {
			.type = "controlTransferStatement"
		}},
		{"declaration", {
			.type = "declaration"
		}},
		{"declarator", {
			.type = "declarator"
		}},
		{"defaultExpression", {
			.type = "defaultExpression"
		}},
		{"defaultType", {
			.type = "defaultType"
		}},
		{"deinitializerDeclaration", {
			.type = "deinitializerDeclaration"
		}},
		{"deleteExpression", {
			.type = "deleteExpression"
		}},
		{"dictionaryLiteral", {
			.type = "dictionaryLiteral"
		}},
		{"dictionaryType", {
			.type = "dictionaryType"
		}},
		{"doStatement", {
			.type = "doStatement"
		}},
		{"elseClause", {
			.type = "elseClause"
		}},
		{"entry", {
			.type = "entry"
		}},
		{"enumerationBody", {
			.type = "enumerationBody"
		}},
		{"enumerationDeclaration", {
			.type = "enumerationDeclaration"
		}},
		{"enumerationExpression", {
			.type = "enumerationExpression"
		}},
		{"enumerationStatements", {
			.type = "enumerationStatements"
		}},
		{"expression", {
			.type = "expression"
		}},
		{"expressionsSequence", {
			.type = "expressionsSequence"
		}},
		{"fallthroughStatement", {
			.type = "fallthroughStatement"
		}},
		{"floatLiteral", {
			.type = "floatLiteral"
		}},
		{"forStatement", {
			.type = "forStatement"
		}},
		{"functionBody", {
			.type = "functionBody"
		}},
		{"functionDeclaration", {
			.type = "functionDeclaration"
		}},
		{"functionExpression", {
			.type = "functionExpression"
		}},
		{"functionSignature", {
			.type = "functionSignature"
		}},
		{"functionStatement", {
			.type = "functionStatement"
		}},
		{"functionStatements", {
			.type = "functionStatements"
		}},
		{"functionType", {
			.type = "functionType"
		}},
		{"genericParameter", {
			.type = "genericParameter"
		}},
		{"genericParametersClause", {
			.type = "genericParametersClause"
		}},
		{"identifier", {
			.type = "identifier"
		}},
		{"ifStatement", {
			.type = "ifStatement"
		}},
		{"implicitChainExpression", {
			.type = "implicitChainExpression"
		}},
		{"implicitChainIdentifier", {
			.type = "implicitChainIdentifier"
		}},
		{"importDeclaration", {
			.type = "importDeclaration"
		}},
		{"infixExpression", {
			.type = "infixExpression"
		}},
		{"infixOperator", {
			.type = "infixOperator"
		}},
		{"inheritedTypesClause", {
			.type = "inheritedTypesClause"
		}},
		{"initializerClause", {
			.type = "initializerClause"
		}},
		{"initializerDeclaration", {
			.type = "initializerDeclaration"
		}},
		{"inOperator", {
			.type = "inOperator"
		}},
		{"inoutExpression", {
			.type = "inoutExpression"
		}},
		{"inoutType", {
			.type = "inoutType"
		}},
		{"integerLiteral", {
			.type = "integerLiteral"
		}},
		{"intersectionType", {
			.type = "intersectionType"
		}},
		{"isOperator", {
			.type = "isOperator"
		}},
		{"literalExpression", {
			.type = "literalExpression"
		}},
		{"modifiers", {
			.type = "modifiers"
		}},
		{"module", {
			.type = "module"
		}},
		{"namespaceBody", {
			.type = "namespaceBody"
		}},
		{"namespaceDeclaration", {
			.type = "namespaceDeclaration"
		}},
		{"namespaceExpression", {
			.type = "namespaceExpression"
		}},
		{"namespaceStatements", {
			.type = "namespaceStatements"
		}},
		{"nillableExpression", {
			.type = "nillableExpression"
		}},
		{"nillableType", {
			.type = "nillableType"
		}},
		{"nilLiteral", {
			.type = "nilLiteral"
		}},
		{"observerDeclaration", {
			.type = "observerDeclaration"
		}},
		{"observersBody", {
			.type = "observersBody"
		}},
		{"observersStatements", {
			.type = "observersStatements"
		}},
		{"operator", {
			.type = "operator"
		}},
		{"operatorBody", {
			.type = "operatorBody"
		}},
		{"operatorDeclaration", {
			.type = "operatorDeclaration"
		}},
		{"operatorStatements", {
			.type = "operatorStatements"
		}},
		{"parameter", {
			.type = "parameter"
		}},
		{"parenthesizedExpression", {
			.type = "parenthesizedExpression"
		}},
		{"parenthesizedType", {
			.type = "parenthesizedType"
		}},
		{"postfixExpression", {
			.type = "postfixExpression"
		}},
		{"postfixOperator", {
			.type = "postfixOperator"
		}},
		{"postfixType", {
			.type = "postfixType"
		}},
		{"predefinedType", {
			.type = "predefinedType"
		}},
		{"prefixExpression", {
			.type = "prefixExpression"
		}},
		{"prefixOperator", {
			.type = "prefixOperator"
		}},
		{"primaryExpression", {
			.type = "primaryExpression"
		}},
		{"primaryType", {
			.type = "primaryType"
		}},
		{"protocolBody", {
			.type = "protocolBody"
		}},
		{"protocolDeclaration", {
			.type = "protocolDeclaration"
		}},
		{"protocolExpression", {
			.type = "protocolExpression"
		}},
		{"protocolStatements", {
			.type = "protocolStatements"
		}},
		{"protocolType", {
			.type = "protocolType"
		}},
		{"returnStatement", {
			.type = "returnStatement"
		}},
		{"statements", {
			.type = "statements"
		}},
		{"stringExpression", {
			.type = "stringExpression"
		}},
		{"stringLiteral", {
			.type = "stringLiteral"
		}},
		{"stringSegment", {
			.type = "stringSegment"
		}},
		{"structureBody", {
			.type = "structureBody"
		}},
		{"structureDeclaration", {
			.type = "structureDeclaration"
		}},
		{"structureExpression", {
			.type = "structureExpression"
		}},
		{"structureStatements", {
			.type = "structureStatements"
		}},
		{"subscriptDeclaration", {
			.type = "subscriptDeclaration"
		}},
		{"subscriptExpression", {
			.type = "subscriptExpression"
		}},
		{"throwStatement", {
			.type = "throwStatement"
		}},
		{"tryExpression", {
			.type = "tryExpression"
		}},
		{"type", {
			.type = "type"
		}},
		{"typeClause", {
			.type = "typeClause"
		}},
		{"typeExpression", {
			.type = "typeExpression"
		}},
		{"typeIdentifier", {
			.type = "typeIdentifier"
		}},
		{"unionType", {
			.type = "unionType"
		}},
		{"variableDeclaration", {
			.type = "variableDeclaration"
		}},
		{"variadicType", {
			.type = "variadicType"
		}},
		{"whileStatement", {
			.type = "whileStatement"
		}}
	};
};