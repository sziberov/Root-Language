class Lexer {
	static code;
	static position;
	static tokens;
	static states;

	static rules = [
		['#!', (v) => {
			if(this.atComments || this.atString) {
				this.helpers.continueString();
				this.token.value += v;
			} else
			if(this.position === 0) {
				this.addToken('commentShebang', v);
				this.addState('comment');
			} else {
				return true;
			}
		}],
		[['/*', '*/'], (v) => {
			if(this.atComments || this.atString) {
				this.helpers.continueString();
				this.token.value += v;
			} else
			if(v === '/*') {
				this.addToken('commentBlock', v);
			}
			if(this.token.type === 'commentBlock') {
				if(v === '/*') {
					this.addState('comment');
				} else {
					this.removeState('comment');
				}
			}
		}],
		['//', (v) => {
			if(this.atComments || this.atString) {
				this.helpers.continueString();
				this.token.value += v;
			} else {
				this.addToken('commentLine', v);
				this.addState('comment');
			}
		}],
		[['\\\\', '\\\'', '\\b', '\\f', '\\n', '\\r', '\\t', '\\v'], (v) => {
			if(this.atComments) {
				this.token.value += v;

				return;
			}
			if(!this.atString) {
				return true;
			}

			this.helpers.continueString();
			this.token.value += ({
				'\\\\': '\\',
				'\\\'': '\'',
				'\\b': '\b',
				'\\f': '\f',
				'\\n': '\n',
				'\\r': '\r',
				'\\t': '\t',
				'\\v': '\v',
				'\\': ''
			})[v]
		}],
		[['\\(', ')'], (v) => {
			if(this.atComments) {
				this.token.value += v;

				return;
			}

			if(v === '\\(' && this.atString) {
				this.addToken('stringExpressionOpen', v);
				this.addState('stringExpression');
			} else
			if(v === ')' && this.atStringExpression) {
				this.addToken('stringExpressionClosed');
				this.removeState('stringExpression');
			} else {
				return true;
			}
		}],
		[['!', '%', '&', '*', '+', ',', '-', '.', '/', ':', '<', '=', '>', '?', '^', '|', '~'], (v) => {
			if(this.atComments || this.atString) {
				this.helpers.continueString();
				this.token.value += v;

				return;
			}

			let initializers = [',', '.', ':'],								// Create new token if value exists in the list and current (operator) token doesn't initialized with it
				singletons = ['!', '?'],									// Create new token if current (postfix operator) token matches any value in the list
				generics = ['!', '&', ',', '.', ':', '<', '>', '?', '|']	// Only values in the list are allowed for generic types

			let initializer = initializers.includes(v) && !this.token.value.startsWith(v),
				singleton = this.token.type === 'operatorPostfix' && singletons.includes(this.token.value),
				generic = generics.includes(v);

			if(!generic) {
				this.helpers.mergeOperators();
			}

			let closingAngle = this.atAngle && v === '>';

			if(this.token.type.startsWith('operator') && !initializer && !singleton && !closingAngle) {
				this.token.value += v;

				return;
			}

			let type = 'operator';

			if(singleton) {
				type = this.token.type;
			} else {
				if(
					['string', 'identifier'].includes(this.token.type) ||
					this.token.type.endsWith('Closed') ||
					this.token.type.startsWith('number') ||
					this.token.type.startsWith('keyword')
				) {
					type += 'Postfix';
				}

				this.helpers.specifyOperatorType();
			}

			this.addToken(type);
			if(initializer || singleton) {
				this.token.nonmergeable = true;
			}

			if(v === '<') this.addState('angle');
			if(v === '>') this.removeState('angle');
		}],
		[['(', ')', '[', ']', '{', '}'], (v) => {
			if(this.atComments || this.atString) {
				this.helpers.continueString();
				this.token.value += v;

				return;
			}

			let type = ({
				'(': 'parenthesisOpen',
				')': 'parenthesisClosed',
				'[': 'bracketOpen',
				']': 'bracketClosed',
				'{': 'braceOpen',
				'}': 'braceClosed'
			})[v]

			if(type.endsWith('Open')) {
				this.helpers.specifyOperatorType();
			}
			this.addToken(type);
			this.removeState('angle', 2);  // Balanced tokens except </> are allowed right after generic types but not inside them

			if(v === '{' && this.atStatement && this.atToken((t, v) => t === 'whitespace' && v.includes('\n'), this.ignorable, -1)) {
				this.addState('statementBody');

				return;
			}
			if(v === '}' && this.atStatementBody && !this.atFutureToken((t) => ['keywordElse', 'keywordWhere'].includes(t), this.ignorable)) {
				this.addToken('delimiter', ';');
				this.token.generated = true;
				this.removeState('statement');

				return;
			}

			if(this.atStringExpressions) {
				if(v === '(') this.addState('parenthesis');
				if(v === ')') this.removeState('parenthesis');
			}
			if(this.atStatements) {
				if(v === '{') this.addState('brace');
				if(v === '}') this.removeState('brace');
			}
		}],
		['\'', (v) => {
			if(this.atComments) {
				this.token.value += v;

				return;
			}

			if(!this.atString) {
				this.helpers.finalizeOperator();
				this.addToken('stringOpen');
				this.addState('string');
			} else {
				this.addToken('stringClosed');
				this.removeState('string');
			}
		}],
		[';', (v) => {
			if(this.atComments || this.atString) {
				this.helpers.continueString();
				this.token.value += v;

				return;
			}

			if(this.token.type === 'delimiter' && this.token.generated) {
				delete this.token.generated;
			} else {
				this.addToken('delimiter');
			}
		}],
		['\n', (v) => {
			if(this.atComments && !['commentShebang', 'commentLine'].includes(this.token.type) || this.atString) {
				this.helpers.continueString();
				this.token.value += v;

				return;
			}

			if(this.token.type !== 'whitespace') {
				this.addToken('whitespace', v);

				if(this.atComments) {
					this.removeState('comment');
				}
			} else {
				this.token.value += v;
			}

			if(this.atStatement && this.token.value.match(/\n/g).length === 1) {
				let braceOpen = (t) => t === 'braceOpen';

				if(this.atToken(braceOpen, this.ignorable)) {
					this.addState('statementBody');
				} else
				if(!this.atState('brace', []) && !this.atFutureToken(braceOpen, this.ignorable)) {
					this.removeState('statement');
				}
			}
		}],
		[/[^\S\n]+/g, (v) => {
			if(this.atComments || this.atString || this.token.type === 'whitespace') {
				this.helpers.continueString();
				this.token.value += v;

				return;
			}

			this.addToken('whitespace', v);
		}],
		[/[0-9]+/g, (v) => {
			if(this.atComments || this.atString) {
				this.helpers.continueString();
				this.token.value += v;

				return;
			}
			if(this.token.type === 'operatorPostfix' && this.token.value === '.' && this.getToken(-1).type === 'numberInteger') {
				this.removeToken();
				this.token.type = 'numberFloat';
				this.token.value += '.'+v;

				return;
			}

			this.helpers.finalizeOperator();
			this.addToken('numberInteger', v);
		}],
		[/[a-z_$][a-z0-9_$]*/gi, (v) => {
			if(this.atComments || this.atString) {
				this.helpers.continueString();
				this.token.value += v;

				return;
			}

			let keywords = [
				// Flow-related words (async, class, for, return...)
				// Literals (false, nil, true...)
				// Types (Any, bool, _, ...)

				// Words as Self, arguments, metaSelf or self are not allowed because they all
				// have dynamic values and it's logical to distinguish them at the interpretation stage

				'Any',
				'Class',
				'Enumeration',
				'Function',
				'Namespace',
				'Object',
				'Protocol',
				'Structure',
				'any', 'async', 'await', 'awaits',
				'bool', 'break',
				'case', 'catch', 'class', 'continue',
				'dict', 'do',
				'else', 'enum', 'extension',
				'fallthrough', 'false', 'final', 'float', 'for', 'func',
				'if', 'import', 'in', 'infix', 'inout', 'int', 'is',
				'lazy',
				'namespace', 'nil',
				'operator',
				'postfix', 'prefix', 'private', 'protected', 'protocol', 'public',
				'return',
				'static', 'string', 'struct',
				'throw', 'throws', 'true', 'try', 'type',
				'var', 'virtual', 'void',
				'when', 'where', 'while',
				'_'
			]

			this.helpers.specifyOperatorType();

			let type = 'identifier',
				chain = this.atToken((t, v) => t.startsWith('operator') && !t.endsWith('Postfix') && v === '.', this.ignorable);

			if(keywords.includes(v) && !chain) {  // Disable keywords in a chains
				type = 'keyword';

				if(v === '_') {
					type += 'Underscore';
				} else {
					let capitalizedV = v[0].toUpperCase()+v.slice(1);

					if(v[0] === capitalizedV[0]) {
						type += 'Capital';
					}

					type += capitalizedV;
				}
			}

			if(this.atAngle && type.startsWith('keyword')) {  // No keywords are allowed in generic types
				this.helpers.mergeOperators();
			}
			this.addToken(type, v);

			// Blocks in some statements can be treated by parser like expressions first, which can lead to false inclusion of futher tokens into that expression and late closing
			// This can be fixed by tracking and closing them beforehand at lexing stage
			if(['For', 'If', 'When', 'While'].some(v => type.endsWith(v))) {
				this.addState('statement');
			}
		}],
		[/./g, (v) => {
			if(this.atComments || this.atString || this.token.type === 'unsupported') {
				this.helpers.continueString();
				this.token.value += v;

				return;
			}

			this.addToken('unsupported', v);
		}]
	]

	static helpers = {
		continueString: () => {
			if(['stringOpen', 'stringExpressionClosed'].includes(this.token.type)) {
				this.addToken('stringSegment', '');
			}
		},
		mergeOperators: () => {
			this.removeState('angle', 2);

			for(let i = this.tokens.length; i >= 0; i--) {
				if(
					!this.token.type.startsWith('operator') || this.token.nonmergeable ||
					!this.getToken(-1).type.startsWith('operator') || this.getToken(-1).nonmergeable
				) {
					break;
				}

				this.getToken(-1).value += this.token.value;
				this.removeToken();
			}
		},
		specifyOperatorType: () => {
			this.token.type = {
				operator: 'operatorPrefix',
				operatorPostfix: 'operatorInfix'
			}[this.token.type] ?? this.token.type;
		},
		finalizeOperator: () => {
			this.helpers.mergeOperators();
			this.helpers.specifyOperatorType();
		}
	}

	static get codeEnd() {
		return this.position >= this.code.length;
	}

	static get token() {
		return this.getToken();
	}

	static get location() {
		let location = {
			line: 0,
			column: 0,
			...(this.token.location ?? {})
		}

		for(let position = this.token.position ?? 0; position < this.position; position++) {
			let character = this.code[position]

			if(character === '\n') {
				location.line++;
				location.column = 0;
			} else {
				location.column += character === '\t' ? 4 : 1;
			}
		}

		return location;
	}

	static get atComments() {
		return this.atState('comment');
	}

	static get atString() {
		return this.atState('string', []);
	}

	static get atStringExpression() {
		return this.atState('stringExpression', []);
	}

	static get atStringExpressions() {
		return this.atState('stringExpression');
	}

	static get atStatement() {
		return this.atState('statement', ['brace']);
	}

	static get atStatements() {
		return this.atState('statement');
	}

	static get atStatementBody() {
		return this.atState('statementBody', []);
	}

	static get atAngle() {
		return this.atState('angle', []);
	}

	static ignorable = (t) => t === 'whitespace' || t.startsWith('comment');

	static getToken(offset = 0) {
		return this.tokens[this.tokens.length-1+offset] ?? {
			position: undefined,
			location: undefined,
			type: '',
			value: ''
		}
	}

	static addToken(type, value) {
		this.tokens.push({
			position: this.position,
			location: this.location,
			type: type,
			value: value ?? this.code[this.position]
		});
	}

	static removeToken(offset = 0) {
		this.tokens = this.tokens.filter((v, k) => k !== this.tokens.length-1+offset);
	}

	/*
	 * Check for token(s) starting from rightmost (inclusively if no offset set), using combinations of type and value within predicate.
	 *
	 * Strict by default, additionally whitelist predicate can be set.
	 *
	 * Useful for complex token sequences check.
	 */
	static atToken(conforms, whitelisted, offset = 0) {
		for(let i = this.tokens.length-1+offset; i >= 0; i--) {
			let token = this.tokens[i],
				type = token.type,
				value = token.value;

			if(conforms(type, value)) {
				return true;
			}
			if(whitelisted != null && !whitelisted(type, value)) {
				return;
			}
		}
	}

	/*
	 * Future-time version of atToken(). Rightmost (at the moment) token is not included in a search.
	 */
	static atFutureToken(conforms, whitelisted) {
		let save = this.getSave(),
			result;

		this.position = this.token.position+this.token.value.length;  // Override allows nested calls

		while(!this.codeEnd) {
			this.nextToken();

			let token = this.tokens.at(-1),
				type = token?.type,
			    value = token?.value;

			if(conforms(type, value)) {
				result = true;

				break;
			}
			if(whitelisted != null && !whitelisted(type, value)) {
				break;
			}
		}

		this.restoreSave(save);

		return result;
	}

	static addState(type) {
		this.states.push(type);
	}

	/*
	 * 0 - Remove states starting from rightmost found (inclusively)
	 * 1 - 0 with subtypes, ignoring nested
	 * 2 - Remove found states globally
	 */
	static removeState(type, mode = 0) {
		if(mode === 0) {
			let i = this.states.lastIndexOf(type);

			if(i > -1) {
				this.states.length = i;
			}
		}
		if(mode === 1) {
			for(let i = this.states.length-1; i >= 0; i--) {
				let type_ = this.states[i]

				if(type_.startsWith(type)) {
					this.states.splice(i, 1);
				}
				if(type_ === type) {
					break;
				}
			}
		}
		if(mode === 2) {
			this.states = this.states.filter(v => v !== type);
		}
	}

	/*
	 * Check for state starting from rightmost (inclusively).
	 *
	 * Unstrict by default, additionaly whitelist can be set.
	 */
	static atState(type, whitelist) {
		for(let i = this.states.length-1; i >= 0; i--) {
			if(this.states[i] === type) {
				return true;
			}
			if(whitelist != null && !whitelist.includes(this.states[i])) {
				return;
			}
		}
	}

	static getSave() {
		return {
			position: this.position,
			tokens: JSON.stringify(this.tokens),
			states: JSON.stringify(this.states)
		}
	}

	static restoreSave(save) {
		this.position = save.position;
		this.tokens = JSON.parse(save.tokens);
		this.states = JSON.parse(save.states);
	}

	static reset() {
		this.code = '';
		this.position = 0;
		this.tokens = []
		this.states = []
			// angle - used to distinguish between common operator and generic type's closing >
			// brace - used in statements
			// parenthesis - used in string expressions
	}

	static atSubstring(substring) {
		return this.code.indexOf(substring, this.position) === this.position;
	}

	static atRegex(regex) {
		let substring;

		if(!regex.global) {
			regex = new RegExp(regex.source, 'g');
		}

		regex.lastIndex = this.position;
		substring = regex.exec(this.code);
		substring = substring?.index === this.position ? substring[0] : undefined;

		return substring;
	}

	static nextToken() {
		for(let rule of this.rules) {
			let triggers = rule[0],
				actions = rule[1],
				plain = typeof triggers === 'string',
				array = Array.isArray(triggers),
				regex = triggers instanceof RegExp,
				trigger;

			if(plain && this.atSubstring(triggers)) {
				trigger = triggers;
			}
			if(array) {
				trigger = triggers.find(v => this.atSubstring(v));
			}
			if(regex) {
				trigger = this.atRegex(triggers);
			}

			if(trigger != null && !actions(trigger)) {  // Multiple rules can be executed on same position if some of them return true
				this.position += trigger.length;  // Rules shouldn't explicitly set position

				break;
			}
		}
	}

	static tokenize(code) {
		this.reset();

		this.code = code;

		while(!this.codeEnd) {
			this.nextToken();  // Zero-length position commits will lead to forever loop, rules developer attention is advised
		}

		let result = {
			rawTokens: this.tokens,
			tokens: this.tokens.filter(v => !this.ignorable(v.type))
		}

		this.reset();

		return result;
	}
}