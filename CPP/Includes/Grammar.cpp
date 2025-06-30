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

	struct Field {
		Field(const char* t, Rule r = "[placeholder]", bool o = false) : title(string(t)), rule(r), optional(o) {}
		Field(const string t, Rule r = "[placeholder]", bool o = false) : title(t), rule(r), optional(o) {}
		Field(const nullopt_t t, Rule r = "[placeholder]", bool o = false) : title(t), rule(r), optional(o) {}

		std::optional<string> title;  // Nil for implicit
		Rule rule;
		bool optional = false;

		bool operator==(const Field& f) const = default;
	};

	struct NodeRule {
		NodeRule(initializer_list<Field> f, bool n = false, function<void(Node&)> p = nullptr) : fields(f), normalize(n), post(p) {}

		vector<Field> fields;
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

		// Ascender and descender can be used to enable the dirt-tolerance:
		// Parser will be aware of the nesting level and have a chance to close the rule before EOF.
		// Unrecognized tokens will be included into the result.
		optional<Rule> ascender,
					   delimiter,
					   descender;

		// Examples: 0/0, 1/1, 0/1, 1/max, 0/max.
		usize range[2] = {0, usize(-1)},
			  innerDelimitRange[2] = {0, usize(-1)},
			  outerDelimitRange[2] = {0, usize(-1)};

		bool delimited = false,  // Include delimiters if any
			 normalize = false;  // Unwrap single value (or nil)

		bool operator==(const SequenceRule& r) const = default;
	};

	// ----------------------------------------------------------------

	// FIXME:
	// - No parsing messages and their rollback mechanism
	// - No "dirty tokens" node support in sequence rule parser
	// - No ternary logic of modifiers in function signatures
	// - Function parameters can't be stated without label using optionals only (node rule parser does not support optional fields branching)
	// - Expressions in statements like "if a {}" can be parsed like "if (call a with closure) then do nothing" instead of "if a then do block"

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
			{nullopt, TokenRule("bracketOpen")},
			{"values", SequenceRule {
				.rule = "expressionsSequence",
				.delimiter = TokenRule("operator.*", ","),
				.innerDelimitRange = {1, usize(-1)}
			}},
			{nullopt, TokenRule("bracketClosed|endOfFile")}
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
		{"body", "[placeholder]"},
		{"booleanLiteral", NodeRule({
			{"value", TokenRule("keywordFalse|keywordTrue")}
		})},
		{"breakStatement", NodeRule({
			{nullopt, TokenRule("keywordBreak")},
			{"label", "identifier", true}
		})},
		{"callArgumentsClause", NodeRule({
			{nullopt, TokenRule("parenthesisOpen")},
			{"arguments", SequenceRule {
				.rule = "argument",
				.ascender = TokenRule("parenthesisOpen"),
				.delimiter = TokenRule("operator.*", ","),
				.descender = TokenRule("parenthesisClosed"),
				.innerDelimitRange = {1, usize(-1)}
			}},
			{nullopt, TokenRule("parenthesisClosed|endOfFile")}
		}, true)},
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
		{"caseDeclaration", "[placeholder]"},
		{"catchClause", "[placeholder]"},
		{"chainDeclaration", "[placeholder]"},
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
		{"chainStatements", "[placeholder]"},
		{"classBody", "[placeholder]"},
		{"classDeclaration", "[placeholder]"},
		{"classExpression", "[placeholder]"},
		{"classStatements", "[placeholder]"},
		{"closureExpression", NodeRule({
			{nullopt, TokenRule("braceOpen")},
			{"signature", "closureSignature", true},
			{"statements", "functionStatements", true},
			{nullopt, TokenRule("braceClosed|endOfFile")}
		})},
		{"closureSignature", NodeRule({
			{"signature", "functionSignature"},
			{nullopt, TokenRule("keywordIn")}
		}, true)},
		{"conditionalOperator", NodeRule({
			{nullopt, TokenRule("operator.*", "\\?")},
			{"value", "expressionsSequence"},
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
		{"deinitializerDeclaration", "[placeholder]"},
		{"deleteExpression", NodeRule({
			{nullopt, TokenRule("identifier", "delete")},
			{"value", "expression"}
		})},
		{"dictionaryLiteral", "[placeholder]"},
		{"dictionaryType", "[placeholder]"},
		{"doStatement", "[placeholder]"},
		{"elseClause", "[placeholder]"},
		{"entry", NodeRule({
			{"key", "expressionsSequence"},
			{nullopt, TokenRule("operator.*", ":")},
			{"value", "expressionsSequence"}
		})},
		{"enumerationBody", "[placeholder]"},
		{"enumerationDeclaration", "[placeholder]"},
		{"enumerationExpression", "[placeholder]"},
		{"enumerationStatements", "[placeholder]"},
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
					.delimited = true
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
		{"forStatement", "[placeholder]"},
		{"functionBody", NodeRule({
			{nullopt, TokenRule("braceOpen")},
			{"statements", "functionStatements"},
			{nullopt, TokenRule("braceClosed|endOfFile")}
		})},
		{"functionDeclaration", NodeRule({
			{"modifiers", "modifiers", true},
			{nullopt, TokenRule("keywordFunc")},
			{"identifier", VariantRule({
				"identifier",
				"operator"
			})},
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
			// TODO: Modifiers (awaits, async)
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
			.ascender = TokenRule("braceOpen"),
			.delimiter = TokenRule("delimiter"),
			.descender = TokenRule("braceClosed")
		}},
		{"functionType", "[placeholder]"},
		{"genericArgument", "type"},
		{"genericArgumentsClause", NodeRule({
			{nullopt, TokenRule("operator.*", "<")},
			{"genericArguments", SequenceRule {
				.rule = "genericArgument",
				.delimiter = TokenRule("operator.*", ","),
				.innerDelimitRange = {1, usize(-1)}
			}},
			{nullopt, VariantRule({
				TokenRule("operator.*", ">"),
				TokenRule("endOfFile")
			})}
		}, true)},
		{"genericParameter", NodeRule({
			{"identifier", "identifier"},
			{"type_", "typeClause", true}
		})},
		{"genericParametersClause", NodeRule({
			{nullopt, TokenRule("operator.*", "<")},
			{"genericParameters", SequenceRule {
				.rule = "genericParameter",
				.ascender = TokenRule("operator.*", "<"),
				.delimiter = TokenRule("operator.*", ","),
				.descender = TokenRule("operator.*", ">"),
				.innerDelimitRange = {1, usize(-1)}
			}},
			{nullopt, VariantRule({
				TokenRule("operator.*", ">"),
				TokenRule("endOfFile")
			})}
		}, true)},
		{"identifier", NodeRule({
			{"value", TokenRule("identifier")}
		})},
		{"ifStatement", "[placeholder]"},
		{"implicitChainExpression", NodeRule({
			{nullopt, TokenRule("operator|operatorPrefix|operatorInfix", "\\.")},
			{"member", VariantRule({
				"identifier",
				"stringLiteral"
			})}
		})},
		{"implicitChainIdentifier", NodeRule({
			{nullopt, TokenRule("operator|operatorPrefix|operatorInfix", "\\.")},
			{"value", "identifier"}
		})},
		{"importDeclaration", "[placeholder]"},
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
				.innerDelimitRange = {1, usize(-1)}
			}}
		}, true)},
		{"initializerClause", NodeRule({
			{nullopt, TokenRule("operator.*", "=")},
			{"value", "expressionsSequence", true}
		}, true)},
		{"initializerDeclaration", "[placeholder]"},
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
		{"intersectionType", "[placeholder]"},
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
			})
		}},
		{"module", NodeRule({
			{"statements", SequenceRule {
				.rule = "functionStatement",
				.delimiter = TokenRule("delimiter"),
				.descender = TokenRule("endOfFile")
			}}
		})},
		{"namespaceBody", "[placeholder]"},
		{"namespaceDeclaration", "[placeholder]"},
		{"namespaceExpression", "[placeholder]"},
		{"namespaceStatements", "[placeholder]"},
		{"nillableExpression", NodeRule({
			{"value", "postfixExpression"},
			{nullopt, TokenRule("operatorPostfix", "\\?")}
		})},
		{"nillableType", NodeRule({
			{"value", "postfixType"},
			{nullopt, TokenRule("operatorPostfix", "\\?")}
		})},
		{"nilLiteral", "[placeholder]"},
		{"observerDeclaration", "[placeholder]"},
		{"observersBody", "[placeholder]"},
		{"observersStatements", "[placeholder]"},
		{"operator", "[placeholder]"},
		{"operatorBody", "[placeholder]"},
		{"operatorDeclaration", "[placeholder]"},
		{"operatorStatements", "[placeholder]"},
		/* TODO: Optionals branching
		{"parameter", NodeRule({
			{"label", "identifier", true},
			{"identifier", "identifier"},
			{"type_", "typeClause", true},
			{"value", "initializerClause", true}
		})},
		*/
		{"parameter", VariantRule({
			NodeRule({
				{"label", "identifier"},
				{"identifier", "identifier"},
				{"type_", "typeClause", true},
				{"value", "initializerClause", true}
			}),
			NodeRule({
				{"identifier", "identifier"},
				{"type_", "typeClause", true},
				{"value", "initializerClause", true}
			})
		})},
		{"parametersClause", NodeRule({
			{nullopt, TokenRule("parenthesisOpen")},
			{"parameters", SequenceRule {
				.rule = "parameter",
				.ascender = TokenRule("parenthesisOpen"),
				.delimiter = TokenRule("operator.*", ","),
				.descender = TokenRule("parenthesisClosed"),
				.innerDelimitRange = {1, usize(-1)}
			}},
			{nullopt, TokenRule("parenthesisClosed|endOfFile")}
		}, true)},
		{"parenthesizedExpression", NodeRule({
			{nullopt, TokenRule("parenthesisOpen")},
			{"value", SequenceRule {
				.rule = "expressionsSequence",
				.ascender = TokenRule("parenthesisOpen"),
				.descender = TokenRule("parenthesisClosed"),
				.range = {1, 1},
				.normalize = true
			}},
			{nullopt, TokenRule("parenthesisClosed|endOfFile")}
		})},
		{"parenthesizedType", "[placeholder]"},
		{"postfixExpression", VariantRule({
			"callExpression",
			"chainExpression",
			"defaultExpression",
			"nillableExpression",
			"subscriptExpression",
			"primaryExpression",
			NodeRule({
				{"value", "postfixExpression"},
				{"operator", "postfixOperator"}
			})
		})},
		{"postfixOperator", NodeRule({
			{"value", TokenRule("operatorPostfix", "[^,:]*")}  // Enclosing nodes can use trailing operators from the exceptions list
		})},
		{"postfixType", VariantRule({
			"defaultType",
			"nillableType",
			"primaryType"
		})},
		{"predefinedType", "[placeholder]"},
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
		{"protocolBody", "[placeholder]"},
		{"protocolDeclaration", "[placeholder]"},
		{"protocolExpression", "[placeholder]"},
		{"protocolStatements", "[placeholder]"},
		{"protocolType", "[placeholder]"},
		{"returnClause", NodeRule({
			{nullopt, TokenRule("operator.*", "->")},
			{"type_", "type"}
		}, true)},
		{"returnStatement", NodeRule({
			{nullopt, TokenRule("keywordReturn")},
			{"value", "expressionsSequence", true}
		})},
		{"statement", "[placeholder]"},
		{"stringExpression", NodeRule({
			{nullopt, TokenRule("stringExpressionOpen")},
			{"value", SequenceRule {
				.rule = "expressionsSequence",
				.ascender = TokenRule("stringExpressionOpen"),
				.descender = TokenRule("stringExpressionClosed"),
				.range = {1, 1},
				.normalize = true
			}},
			{nullopt, TokenRule("stringExpressionClosed|endOfFile")}
		})},
		{"stringLiteral", NodeRule({
			{nullopt, TokenRule("stringOpen")},
			{"segments", SequenceRule {
				.rule = VariantRule({
					"stringSegment",
					"stringExpression"
				}),
				.ascender = TokenRule("stringOpen"),
				.descender = TokenRule("stringClosed")
			}},
			{nullopt, TokenRule("stringClosed|endOfFile")}
		})},
		{"stringSegment", NodeRule({
			{"value", TokenRule("stringSegment")}
		})},
		{"structureBody", "[placeholder]"},
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
		{"structureStatements", "[placeholder]"},
		{"subscriptArgumentsClause", NodeRule({
			{nullopt, TokenRule("bracketOpen")},
			{"arguments", SequenceRule {
				.rule = "argument",
				.ascender = TokenRule("bracketOpen"),
				.delimiter = TokenRule("operator.*", ","),
				.descender = TokenRule("bracketClosed"),
				.innerDelimitRange = {1, usize(-1)}
			}},
			{nullopt, TokenRule("bracketClosed|endOfFile")}
		}, true)},
		{"subscriptDeclaration", "[placeholder]"},
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
		{"unionType", "[placeholder]"},
		{"variableDeclaration", NodeRule({
			{"modifiers", "modifiers", true},
			{nullopt, TokenRule("keywordVar")},
			{"declarators", SequenceRule {
				.rule = "declarator",
				.delimiter = TokenRule("operator.*", ","),
				.innerDelimitRange = {1, usize(-1)}
			}}
		})},
		{"variadicType", NodeRule({
			{"value", VariantRule({
				"inoutType",
				"unionType"
			})},
			{nullopt, TokenRule("operator.*", "\\.\\.\\.")}
		})},
		{"whileStatement", "[placeholder]"}
	};
};