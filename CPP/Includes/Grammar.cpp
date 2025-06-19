#pragma once

#include "Node.cpp"

namespace Grammar {
	struct NodeRule;
	struct TokenRule;
	struct VariantRule;
	struct SequenceRule;

	using RuleRef = string;
	using Rule = variant<RuleRef,
						 recursive_wrapper<NodeRule>,
						 recursive_wrapper<TokenRule>,
						 recursive_wrapper<VariantRule>,
						 recursive_wrapper<SequenceRule>>;

	// ----------------------------------------------------------------

	struct NodeField {
		NodeField(const char* t, Rule r = RuleRef(), bool o = false) : title(string(t)), rule(r), optional(o) {}
		NodeField(const string t, Rule r = RuleRef(), bool o = false) : title(t), rule(r), optional(o) {}
		NodeField(const nullopt_t t, Rule r = RuleRef(), bool o = false) : title(t), rule(r), optional(o) {}

		std::optional<string> title;  // Nil for implicit
		Rule rule;
		bool optional = false;

		bool operator==(const NodeField& f) const = default;
	};

	struct NodeRule {
		NodeRule(initializer_list<NodeField> f, bool n = false, function<void(Node&)> p = nullptr) : fields(f), normalize(n), post(p) {}

		vector<NodeField> fields;
		bool normalize = false;  // Unwrap single field
		function<void(Node&)> post;

		bool operator==(const NodeRule& r) const {
			return fields == r.fields &&
				   normalize == r.normalize &&
				   post.target<void(Node&)>() == r.post.target<void(Node&)>();
		}
	};

	struct TokenRule {
		optional<string> type, value;

		bool operator==(const TokenRule& r) const = default;
	};

	struct VariantRule : vector<Rule> {};

	struct SequenceRule {
		Rule rule;
		optional<Rule> opener, delimiter, closer;

		// Examples: 0/0, 1/1, 0/1, 1/max, 0/max.
		usize range[2] = {0, usize(-1)},
			  innerDelimitRange[2] = {0, usize(-1)},
			  outerDelimitRange[2] = {0, usize(-1)};

		bool delimited = false,  // Include delimiters if any
			 dirty = true,  // Include unrecognized tokens if any
			 normalize = false;  // Unwrap single value (or nil)

		bool operator==(const SequenceRule& r) const = default;
	};

	// ----------------------------------------------------------------

	unordered_map<RuleRef, Rule> rules = {
		{"argument", VariantRule({
			NodeRule({
				{"label", "labelClause"},
				{"value", "expressionsSequence", true},
			}),
			NodeRule({
				{"value", "expressionsSequence"}
			})
		})},
		{"arrayLiteral", NodeRule({
			{"values", SequenceRule {
				.rule = "expressionsSequence",
				.opener = TokenRule("bracketOpen"),
				.delimiter = TokenRule("operator.*", ","),
				.closer = TokenRule("bracketClosed"),
				.innerDelimitRange = {1, usize(-1)},
				.dirty = false
			}}
		})},
		{"arrayType", NodeRule({
			{nullopt, TokenRule("bracketOpen")},
			{"value", "type"},
			{nullopt, TokenRule("bracketClosed")}
		})},
		{"asExpression", NodeRule({
			{"value", "expression"},
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
		{"body", RuleRef()},
		{"booleanLiteral", NodeRule({
			{"value", TokenRule("keywordFalse|keywordTrue")}
		})},
		{"breakStatement", NodeRule({
			{nullopt, TokenRule("keywordBreak")},
			{"label", "identifier", true}
		})},
		{"callArgumentsClause", SequenceRule {
			.rule = "argument",
			.opener = TokenRule("parenthesisOpen"),
			.delimiter = TokenRule("operator.*", ","),
			.closer = TokenRule("parenthesisClosed"),
			.innerDelimitRange = {1, usize(-1)}
		}},
		{"callExpression", VariantRule({
			NodeRule({
				{"callee", "postfixExpression"},
				{"genericArguments", "genericArgumentsClause", true},
				{"arguments", "callArgumentsClause"},
				{"closure", "closureExpression", true}
			}),
			NodeRule({
				{"callee", "postfixExpression"},
				{"genericArguments", "genericArgumentsClause", true},
				{"closure", "closureExpression"}
			})
		})},
		{"caseDeclaration", RuleRef()},
		{"catchClause", RuleRef()},
		{"chainDeclaration", RuleRef()},
		{"chainExpression", NodeRule({
			{"composite", "postfixExpression"},
			{nullopt, TokenRule("operator|operatorInfix", "\\.")},
			{"member", VariantRule({
				"identifier",
				"stringLiteral"
			})}
		})},
		{"chainIdentifier", NodeRule({
			{"supervalue", "postfixExpression"},
			{nullopt, TokenRule("operatorPrefix|operatorInfix", "\\.")},
			{"value", "identifier"}
		})},
		{"chainStatements", RuleRef()},
		{"classBody", RuleRef()},
		{"classDeclaration", RuleRef()},
		{"classExpression", RuleRef()},
		{"classStatements", RuleRef()},
		{"closureExpression", RuleRef()},
		{"conditionalOperator", NodeRule({
			{nullopt, TokenRule("operator.*", "\\?")},
			{"expression", "expressionsSequence"},
			{nullopt, TokenRule("operator.*", ":")}
		})},
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
		{"declaration", VariantRule({
			"chainDeclaration",
			"classDeclaration",
			"deinitializerDeclaration",
			"enumerationDeclaration",
			"functionDeclaration",
			"importDeclaration",
			"initializerDeclaration",
			"namespaceDeclaration",
			"operatorDeclaration",
			"protocolDeclaration",
			"structureDeclaration",
			"subscriptDeclaration",
			"variableDeclaration"
		})},
		{"declarator", NodeRule({
			{"identifier", "identifier"},
			{"type_", "typeClause", true},
			{"value", "initializerClause", true},
			{"body", VariantRule({
				"observersBody",
				"functionBody"
			}), true}
		})},
		{"defaultExpression", NodeRule({
			{"value", "postfixExpression"},
			{nullopt, TokenRule("operatorPostfix", "!")}
		})},
		{"defaultType", NodeRule({
			{"value", "postfixType"},
			{nullopt, TokenRule("operatorPostfix", "!")}
		})},
		{"deinitializerDeclaration", RuleRef()},
		{"deleteExpression", NodeRule({
			{nullopt, TokenRule("identifier", "delete")},
			{"value", "expression"}
		})},
		{"dictionaryLiteral", RuleRef()},
		{"dictionaryType", RuleRef()},
		{"doStatement", RuleRef()},
		{"elseClause", RuleRef()},
		{"entry", NodeRule({
			{"key", "expressionsSequence"},
			{nullopt, TokenRule("operator.*", ":")},
			{"value", "expressionsSequence"}
		})},
		{"enumerationBody", RuleRef()},
		{"enumerationDeclaration", RuleRef()},
		{"enumerationExpression", RuleRef()},
		{"enumerationStatements", RuleRef()},
		{"expression", VariantRule({
			"asExpression",
			"isExpression",
			"asyncExpression",
			"awaitExpression",
			"deleteExpression",
			"tryExpression",
			"prefixExpression"
		})},
		{"expressionsSequence", VariantRule({
			NodeRule({
				{"values", SequenceRule {
					.rule = "expression",
					.delimiter = "infixExpression",
					.range = {2, usize(-1)},
					.innerDelimitRange = {1, 1},
					.outerDelimitRange = {0, 0},
					.delimited = true,
					.dirty = false
				}}
			}),
			"expression"
		})},
		{"fallthroughStatement", NodeRule({
			{nullopt, TokenRule("keywordFallthrough")},
			{"label", "identifier", true}
		})},
		{"floatLiteral", NodeRule({
			{"value", TokenRule("numberFloat")}
		})},
		{"forStatement", RuleRef()},
		{"functionBody", NodeRule({
			{"statements", SequenceRule {
				.rule = "functionStatement",
				.opener = TokenRule("braceOpen"),
				.delimiter = TokenRule("delimiter"),
				.closer = TokenRule("braceClosed")
			}}
		})},
		{"functionDeclaration", NodeRule({
			{"modifiers", "modifiers", true},
			{nullopt, TokenRule("keywordFunc")},
			{"identifier", VariantRule({
				"identifier",
				"operator"
			}), true},
			{"signature", "functionSignature", true},
			{"body", "functionBody", true}
		})},
		{"functionExpression", NodeRule({
			{nullopt, TokenRule("keywordFunc")},
			{"signature", "functionSignature", true},
			{"body", "functionBody", true}
		})},
		{"functionSignature", NodeRule({
			{"genericParameters", "genericParametersClause", true},
			{"parameters", "parametersClause", true},
			{"returnType", "returnClause", true}
		})},
		{"functionStatement", VariantRule({
			"declaration",  // Must be parsed before expressions, otherwise composite declarations will be parsed as [anonymous composite, identifier] expressions
			"expressionsSequence",
			"controlTransferStatement",
			"doStatement",
			"forStatement",
			"ifStatement",
			"whileStatement"
		})},
		{"functionStatements", SequenceRule {
			.rule = "functionStatement",
			.delimiter = TokenRule("delimiter")
		}},
		{"functionType", RuleRef()},
		{"genericArgument", "type"},
		{"genericArgumentsClause", SequenceRule {
			.rule = "genericArgument",
			.opener = TokenRule("operator.*", "<"),
			.delimiter = TokenRule("operator.*", ","),
			.closer = TokenRule("operator.*", ">"),
			.innerDelimitRange = {1, usize(-1)},
			.dirty = false
		}},
		{"genericParameter", NodeRule({
			{"identifier", "identifier"},
			{"type_", "typeClause", true}
		})},
		{"genericParametersClause", SequenceRule {
			.rule = "genericParameter",
			.opener = TokenRule("operator.*", "<"),
			.delimiter = TokenRule("operator.*", ","),
			.closer = TokenRule("operator.*", ">"),
			.innerDelimitRange = {1, usize(-1)}
		}},
		{"identifier", NodeRule({
			{"value", TokenRule("identifier")}
		})},
		{"ifStatement", RuleRef()},
		{"implicitChainExpression", NodeRule({
			{nullopt, TokenRule("operator|operatorPrefix|opratorInfix", "\\.")},
			{"member", VariantRule({
				"identifier",
				"stringLiteral"
			})}
		})},
		{"implicitChainIdentifier", NodeRule({
			{nullopt, TokenRule("operator|operatorPrefix|opratorInfix", "\\.")},
			{"value", "identifier"}
		})},
		{"importDeclaration", RuleRef()},
		{"infixExpression", VariantRule({
			"conditionalOperator",
			"inOperator",
			"infixOperator"
		})},
		{"infixOperator", NodeRule({
			{"value", TokenRule("operator|operatorInfix", "[^,:]*")}  // Enclosing nodes can use operators from the exceptions list as delimiters
		})},
		{"inheritedTypesClause", NodeRule({
			{nullopt, TokenRule("operator.*", ":")},
			{"types", SequenceRule {
				.rule = "typeIdentifier",
				.delimiter = TokenRule("operator.*", ","),
				.innerDelimitRange = {1, usize(-1)},
				.dirty = false
			}}
		}, true)},
		{"initializerClause", NodeRule({
			{nullopt, TokenRule("operator.*", "=")},
			{"value", "expressionsSequence", true}
		}, true)},
		{"initializerDeclaration", RuleRef()},
		{"inOperator", NodeRule({
			{"inverted", TokenRule("operatorPrefix", "!"), true},
			{nullopt, TokenRule("keywordIn")}
		})},
		{"inoutExpression", NodeRule({
			{nullopt, TokenRule("operatorPrefix", "&")},
			{"value", "postfixExpression"},
		})},
		{"inoutType", NodeRule({
			{nullopt, TokenRule("keywordInout")},
			{"value", "unionType"},
		})},
		{"integerLiteral", NodeRule({
			{"value", TokenRule("numberInteger")}
		})},
		{"intersectionType", RuleRef()},
		{"isExpression", NodeRule({
			{"value", "expression"},
			{"inverted", TokenRule("operatorPrefix", "!"), true},
			{nullopt, TokenRule("keywordIs")},
			{"type_", "type"},
		})},
		{"labelClause", NodeRule({
			{"label", "identifier"},
			{nullopt, TokenRule("operator.*", ":")},
		}, true)},
		{"literalExpression", VariantRule({
			"arrayLiteral",
			"booleanLiteral",
			"dictionaryLiteral",
			"floatLiteral",
			"integerLiteral",
			"nilLiteral",
			"stringLiteral"
		})},
		{"modifiers", SequenceRule {
			.rule = VariantRule({
				TokenRule("keywordInfix"),
				TokenRule("keywordPostfix"),
				TokenRule("keywordPrefix"),

				TokenRule("keywordPrivate"),
				TokenRule("keywordProtected"),
				TokenRule("keywordPublic"),

				TokenRule("keywordFinal"),
				TokenRule("keywordLazy"),
				TokenRule("keywordStatic"),
				TokenRule("keywordVirtual")
			}),
			.dirty = false
		}},
		{"module", NodeRule({
			{"statements", "functionStatements"}
		})},
		{"namespaceBody", RuleRef()},
		{"namespaceDeclaration", RuleRef()},
		{"namespaceExpression", RuleRef()},
		{"namespaceStatements", RuleRef()},
		{"nillableExpression", NodeRule({
			{"value", "postfixExpression"},
			{nullopt, TokenRule("operatorPostfix", "\\?")}
		})},
		{"nillableType", NodeRule({
			{"value", "postfixType"},
			{nullopt, TokenRule("operatorPostfix", "\\?")}
		})},
		{"nilLiteral", RuleRef()},
		{"observerDeclaration", RuleRef()},
		{"observersBody", RuleRef()},
		{"observersStatements", RuleRef()},
		{"operator", RuleRef()},
		{"operatorBody", RuleRef()},
		{"operatorDeclaration", RuleRef()},
		{"operatorStatements", RuleRef()},
		{"parameter", NodeRule({
			{"label", "identifier", true},
			{"identifier", "identifier"},
			{"type_", "typeClause", true},
			{"value", "initializerClause", true}
		})},
		{"parametersClause", SequenceRule {
			.rule = "parameter",
			.opener = TokenRule("parenthesisOpen"),
			.delimiter = TokenRule("operator.*", ","),
			.closer = TokenRule("parenthesisClosed"),
			.innerDelimitRange = {1, usize(-1)}
		}},
		{"parenthesizedExpression", NodeRule({
			{"value", SequenceRule {
				.rule = "expressionsSequence",
				.opener = TokenRule("parenthesisOpen"),
				.closer = TokenRule("parenthesisClosed"),
				.range = {1, 1},
				.normalize = true
			}}
		})},
		{"parenthesizedType", RuleRef()},
		{"postfixExpression", NodeRule({
			{"value", VariantRule({
				"callExpression",
				"chainExpression",
				"defaultExpression",
				"nillableExpression",
				"subscriptExpression",
				"primaryExpression"
			})},
			{"operator", "postfixOperator", true}
		}, true)},
		{"postfixOperator", NodeRule({
			{"value", TokenRule("operatorPostfix", "[^,:]*")}  // Enclosing nodes can use trailing operators from the exceptions list
		})},
		{"postfixType", VariantRule({
			"defaultType",
			"nillableType",
			"primaryType"
		})},
		{"predefinedType", RuleRef()},
		{"prefixExpression", NodeRule({
			{"operator", "prefixOperator", true},
			{"value", "postfixExpression"},
		}, true)},
		{"prefixOperator", NodeRule({
			{"value", TokenRule("operatorPrefix", "[^&.]*")}  // primaryExpressions can start with operators from the exceptions list
		})},
		{"primaryExpression", VariantRule({
			"classExpression",
			"closureExpression",
			"enumerationExpression",
			"functionExpression",
			"identifier",
			"implicitChainExpression",
			"inoutExpression",
			"literalExpression",
			"namespaceExpression",
			"parenthesizedExpression",
			"protocolExpression",
			"structureExpression",
			"typeExpression"
		})},
		{"primaryType", VariantRule({
			"arrayType",
			"dictionaryType",
			"functionType",
			"parenthesizedType",
			"predefinedType",
			"protocolType",
			"typeIdentifier"
		})},
		{"protocolBody", RuleRef()},
		{"protocolDeclaration", RuleRef()},
		{"protocolExpression", RuleRef()},
		{"protocolStatements", RuleRef()},
		{"protocolType", RuleRef()},
		{"returnClause", NodeRule({
			{nullopt, TokenRule("operator.*", "->")},
			{"type_", "type"}
		}, true)},
		{"returnStatement", NodeRule({
			{nullopt, TokenRule("keywordReturn")},
			{"value", "expressionsSequence", true}
		})},
		{"statement", RuleRef()},
		{"stringExpression", NodeRule({
			{"value", SequenceRule {
				.rule = "expressionsSequence",
				.opener = TokenRule("stringExpressionOpen"),
				.closer = TokenRule("stringExpressionClosed"),
				.range = {1, 1},
				.normalize = true
			}}
		})},
		{"stringLiteral", NodeRule({
			{"segments", SequenceRule {
				.rule = VariantRule({
					"stringSegment",
					"stringExpression"
				}),
				.opener = TokenRule("stringOpen"),
				.closer = TokenRule("stringClosed")
			}}
		})},
		{"stringSegment", NodeRule({
			{"value", TokenRule("stringSegment")}
		})},
		{"structureBody", RuleRef()},
		{"structureDeclaration", NodeRule({
			{"modifiers", "modifiers", true},
			{nullopt, TokenRule("keywordStruct")},
			{"identifier", "identifier"},
			{"genericParameters", "genericParametersClause", true},
			{"inheritedTypes", "inheritedTypesClause", true},
			{"body", "structureBody", true}
		})},
		{"structureExpression", NodeRule({
			{nullopt, TokenRule("keywordStruct")},
			{"genericParameters", "genericParametersClause", true},
			{"inheritedTypes", "inheritedTypesClause", true},
			{"body", "structureBody", true}
		})},
		{"structureStatements", RuleRef()},
		{"subscriptArgumentsClause", SequenceRule {
			.rule = "argument",
			.opener = TokenRule("bracketOpen"),
			.delimiter = TokenRule("operator.*", ","),
			.closer = TokenRule("bracketClosed"),
			.innerDelimitRange = {1, usize(-1)}
		}},
		{"subscriptDeclaration", RuleRef()},
		{"subscriptExpression", NodeRule({
			{"composite", "postfixExpression"},
			{"genericArguments", "genericArgumentsClause", true},
			{"arguments", "subscriptArgumentsClause"},
			{"closure", "closureExpression", true}
		})},
		{"throwStatement", NodeRule({
			{nullopt, TokenRule("keywordThrow")},
			{"value", "expressionsSequence", true}
		})},
		{"tryExpression", NodeRule({
			{nullopt, TokenRule("keywordTry")},
			{"nillable", TokenRule("operatorPostfix", "\\?"), true},
			{"value", "expression"},
		})},
		{"type", VariantRule({
			"variadicType",
			"inoutType",
			"unionType",
			"typeIdentifier"
		})},
		{"typeClause", NodeRule({
			{nullopt, TokenRule("operator.*", ":")},
			{"type_", "type"}
		}, true)},
		{"typeExpression", NodeRule({
			{nullopt, TokenRule("keywordType")},
			{"type_", "type"}
		})},
		{"typeIdentifier", RuleRef("identifier")},
		{"unionType", RuleRef()},
		{"variableDeclaration", NodeRule({
			{"modifiers", "modifiers", true},
			{nullopt, TokenRule("keywordVar")},
			{"declarators", SequenceRule {
				.rule = "declarator",
				.delimiter = TokenRule("operator.*", ","),
				.innerDelimitRange = {1, usize(-1)},
				.dirty = false
			}}
		})},
		{"variadicType", NodeRule({
			{"value", VariantRule({
				"inoutType",
				"unionType"
			})},
			{nullopt, TokenRule("operator.*", "\\.\\.\\.")}
		})},
		{"whileStatement", RuleRef()}
	};
};