class Parser {
	static tokens;
	static #position;
	static reports;

	static rules = {
		argument: () => {
			let node = {
				type: 'argument',
				range: {
					start: this.position
				},
				label: this.rules.identifier(),
				value: undefined
			}

			if(node.label != null && this.token.type.startsWith('operator') && this.token.value === ':') {
				this.position++;
			} else {
				this.position = node.range.start;
				node.label = undefined;
			}

			node.value = this.rules.expressionsSequence();

			if(node.label == null && node.value == null) {
				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		arrayLiteral: () => {
			let node = {
				type: 'arrayLiteral',
				range: {},
				values: []
			}

			if(this.token.type !== 'bracketOpen') {
				return;
			}

			node.range.start = this.position++;
			node.values = this.helpers.sequentialNodes(
				['expressionsSequence'],
				() => this.token.type.startsWith('operator') && this.token.value === ','
			);

			if(this.token.type !== 'bracketClosed') {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position++;

			return node;
		},
		arrayType: () => {
			let node = {
				type: 'arrayType',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'bracketOpen') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.type();

			if(this.token.type !== 'bracketClosed') {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position++;

			return node;
		},
		asyncExpression: () => {
			let node = {
				type: 'asyncExpression',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'keywordAsync') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.expression();

			if(node.value == null) {
				this.position--;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		awaitExpression: () => {
			let node = {
				type: 'awaitExpression',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'keywordAwait') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.expression();

			if(node.value == null) {
				this.position--;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		body: (type) => {
			let node = {
				type: type+'Body',
				range: {},
				statements: []
			}

			if(this.token.type !== 'braceOpen') {
				return;
			}

			node.range.start = this.position++;
			node.statements = this.rules[type+'Statements']?.();

			if(node.statements.length === 0) {
				this.report(0, node.range.start, node.type, 'No statements.');
			}

			if(!this.tokensEnd) {
				node.range.end = this.position++;
			} else {
				node.range.end = this.position-1;

				this.report(1, node.range.start, node.type, 'Node doesn\'t have the closing brace and was decided to be autoclosed at the end of stream.');
			}

			return node;
		},
		booleanLiteral: () => {
			let node = {
				type: 'booleanLiteral',
				range: {},
				value: undefined
			}

			if(!['keywordFalse', 'keywordTrue'].includes(this.token.type)) {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		breakStatement: () => {
			let node = {
				type: 'breakStatement',
				range: {},
				label: undefined
			}

			if(this.token.type !== 'keywordBreak') {
				return;
			}

			node.range.start = this.position++;
			node.label = this.rules.identifier();
			node.range.end = this.position-1;

			return node;
		},
		callExpression: (node_) => {
			let node = {
				type: 'callExpression',
				range: {
					start: node_.range.start
				},
				callee: node_,
				genericArguments: [],
				arguments: [],
				closure: undefined
			}

			if(this.token.type.startsWith('operator') && this.token.value === '<') {
				this.position++;
				node.genericArguments = this.helpers.sequentialNodes(
					['type'],
					() => this.token.type.startsWith('operator') && this.token.value === ','
				);

				if(this.token.type.startsWith('operator') && this.token.value === '>') {
					this.position++;
				} else {
					this.position = node_.range.end+1;
				}
			}

			node.range.end = this.position-1;

			if(this.token.type === 'parenthesisOpen') {
				this.position++;
				node.arguments = this.helpers.skippableNodes(
					['argument'],
					() => this.token.type === 'parenthesisOpen',
					() => this.token.type === 'parenthesisClosed',
					() => this.token.type.startsWith('operator') && this.token.value === ','
				);

				if(this.token.type === 'parenthesisClosed') {
					this.position++;
				} else {
					node.range.end = this.position-1;

					this.report(1, node.range.start, node.type, 'Node doesn\'t have the closing parenthesis and was decided to be autoclosed at the end of stream.');

					return node;
				}
			}

			node.closure = this.rules.closureExpression();

			if(node.closure == null && node.range.end === this.position-1) {
				this.position = node_.range.end+1;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		caseDeclaration: () => {
			let node = {
				type: 'caseDeclaration',
				range: {},
				identifiers: []
			}

			if(this.token.type !== 'keywordCase') {
				return;
			}

			node.range.start = this.position++;
			node.identifiers = this.helpers.sequentialNodes(
				['identifier'],
				() => this.token.type.startsWith('operator') && this.token.value === ','
			);

			if(node.identifiers.length === 0) {
				this.report(0, node.range.start, node.type, 'No identifiers(s).');
			}

			node.range.end = this.position-1;

			return node;
		},
		catchClause: () => {
			let node = {
				type: 'catchClause',
				range: {},
				typeIdentifiers: [],
				body: undefined,
				catch: undefined
			}

			if(this.token.type !== 'keywordCatch') {
				return;
			}

			node.range.start = this.position++;
			node.typeIdentifiers = this.helpers.sequentialNodes(
				['typeIdentifier'],
				() => this.token.type.startsWith('operator') && this.token.value === ','
			);
			node.body = this.rules.functionBody();
			node.catch = this.rules.catchClause();

			if(node.typeIdentifiers.length === 0) {
				this.report(0, node.range.start, node.type, 'No type identifiers.');
			}
			if(node.body == null) {
				this.report(1, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		chainDeclaration: () => {
			let node = {
				type: 'chainDeclaration',
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				body: undefined
			}

			if(this.token.type !== 'identifier' || this.token.value !== 'chain') {
				this.position = node.range.start;

				return;
			}

			node.range.start = this.position++;
			node.body = this.rules.observersBody();

			if(node.modifiers.some(v => v !== 'static')) {
				this.report(1, node.range.start, node.type, 'Can only have specific modifier (static).');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		chainExpression: (node_) => {
			let node = {
				type: 'chainExpression',
				range: {
					start: node_.range.start
				},
				composite: node_,
				member: undefined
			}

			if(!['operator', 'operatorInfix'].includes(this.token.type) || this.token.value !== '.') {
				return;
			}

			this.position++;
			node.member =
				this.rules.identifier() ??
				this.rules.stringLiteral();

			if(node.member == null) {
				this.position = node_.range.end+1;

				return;
			}

			node.range.end = node.member.range.end;

			return node;
		},
		chainIdentifier: (node_) => {
			let node = {
				type: 'chainIdentifier',
				range: {
					start: node_.range.start
				},
				supervalue: node_,
				value: undefined
			}

			if(!['operatorPrefix', 'operatorInfix'].includes(this.token.type) || this.token.value !== '.') {
				return;
			}

			this.position++;
			node.value = this.rules.identifier();

			if(node.value == null) {
				this.position = node_.range.end+1;

				return;
			}

			node.range.end = node.value.range.end;

			return node;
		},
		chainStatements: () => {
			return this.rules.statements(['observerDeclaration']);
		},
		classBody: () => {
			return this.rules.body('class');
		},
		classDeclaration: (anonymous) => {
			let node = {
				type: 'class'+(!anonymous ? 'Declaration' : 'Expression'),
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				identifier: undefined,
				genericParameters: undefined,
				inheritedTypes: undefined,
				body: undefined
			}

			if(this.token.type !== 'keywordClass') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.identifier = this.rules.identifier();

			if(anonymous) {
				if(node.modifiers.length > 0 || node.identifier != null) {
					this.position = node.range.start;

					return;
				}

				delete node.modifiers;
				delete node.identifier;
			} else
			if(node.identifier == null) {
				this.report(2, node.range.start, node.type, 'No identifier.');
			}

			node.genericParameters = this.rules.genericParametersClause();
			node.inheritedTypes = this.rules.inheritedTypesClause();
			node.body = this.rules.classBody();

			if(node.modifiers?.some(v => !['private', 'protected', 'public', 'static', 'final'].includes(v))) {
				this.report(1, node.range.start, node.type, 'Wrong modifier(s).');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		classExpression: () => {
			return this.rules.classDeclaration(true);
		},
		classStatements: () => {
			return this.rules.statements([
				'chainDeclaration',
				'classDeclaration',
				'deinitializerDeclaration',
				'enumerationDeclaration',
				'functionDeclaration',
				'initializerDeclaration',
				'namespaceDeclaration',
				'protocolDeclaration',
				'structureDeclaration',
				'subscriptDeclaration',
				'variableDeclaration'
			]);
		},
		closureExpression: () => {
			let node = {
				type: 'closureExpression',
				range: {},
				signature: undefined,
				statements: []
			}

			if(this.token.type !== 'braceOpen') {
				return;
			}

			node.range.start = this.position++;
			node.signature = this.rules.functionSignature();

			if(node.signature != null) {
				if(this.token.type !== 'keywordIn') {
					this.position = node.range.start;

					return;
				}

				this.position++;
			} else {
				this.report(0, node.range.start, node.type, 'No signature.');
			}

			node.statements = this.rules.functionStatements();

			if(node.statements.length === 0) {
				this.report(0, node.range.start, node.type, 'No statements.');
			}

			if(!this.tokensEnd) {
				node.range.end = this.position++;
			} else {
				node.range.end = this.position-1;

				this.report(1, node.range.start, node.type, 'Node doesn\'t have the closing brace and was decided to be autoclosed at the end of stream.');
			}

			return node;
		},
		conditionalOperator: () => {
			let node = {
				type: 'conditionalOperator',
				range: {},
				expression: undefined
			}

			if(!this.token.type.startsWith('operator') || this.token.value !== '?') {
				return;
			}

			node.range.start = this.position++;
			node.expression = this.rules.expressionsSequence();

			if(!this.token.type.startsWith('operator') || this.token.value !== ':') {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position++;

			return node;
		},
		continueStatement: () => {
			let node = {
				type: 'continueStatement',
				range: {},
				label: undefined
			}

			if(this.token.type !== 'keywordContinue') {
				return;
			}

			node.range.start = this.position++;
			node.label = this.rules.identifier();
			node.range.end = this.position-1;

			return node;
		},
		controlTransferStatement: () => {
			return (
				this.rules.breakStatement() ??
				this.rules.continueStatement() ??
				this.rules.fallthroughStatement() ??
				this.rules.returnStatement() ??
				this.rules.throwStatement()
			);
		},
		declaration: () => {
			return (
				this.rules.chainDeclaration() ??
				this.rules.classDeclaration() ??
				this.rules.deinitializerDeclaration() ??
				this.rules.enumerationDeclaration() ??
				this.rules.functionDeclaration() ??
				this.rules.importDeclaration() ??
				this.rules.initializerDeclaration() ??
				this.rules.namespaceDeclaration() ??
				this.rules.operatorDeclaration() ??
				this.rules.protocolDeclaration() ??
				this.rules.structureDeclaration() ??
				this.rules.subscriptDeclaration() ??
				this.rules.variableDeclaration()
			);
		},
		declarator: () => {
			let node = {
				type: 'declarator',
				range: {
					start: this.position
				},
				identifier: this.rules.identifier(),
				type_: undefined,
				value: undefined,
				body: undefined
			}

			if(node.identifier == null) {
				return;
			}

			node.type_ = this.rules.typeClause();
			node.value = this.rules.initializerClause();

			this.helpers.bodyTrailedValue(node, 'value', 'body', false, () => this.rules.observersBody(true) ?? this.rules.functionBody());

			node.range.end = this.position-1;

			return node;
		},
		defaultExpression: (node_) => {
			let node = {
				type: 'defaultExpression',
				range: {
					start: node_.range.start
				},
				value: node_
			}

			if(this.token.type !== 'operatorPostfix' || this.token.value !== '!') {
				return;
			}

			node.range.end = this.position++;

			return node;
		},
		defaultType: (node_) => {
			let node = {
				type: 'defaultType',
				range: {
					start: node_.range.start
				},
				value: node_
			}

			if(this.token.type !== 'operatorPostfix' || this.token.value !== '!') {
				return;
			}

			node.range.end = this.position++;

			return node;
		},
		deinitializerDeclaration: () => {
			let node = {
				type: 'deinitializerDeclaration',
				range: {},
				body: undefined
			}

			if(this.token.type !== 'identifier' || this.token.value !== 'deinit') {
				return;
			}

			this.position++;
			node.body = this.rules.functionBody();

			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		deleteExpression: () => {
			let node = {
				type: 'deleteExpression',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'identifier' || this.token.value !== 'delete') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.expression();

			if(node.value == null) {
				this.position--;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		dictionaryLiteral: () => {
			let node = {
				type: 'dictionaryLiteral',
				range: {},
				entries: []
			}

			if(this.token.type !== 'bracketOpen') {
				return;
			}

			node.range.start = this.position++;
			node.entries = this.helpers.sequentialNodes(
				['entry'],
				() => this.token.type.startsWith('operator') && this.token.value === ','
			);

			if(node.entries.length === 0 && this.token.type.startsWith('operator') && this.token.value === ':') {
				this.position++;
			}

			if(this.token.type !== 'bracketClosed') {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position++;

			return node;
		},
		dictionaryType: () => {
			let node = {
				type: 'dictionaryType',
				range: {},
				key: undefined,
				value: undefined
			}

			if(this.token.type !== 'bracketOpen') {
				return;
			}

			node.range.start = this.position++;
			node.key = this.rules.type();

			if(!this.token.type.startsWith('operator') || this.token.value !== ':') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.value = this.rules.type();

			if(this.token.type !== 'bracketClosed') {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position++;

			return node;
		},
		doStatement: () => {
			let node = {
				type: 'doStatement',
				range: {},
				body: undefined,
				catch: undefined
			}

			if(this.token.type !== 'keywordDo') {
				return;
			}

			node.range.start = this.position++;
			node.body = this.rules.functionBody();
			node.catch = this.rules.catchClause();

			if(node.body == null) {
				this.report(2, node.range.start, node.type, 'No body.');
			}
			if(node.catch == null) {
				this.report(1, node.range.start, node.type, 'No catch.');
			}

			node.range.end = this.position-1;

			return node;
		},
		elseClause: () => {
			let node,
				start = this.position;

			if(this.token.type !== 'keywordElse') {
				return;
			}

			this.position++;
			node =
				this.rules.functionBody() ??
				this.rules.functionStatement();

			if(node == null) {
				this.report(0, start, 'elseClause', 'No value.');
			}

			return node;
		},
		entry: () => {
			let node = {
				type: 'entry',
				range: {
					start: this.position
				},
				key: this.rules.expressionsSequence(),
				value: undefined
			}

			if(node.key == null || !this.token.type.startsWith('operator') || this.token.value !== ':') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.value = this.rules.expressionsSequence();

			if(node.value == null) {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		enumerationBody: () => {
			return this.rules.body('enumeration');
		},
		enumerationDeclaration: (anonymous) => {
			let node = {
				type: 'enumeration'+(!anonymous ? 'Declaration' : 'Expression'),
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				identifier: undefined,
				inheritedTypes: undefined,
				body: undefined
			}

			if(this.token.type !== 'keywordEnum') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.identifier = this.rules.identifier();

			if(anonymous) {
				if(node.modifiers.length > 0 || node.identifier != null) {
					this.position = node.range.start;

					return;
				}

				delete node.modifiers;
				delete node.identifier;
			} else
			if(node.identifier == null) {
				this.report(2, node.range.start, node.type, 'No identifier.');
			}

			node.inheritedTypes = this.rules.inheritedTypesClause();
			node.body = this.rules.enumerationBody();

			if(node.modifiers?.some(v => !['private', 'protected', 'public', 'static', 'final'].includes(v))) {
				this.report(1, node.range.start, node.type, 'Wrong modifier(s).');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		enumerationExpression: () => {
			return this.rules.enumerationDeclaration(true);
		},
		enumerationStatements: () => {
			return this.rules.statements([
				'caseDeclaration',
				'enumerationDeclaration'
			]);
		},
		expression: () => {
			return (
				this.rules.asyncExpression() ??
				this.rules.awaitExpression() ??
				this.rules.deleteExpression() ??
				this.rules.tryExpression() ??
				this.rules.prefixExpression()
			);
		},
		expressionsSequence: () => {
			let node = {
				type: 'expressionsSequence',
				range: {
					start: this.position
				},
				values: []
			}

			let subsequentialTypes = ['inOperator', 'isOperator']

			node.values = this.helpers.sequentialNodes(['expression', 'infixExpression'], undefined, subsequentialTypes);

			if(node.values.length === 0) {
				return;
			}
			if(node.values.filter(v => !subsequentialTypes.includes(v.type)).length%2 === 0) {
				this.position = node.values.at(-1).range.start;

				node.values.pop();
			}
			if(node.values.length === 1) {
				return node.values[0]
			}

			node.range.end = this.position-1;

			return node;
		},
		fallthroughStatement: () => {
			let node = {
				type: 'fallthroughStatement',
				range: {},
				label: undefined
			}

			if(this.token.type !== 'keywordFallthrough') {
				return;
			}

			node.range.start = this.position++;
			node.label = this.rules.identifier();
			node.range.end = this.position-1;

			return node;
		},
		floatLiteral: () => {
			let node = {
				type: 'floatLiteral',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'numberFloat') {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		forStatement: () => {
			let node = {
				type: 'forStatement',
				range: {},
				identifier: undefined,
				in: undefined,
				where: undefined,
				value: undefined
			}

			if(this.token.type !== 'keywordFor') {
				return;
			}

			node.range.start = this.position++;
			node.identifier = this.rules.identifier();

			if(this.token.type === 'keywordIn') {
				this.position++;
				node.in = this.rules.expressionsSequence();
			}
			if(this.token.type === 'keywordWhere') {
				this.position++;
				node.where = this.rules.expressionsSequence();
			}

			this.helpers.bodyTrailedValue(node, node.where != null ? 'where' : 'in', 'value');

			if(node.identifier == null) {
				this.report(1, node.range.start, node.type, 'No identifier.');
			}
			if(node.in == null) {
				this.report(2, node.range.start, node.type, 'No in.');
			}
			if(node.value == null) {
				this.report(0, node.range.start, node.type, 'No value.');
			}

			node.range.end = this.position-1;

			return node;
		},
		functionBody: () => {
			return this.rules.body('function');
		},
		functionDeclaration: (anonymous) => {
			let node = {
				type: 'function'+(!anonymous ? 'Declaration' : 'Expression'),
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				identifier: undefined,
				signature: undefined,
				body: undefined
			}

			if(this.token.type !== 'keywordFunc') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.identifier =
				this.rules.identifier() ??
				this.rules.operator();

			if(anonymous) {
				if(node.modifiers.length > 0 || node.identifier != null) {
					this.position = node.range.start;

					return;
				}

				delete node.modifiers;
				delete node.identifier;
			} else
			if(node.identifier == null) {
				this.report(2, node.range.start, node.type, 'No identifier.');
			}

			node.signature = this.rules.functionSignature();
			node.body = this.rules.functionBody();

			if(node.signature == null) {
				this.report(1, node.range.start, node.type, 'No signature.');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		functionExpression: () => {
			return this.rules.functionDeclaration(true);
		},
		functionSignature: () => {
			let node = {
				type: 'functionSignature',
				range: {
					start: this.position
				},
				genericParameters: this.rules.genericParametersClause(),
				parameters: [],
				inits: -1,
				deinits: -1,
				awaits: -1,
				throws: -1,
				returnType: undefined
			}

			if(this.token.type === 'parenthesisOpen') {
				this.position++;
				node.parameters = this.helpers.skippableNodes(
					['parameter'],
					() => this.token.type === 'parenthesisOpen',
					() => this.token.type === 'parenthesisClosed',
					() => this.token.type.startsWith('operator') && this.token.value === ','
				);

				if(this.token.type === 'parenthesisClosed') {
					this.position++;
				} else {
					node.range.end = this.position-1;

					this.report(1, node.range.start, node.type, 'Parameters don\'t have the closing parenthesis and were decided to be autoclosed at the end of stream.');

					return node;
				}
			}

			while(['keywordAwaits', 'keywordThrows'].includes(this.token.type)) {
				if(this.token.type === 'keywordAwaits') {
					this.position++;
					node.awaits = 1;
				}
				if(this.token.type === 'keywordThrows') {
					this.position++;
					node.throws = 1;
				}
			}

			if(this.token.type.startsWith('operator') && this.token.value === '->') {
				this.position++;
				node.returnType = this.rules.type();
			}

			node.range.end = this.position-1;

			if(node.range.end < node.range.start) {
				return;
			}

			return node;
		},
		functionStatement: () => {
			let types = this.rules.functionStatements(true);

			for(let type of types) {
				let node = this.rules[type]?.();

				if(node != null) {
					return node;
				}
			}
		},
		functionStatements: (list) => {
			let types = [
				'expressionsSequence',  // Expressions must be parsed first as they may include (anonymous) declarations
				'declaration',
				'controlTransferStatement',
				'doStatement',
				'forStatement',
				'ifStatement',
				'whileStatement'
			]

			return !list ? this.rules.statements(types) : types;
		},
		functionType: () => {
			let node = {
				type: 'functionType',
				range: {
					start: this.position,
					end: this.position
				},
				genericParameterTypes: [],
				parameterTypes: [],
				awaits: -1,
				throws: -1,
				returnType: undefined
			}

			if(this.token.type.startsWith('operator') && this.token.value === '<') {
				this.position++;
				node.genericParameterTypes = this.helpers.sequentialNodes(
					['type'],
					() => this.token.type.startsWith('operator') && this.token.value === ','
				);

				if(this.token.type.startsWith('operator') && this.token.value === '>') {
					node.range.end = this.position++;
				} else {
					this.position = node.range.start;

					return;
				}
			}

			if(this.token.type !== 'parenthesisOpen') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.parameterTypes = this.helpers.sequentialNodes(
				['type'],
				() => this.token.type.startsWith('operator') && this.token.value === ','
			);

			if(this.token.type !== 'parenthesisClosed') {
				this.position = node.range.start;

				return;
			}

			this.position++;

			while(['keywordAwaits', 'keywordThrows'].includes(this.token.type)) {
				if(this.token.type === 'keywordAwaits') {
					this.position++;
					node.awaits = 1;

					if(this.token.type === 'operatorPostfix' && this.token.value === '?') {
						this.position++;
						node.awaits = 0;
					}
				}
				if(this.token.type === 'keywordThrows') {
					this.position++;
					node.throws = 1;

					if(this.token.type === 'operatorPostfix' && this.token.value === '?') {
						this.position++;
						node.throws = 0;
					}
				}
			}

			if(!this.token.type.startsWith('operator') || this.token.value !== '->') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.returnType = this.rules.type();

			if(node.returnType == null) {
				this.report(1, node.range.start, node.type, 'No return type.');
			}

			node.range.end = this.position-1;

			return node;
		},
		genericParameter: () => {
			let node = {
				type: 'genericParameter',
				range: {
					start: this.position
				},
				identifier: this.rules.identifier(),
				type_: undefined
			}

			if(node.identifier == null) {
				return;
			}

			node.type_ = this.rules.typeClause();
			node.range.end = this.position-1;

			return node;
		},
		genericParametersClause: () => {
			let nodes = [],
				start = this.position;

			if(!this.token.type.startsWith('operator') || this.token.value !== '<') {
				return nodes;
			}

			this.position++;
			nodes = this.helpers.skippableNodes(
				['genericParameter'],
				() => this.token.type.startsWith('operator') && this.token.value === '<',
				() => this.token.type.startsWith('operator') && this.token.value === '>',
				() => this.token.type.startsWith('operator') && this.token.value === ','
			);

			if(!this.token.type.startsWith('operator') || this.token.value !== '>') {
				this.report(1, start, 'genericParametersClause', 'Node doesn\'t have the closing angle and was decided to be autoclosed at the end of stream.');

				return nodes;
			}

			this.position++;

			return nodes;
		},
		identifier: () => {
			let node = {
				type: 'identifier',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'identifier') {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		ifStatement: () => {
			let node = {
				type: 'ifStatement',
				range: {},
				condition: undefined,
				then: undefined,
				else: undefined
			}

			if(this.token.type !== 'keywordIf') {
				return;
			}

			node.range.start = this.position++;
			node.condition = this.rules.expressionsSequence();

			this.helpers.bodyTrailedValue(node, 'condition', 'then');

			node.else = this.rules.elseClause();

			if(node.condition == null) {
				this.report(2, node.range.start, node.type, 'No condition.');
			}
			if(node.then == null) {
				this.report(0, node.range.start, node.type, 'No value.');
			}

			node.range.end = this.position-1;

			return node;
		},
		implicitChainExpression: () => {
			let node = {
				type: 'implicitChainExpression',
				range: {},
				member: undefined
			}

			if(!this.token.type.startsWith('operator') || this.token.type.endsWith('Postfix') || this.token.value !== '.') {
				return;
			}

			node.range.start = this.position++;
			node.member =
				this.rules.identifier() ??
				this.rules.stringLiteral();

			if(node.member == null) {
				this.position--;

				return;
			}

			node.range.end = node.member.range.end;

			return node;
		},
		implicitChainIdentifier: () => {
			let node = {
				type: 'implicitChainIdentifier',
				range: {},
				value: undefined
			}

			if(!this.token.type.startsWith('operator') || this.token.type.endsWith('Postfix') || this.token.value !== '.') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.identifier();

			if(node.value == null) {
				this.position--;

				return;
			}

			node.range.end = node.value.range.end;

			return node;
		},
		importDeclaration: () => {
			let node = {
				type: 'importDeclaration',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'keywordImport') {
				return;
			}

			node.range.start =
			node.range.end = this.position++;
			node.value = this.rules.identifier();

			if(node.value != null) {
				while(!this.tokensEnd) {
					let node_ = this.rules.chainIdentifier(node.value);

					if(node_ == null) {
						break;
					}

					node.value = node_;
				}

				node.range.end = node.value.range.end;
			} else {
				this.report(1, node.range.start, node.type, 'No value.');
			}

			return node;
		},
		infixExpression: () => {
			return (
				this.rules.conditionalOperator() ??
				this.rules.inOperator() ??
				this.rules.isOperator() ??
				this.rules.infixOperator()
			);
		},
		infixOperator: () => {
			let node = {
				type: 'infixOperator',
				range: {},
				value: undefined
			}

			let exceptions = [',', ':']  // Enclosing nodes can use operators from the list as delimiters

			if(!['operator', 'operatorInfix'].includes(this.token.type) || exceptions.includes(this.token.value)) {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		inheritedTypesClause: () => {
			let nodes = [],
				start = this.position;

			if(!this.token.type.startsWith('operator') || this.token.value !== ':') {
				return nodes;
			}

			this.position++;
			nodes = this.helpers.sequentialNodes(
				['typeIdentifier'],
				() => this.token.type.startsWith('operator') && this.token.value === ','
			);

			if(nodes.length === 0) {
				this.report(0, start, 'inheritedTypesClause', 'No type identifiers.');
			}

			return nodes;
		},
		initializerClause: () => {
			let node,
				start = this.position;

			if(!this.token.type.startsWith('operator') || this.token.value !== '=') {
				return;
			}

			this.position++;
			node = this.rules.expressionsSequence();

			if(node == null) {
				this.report(0, start, 'initializerClause', 'No value.');
			}

			return node;
		},
		initializerDeclaration: () => {
			let node = {
				type: 'initializerDeclaration',
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				nillable: false,
				signature: undefined,
				body: undefined
			}

			if(this.token.type !== 'identifier' || this.token.value !== 'init') {
				this.position = node.range.start;

				return;
			}

			this.position++;

			if(this.token.type.startsWith('operator') && this.token.value === '?') {
				this.position++;
				node.nillable = true;
			}

			node.signature = this.rules.functionSignature();
			node.body = this.rules.functionBody();

			if(node.modifiers.some(v => !['private', 'protected', 'public'].includes(v))) {
				this.report(1, node.range.start, node.type, 'Wrong modifier(s).');
			}
			if(node.signature == null) {
				this.report(0, node.range.start, node.type, 'No signature.');
			} else
			if(node.signature.returnType != null) {
				this.report(1, node.range.start, node.type, 'Signature shouldn\'t have a return type.');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		inOperator: () => {
			let node = {
				type: 'inOperator',
				range: {
					start: this.position
				},
				inverted: false,
				composite: undefined
			}

			if(this.token.type === 'operatorPrefix' && this.token.value === '!') {
				this.position++;
				node.inverted = true;
			}

			if(this.token.type !== 'keywordIn') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.composite = this.rules.expressionsSequence();

			if(node.composite == null) {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		inoutExpression: () => {
			let node = {
				type: 'inoutExpression',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'operatorPrefix' || this.token.value !== '&') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.postfixExpression();

			if(node.value == null) {
				this.position--;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		inoutType: () => {
			let node = {
				type: 'inoutType',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'keywordInout') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.unionType();
			node.range.end = this.position-1;

			return node;
		},
		integerLiteral: () => {
			let node = {
				type: 'integerLiteral',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'numberInteger') {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		intersectionType: () => {
			let node = {
				type: 'intersectionType',
				range: {
					start: this.position
				},
				subtypes: this.helpers.sequentialNodes(
					['postfixType'],
					() => this.token.type.startsWith('operator') && this.token.value === '&'
				)
			}

			if(node.subtypes.length === 0) {
				return;
			}
			if(node.subtypes.length === 1) {
				return node.subtypes[0]
			}

			node.range.end = this.position-1;

			return node;
		},
		isOperator: () => {
			let node = {
				type: 'isOperator',
				range: {
					start: this.position
				},
				type_: undefined,
				inverted: false
			}

			if(this.token.type === 'operatorPrefix' && this.token.value === '!') {
				this.position++;
				node.inverted = true;
			}

			if(this.token.type !== 'keywordIs') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.type_ = this.rules.type();

			if(node.type_ == null) {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		literalExpression: () => {
			return (
				this.rules.arrayLiteral() ??
				this.rules.booleanLiteral() ??
				this.rules.dictionaryLiteral() ??
				this.rules.floatLiteral() ??
				this.rules.integerLiteral() ??
				this.rules.nilLiteral() ??
				this.rules.stringLiteral()
			);
		},
		modifiers: () => {
			let values = [],
				start = this.position,
				keywords = [
					'keywordInfix',
					'keywordPostfix',
					'keywordPrefix',

					'keywordPrivate',
					'keywordProtected',
					'keywordPublic',

					'keywordFinal',
					'keywordLazy',
					'keywordStatic',
					'keywordVirtual'
				]

			while(keywords.includes(this.token.type)) {
				values.push(this.token.value);
				this.position++;
			}

			let operator = ['infix', 'postfix', 'prefix'],
				access = ['private', 'protected', 'public']

			if(
				values.filter(v => operator.includes(v)).length > 1 ||
				values.filter(v => access.includes(v)).length > 1 ||
				values.includes('final') && values.includes('virtual')
			) {
				this.report(1, start, 'modifiers', 'Mutual exclusion.');
			}

			return values;
		},
		module: () => {
			let node = {
				type: 'module',
				range: {
					start: this.position
				},
				statements: this.rules.functionStatements()
			}

			node.range.end = node.statements.length > 0 ? this.position-1 : 0;

			return node;
		},
		namespaceBody: () => {
			return this.rules.body('namespace');
		},
		namespaceDeclaration: (anonymous) => {
			let node = {
				type: 'namespace'+(!anonymous ? 'Declaration' : 'Expression'),
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				identifier: undefined,
				body: undefined
			}

			if(this.token.type !== 'keywordNamespace') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.identifier = this.rules.identifier();

			if(anonymous) {
				if(node.modifiers.length > 0 || node.identifier != null) {
					this.position = node.range.start;

					return;
				}

				delete node.modifiers;
				delete node.identifier;
			} else
			if(node.identifier == null) {
				this.report(2, node.range.start, node.type, 'No identifier.');
			}

			node.body = this.rules.namespaceBody();

			if(node.modifiers?.some(v => !['private', 'protected', 'public', 'static', 'final'].includes(v))) {
				this.report(1, node.range.start, node.type, 'Wrong modifier(s).');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		namespaceExpression: () => {
			return this.rules.namespaceDeclaration(true);
		},
		namespaceStatements: () => {
			return this.rules.statements(['declaration']);
		},
		nillableExpression: (node_) => {
			let node = {
				type: 'nillableExpression',
				range: {
					start: node_.range.start
				},
				value: node_
			}

			if(!['operatorInfix', 'operatorPostfix'].includes(this.token.type) || this.token.value !== '?') {
				return;
			}

			node.range.end = this.position++;

			return node;
		},
		nillableType: (node_) => {
			let node = {
				type: 'nillableType',
				range: {
					start: node_.range.start
				},
				value: node_
			}

			if(this.token.type !== 'operatorPostfix' || this.token.value !== '?') {
				return;
			}

			node.range.end = this.position++;

			return node;
		},
		nilLiteral: () => {
			let node = {
				type: 'nilLiteral',
				range: {}
			}

			if(this.token.type !== 'keywordNil') {
				return;
			}

			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		observerDeclaration: () => {
			let node = {
				type: 'observerDeclaration',
				range: {
					start: this.position
				},
				identifier: this.rules.identifier(),
				body: undefined
			}

			if(![
				'willGet',
				'get',
				'didGet',
				'willSet',
				'set',
				'didSet',
				'willDelete',
				'delete',
				'didDelete'
			].includes(node.identifier?.value)) {
				this.position = node.range.start;

				return;
			}

			node.body = this.rules.functionBody();

			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		observersBody: (strict) => {
			let node = this.rules.body('observers');

			if(node != null && strict && !node.statements.some(v => v.type !== 'unsupported')) {
				this.position = node.range.start;

				return;
			}

			return node;
		},
		observersStatements: () => {
			return this.rules.statements(['observerDeclaration']);
		},
		operator: () => {
			let node = {
				type: 'operator',
				range: {},
				value: undefined
			}

			if(!this.token.type.startsWith('operator')) {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		operatorBody: () => {
			return this.rules.body('operator');
		},
		operatorDeclaration: () => {
			let node = {
				type: 'operatorDeclaration',
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				operator: undefined,
				body: undefined
			}

			if(this.token.type !== 'keywordOperator') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.operator = this.rules.operator();
			node.body = this.rules.operatorBody();

			if(!node.modifiers.some(v => ['infix', 'postfix', 'prefix'].includes(v))) {
				this.report(2, node.range.start, node.type, 'Should have specific modifier (infix, postfix, prefix).');
			}
			if(node.operator == null) {
				this.report(2, node.range.start, node.type, 'No operator.');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			} else {
				for(let entry of node.body.statements.filter(v => v.type === 'entry')) {
					if(entry.key.type !== 'identifier' || !['associativity', 'precedence'].includes(entry.key.value)) {
						this.report(1, entry.range.start, node.type, 'Should have only identifiers as keys (associativity, precedence).');
					} else {
						if(entry.key.value === 'associativity') {
							if(entry.value.type !== 'identifier') {
								this.report(2, entry.range.start, node.type, 'Associativity value should be identifier.');
							}
							if(!['left', 'right', 'none'].includes(entry.value.value)) {
								this.report(2, entry.range.start, node.type, 'Associativity accepts only one of following values: left, right, none.');
							}
						}
						if(entry.key.value === 'precedence' && entry.value.type !== 'integerLiteral') {
							this.report(2, entry.range.start, node.type, 'Precedence value should be integer.');
						}
					}
				}
			}

			node.range.end = this.position-1;

			return node;
		},
		operatorStatements: () => {
			return this.rules.statements(['entry']);
		},
		parameter: () => {
			let node = {
				type: 'parameter',
				range: {
					start: this.position
				},
				label: this.rules.identifier(),
				identifier: undefined,
				type_: undefined,
				value: undefined
			}

			if(node.label == null) {
				return;
			}

			node.identifier = this.rules.identifier();

			if(node.identifier == null) {
				node.identifier = node.label;
				node.label = undefined;
			}

			node.type_ = this.rules.typeClause();
			node.value = this.rules.initializerClause();
			node.range.end = this.position-1;

			return node;
		},
		parenthesizedExpression: () => {
			let node = {
				type: 'parenthesizedExpression',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'parenthesisOpen') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.helpers.skippableNode(
				'expressionsSequence',
				() => this.token.type === 'parenthesisOpen',
				() => this.token.type === 'parenthesisClosed'
			);

			if(this.token.type === 'parenthesisClosed') {
				node.range.end = this.position++;
			} else
			if(this.tokensEnd) {
				node.range.end = this.position-1;

				this.report(1, node.range.start, node.type, 'Node doesn\'t have the closing parenthesis and was decided to be autoclosed at the end of stream.');
			} else {
				this.position = node.range.start;

				return;
			}

			return node;
		},
		parenthesizedType: () => {
			let node = {
				type: 'parenthesizedType',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'parenthesisOpen') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.type();

			if(this.token.type !== 'parenthesisClosed') {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position++;

			return node;
		},
		postfixExpression: () => {
			let node = {
				type: 'postfixExpression',
				range: {
					start: this.position
				},
				value: this.rules.primaryExpression(),
				operator: undefined
			}

			if(node.value == null) {
				return;
			}

			while(!this.tokensEnd) {
				let node_ =
					this.rules.callExpression(node.value) ??
					this.rules.chainExpression(node.value) ??
					this.rules.defaultExpression(node.value) ??
					this.rules.nillableExpression(node.value) ??
					this.rules.subscriptExpression(node.value);

				if(node_ == null) {
					break;
				}

				node.value = node_;
			}

			node.operator = this.rules.postfixOperator();

			if(node.operator == null) {
				return node.value;
			}

			node.range.end = node.operator.range.end;

			return node;
		},
		postfixOperator: () => {
			let node = {
				type: 'postfixOperator',
				range: {},
				value: undefined
			}

			let exceptions = [',', ':']  // Enclosing nodes can use trailing operators from the list

			if(this.token.type !== 'operatorPostfix' || exceptions.includes(this.token.value)) {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		postfixType: () => {
			let node = this.rules.primaryType();

			if(node == null) {
				return;
			}

			while(!this.tokensEnd) {
				let node_ =
					this.rules.defaultType(node) ??
					this.rules.nillableType(node);

				if(node_ == null) {
					break;
				}

				node = node_;
			}

			return node;
		},
		predefinedType: () => {
			let node = {
				type: 'predefinedType',
				range: {},
				value: undefined
			}

			if(![
				'keywordUnderscore',
				'keywordVoid',

				'keywordAny',
				'keywordBool',
				'keywordDict',
				'keywordFloat',
				'keywordInt',
				'keywordString',
				'keywordType',

				'keywordCapitalAny',
				'keywordCapitalClass',
				'keywordCapitalEnumeration',
				'keywordCapitalFunction',
				'keywordCapitalNamespace',
				'keywordCapitalObject',
				'keywordCapitalProtocol',
				'keywordCapitalStructure'
			].includes(this.token.type)) {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		prefixExpression: () => {
			let node = {
				type: 'prefixExpression',
				range: {
					start: this.position
				},
				operator: this.rules.prefixOperator(),
				value: undefined
			}

			node.value = this.rules.postfixExpression();

			if(node.value == null) {
				this.position = node.range.start;

				return;
			}
			if(node.operator == null) {
				return node.value;
			}

			node.range.end = node.value.range.end;

			return node;
		},
		prefixOperator: () => {
			let node = {
				type: 'prefixOperator',
				range: {},
				value: undefined
			}

			let exceptions = ['&', '.']  // primaryExpressions can start with operators from the list

			if(this.token.type !== 'operatorPrefix' || exceptions.includes(this.token.value)) {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		primaryExpression: () => {
			return (
				this.rules.classExpression() ??
				this.rules.closureExpression() ??
				this.rules.enumerationExpression() ??
				this.rules.functionExpression() ??
				this.rules.identifier() ??
				this.rules.implicitChainExpression() ??
				this.rules.inoutExpression() ??
				this.rules.literalExpression() ??
				this.rules.namespaceExpression() ??
				this.rules.parenthesizedExpression() ??
				this.rules.protocolExpression() ??
				this.rules.structureExpression() ??
				this.rules.typeExpression()
			);
		},
		primaryType: () => {
			return (
				this.rules.arrayType() ??
				this.rules.dictionaryType() ??
				this.rules.functionType() ??
				this.rules.parenthesizedType() ??
				this.rules.predefinedType() ??
				this.rules.protocolType() ??
				this.rules.typeIdentifier()
			);
		},
		protocolBody: () => {
			return this.rules.body('protocol');
		},
		protocolDeclaration: (anonymous) => {
			let node = {
				type: 'protocol'+(!anonymous ? 'Declaration' : 'Expression'),
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				identifier: undefined,
				inheritedTypes: undefined,
				body: undefined
			}

			if(this.token.type !== 'keywordProtocol') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.identifier = this.rules.identifier();

			if(anonymous) {
				if(node.modifiers.length > 0 || node.identifier != null) {
					this.position = node.range.start;

					return;
				}

				delete node.modifiers;
				delete node.identifier;
			} else
			if(node.identifier == null) {
				this.report(2, node.range.start, node.type, 'No identifier.');
			}

			node.inheritedTypes = this.rules.inheritedTypesClause();
			node.body = this.rules.protocolBody();

			if(node.modifiers?.some(v => !['private', 'protected', 'public', 'static', 'final'].includes(v))) {
				this.report(1, node.range.start, node.type, 'Wrong modifier(s).');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		protocolExpression: () => {
			return this.rules.protocolDeclaration(true);
		},
		protocolStatements: () => {
			return this.rules.statements([
				'classDeclaration',
				'enumerationDeclaration',
				'functionDeclaration',
				'namespaceDeclaration',
				'protocolDeclaration',
				'structureDeclaration',
				'variableDeclaration'
			]);
		},
		protocolType: () => {
			let node = {
				type: 'protocolType',
				range: {
					start: this.position
				},
				body: this.rules.protocolBody()
			}

			if(node.body == null) {
				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		returnStatement: () => {
			let node = {
				type: 'returnStatement',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'keywordReturn') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.expressionsSequence();
			node.range.end = this.position-1;

			return node;
		},
		statements: (types) => {
			return this.helpers.skippableNodes(
				types,
				() => this.token.type === 'braceOpen',
				() => this.token.type === 'braceClosed',
				() => this.token.type === 'delimiter',
				true
			);
		},
		stringExpression: () => {
			let node = {
				type: 'stringExpression',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'stringExpressionOpen') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.helpers.skippableNode(
				'expressionsSequence',
				() => this.token.type === 'stringExpressionOpen',
				() => this.token.type === 'stringExpressionClosed'
			);

			if(this.token.type === 'stringExpressionClosed') {
				node.range.end = this.position++;
			} else
			if(this.tokensEnd) {
				node.range.end = this.position-1;

				this.report(1, node.range.start, node.type, 'Node doesn\'t have the closing parenthesis and was decided to be autoclosed at the end of stream.');
			} else {
				this.position = node.range.start;

				return;
			}

			return node;
		},
		stringLiteral: () => {
			let node = {
				type: 'stringLiteral',
				range: {},
				segments: []
			}

			if(this.token.type !== 'stringOpen') {
				return;
			}

			node.range.start = this.position++;
			node.segments = this.helpers.skippableNodes(
				['stringSegment', 'stringExpression'],
				() => this.token.type === 'stringOpen',
				() => this.token.type === 'stringClosed'
			);

			if(this.token.type === 'stringClosed') {
				node.range.end = this.position++;
			} else {
				node.range.end = this.position-1;

				this.report(1, node.range.start, node.type, 'Node doesn\'t have the closing apostrophe and decided to be autoclosed at the end of stream.');
			}

			return node;
		},
		stringSegment: () => {
			let node = {
				type: 'stringSegment',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'stringSegment') {
				return;
			}

			node.value = this.token.value;
			node.range.start =
			node.range.end = this.position++;

			return node;
		},
		structureBody: () => {
			return this.rules.body('structure');
		},
		structureDeclaration: (anonymous) => {
			let node = {
				type: 'structure'+(!anonymous ? 'Declaration' : 'Expression'),
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				identifier: undefined,
				genericParameters: undefined,
				inheritedTypes: undefined,
				body: undefined
			}

			if(this.token.type !== 'keywordStruct') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.identifier = this.rules.identifier();

			if(anonymous) {
				if(node.modifiers.length > 0 || node.identifier != null) {
					this.position = node.range.start;

					return;
				}

				delete node.modifiers;
				delete node.identifier;
			} else
			if(node.identifier == null) {
				this.report(2, node.range.start, node.type, 'No identifier.');
			}

			node.genericParameters = this.rules.genericParametersClause();
			node.inheritedTypes = this.rules.inheritedTypesClause();
			node.body = this.rules.structureBody();

			if(node.modifiers?.some(v => !['private', 'protected', 'public', 'static', 'final'].includes(v))) {
				this.report(1, node.range.start, node.type, 'Wrong modifier(s).');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		structureExpression: () => {
			return this.rules.structureDeclaration(true);
		},
		structureStatements: () => {
			return this.rules.statements([
				'chainDeclaration',
				'classDeclaration',
				'deinitializerDeclaration',
				'enumerationDeclaration',
				'functionDeclaration',
				'initializerDeclaration',
				'namespaceDeclaration',
				'protocolDeclaration',
				'structureDeclaration',
				'subscriptDeclaration',
				'variableDeclaration'
			]);
		},
		subscriptDeclaration: () => {
			let node = {
				type: 'subscriptDeclaration',
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				signature: undefined,
				body: undefined
			}

			if(this.token.type !== 'identifier' || this.token.value !== 'subscript') {
				this.position = node.range.start;

				return;
			}

			node.range.start = this.position++;
			node.signature = this.rules.functionSignature();
			node.body = this.rules.observersBody(true) ?? this.rules.functionBody();

			if(node.modifiers.some(v => !['private', 'protected', 'public', 'static'].includes(v))) {
				this.report(1, node.range.start, node.type, 'Wrong modifier(s).');
			}
			if(node.signature == null) {
				this.report(2, node.range.start, node.type, 'No signature.');
			}
			if(node.body == null) {
				this.report(0, node.range.start, node.type, 'No body.');
			}

			node.range.end = this.position-1;

			return node;
		},
		subscriptExpression: (node_) => {
			let node = {
				type: 'subscriptExpression',
				range: {
					start: node_.range.start
				},
				composite: node_,
				arguments: undefined,
				closure: undefined
			}

			if(this.token.type.startsWith('operator') && this.token.value === '<') {
				this.position++;
				node.genericArguments = this.helpers.sequentialNodes(
					['type'],
					() => this.token.type.startsWith('operator') && this.token.value === ','
				);

				if(this.token.type.startsWith('operator') && this.token.value === '>') {
					this.position++;
				} else {
					this.position = node_.range.end+1;
				}
			}

			node.range.end = this.position-1;

			if(this.token.type === 'bracketOpen') {
				this.position++;
				node.arguments = this.helpers.skippableNodes(
					['argument'],
					() => this.token.type === 'bracketOpen',
					() => this.token.type === 'bracketClosed',
					() => this.token.type.startsWith('operator') && this.token.value === ','
				);

				if(this.token.type === 'bracketClosed') {
					this.position++;
				} else {
					node.range.end = this.position-1;

					this.report(1, node.range.start, node.type, 'Node doesn\'t have the closing bracket and was decided to be autoclosed at the end of stream.');

					return node;
				}
			}

			node.closure = this.rules.closureExpression();

			if(node.closure == null && node.range.end === this.position-1) {
				this.position = node_.range.end+1;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		throwStatement: () => {
			let node = {
				type: 'throwStatement',
				range: {},
				value: undefined
			}

			if(this.token.type !== 'keywordThrow') {
				return;
			}

			node.range.start = this.position++;
			node.value = this.rules.expressionsSequence();
			node.range.end = this.position-1;

			return node;
		},
		tryExpression: () => {
			let node = {
				type: 'tryExpression',
				range: {},
				nillable: false,
				value: undefined
			}

			if(this.token.type !== 'keywordTry') {
				return;
			}

			node.range.start = this.position++;

			if(this.token.type === 'operatorPostfix' && this.token.value === '?') {
				this.position++;
				node.nillable = true;
			}

			node.value = this.rules.expression();

			if(node.value == null) {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		type: () => {
			return (
				this.rules.variadicType() ??
				this.rules.inoutType() ??
				this.rules.unionType()
			);
		},
		typeClause: () => {
			let node,
				start = this.position;

			if(!this.token.type.startsWith('operator') || this.token.value !== ':') {
				return;
			}

			this.position++;
			node = this.rules.type();

			if(node == null) {
				this.report(0, start, 'typeClause', 'No value.');
			}

			return node;
		},
		typeExpression: () => {
			let node = {
				type: 'typeExpression',
				range: {
					start: this.position
				},
				type_: undefined
			}

			if(this.token.type !== 'keywordType') {
				return;
			}

			this.position++;
			node.type_ = this.rules.type();

			if(node.type_ == null) {
				this.position = node.range.start;

				return;
			}

			node.range.end = this.position-1;

			return node;
		},
		typeIdentifier: () => {
			let node = {
				type: 'typeIdentifier',
				range: {
					start: this.position
				},
				identifier:
					this.rules.identifier() ??
					this.rules.implicitChainIdentifier(),
				genericArguments: []
			}

			if(node.identifier == null) {
				return;
			}

			while(!this.tokensEnd) {
				let node_ = this.rules.chainIdentifier(node.identifier);

				if(node_ == null) {
					break;
				}

				node.identifier = node_;
			}

			node.range.end = this.position-1;

			if(!this.token.type.startsWith('operator') || this.token.value !== '<') {
				return node;
			}

			this.position++;
			node.genericArguments = this.helpers.sequentialNodes(
				['type'],
				() => this.token.type.startsWith('operator') && this.token.value === ','
			);

			if(!this.token.type.startsWith('operator') || this.token.value !== '>') {
				this.position = node.range.end+1;
				node.genericArguments = []
			} else {
				node.range.end = this.position++;
			}

			return node;
		},
		unionType: () => {
			let node = {
				type: 'unionType',
				range: {
					start: this.position
				},
				subtypes: this.helpers.sequentialNodes(
					['intersectionType'],
					() => this.token.type.startsWith('operator') && this.token.value === '|'
				)
			}

			if(node.subtypes.length === 0) {
				return;
			}
			if(node.subtypes.length === 1) {
				return node.subtypes[0]
			}

			node.range.end = this.position-1;

			return node;
		},
		variableDeclaration: () => {
			let node = {
				type: 'variableDeclaration',
				range: {
					start: this.position
				},
				modifiers: this.rules.modifiers(),
				declarators: []
			}

			if(this.token.type !== 'keywordVar') {
				this.position = node.range.start;

				return;
			}

			this.position++;
			node.declarators = this.helpers.sequentialNodes(
				['declarator'],
				() => this.token.type.startsWith('operator') && this.token.value === ','
			);

			if(node.modifiers.some(v => !['private', 'protected', 'public', 'final', 'lazy', 'static', 'virtual'].includes(v))) {
				this.report(1, node.range.start, node.type, 'Wrong modifier(s).');
			}
			if(node.declarators.length === 0) {
				this.report(0, node.range.start, node.type, 'No declarator(s).');
			}

			node.range.end = this.position-1;

			return node;
		},
		variadicType: () => {
			let node = {
				type: 'variadicType',
				range: {
					start: this.position
				},
				value: this.rules.inoutType() ?? this.rules.unionType()
			}

			if(!this.token.type.startsWith('operator') || this.token.value !== '...') {
				return node.value;
			}

			this.position++;
			node.range.end = this.position-1;

			return node;
		},
		whileStatement: () => {
			let node = {
				type: 'whileStatement',
				range: {},
				condition: undefined,
				value: undefined
			}

			if(this.token.type !== 'keywordWhile') {
				return;
			}

			node.range.start = this.position++;
			node.condition = this.rules.expressionsSequence();

			this.helpers.bodyTrailedValue(node, 'condition', 'value');

			if(node.condition == null) {
				this.report(2, node.range.start, node.type, 'No condition.');
			}
			if(node.value == null) {
				this.report(0, node.range.start, node.type, 'No value.');
			}

			node.range.end = this.position-1;

			return node;
		}
	}

	static helpers = {
		/*
		 * Trying to set the node's value and body basing on its value.
		 *
		 * Value that have unsignatured trailing closure will be divided to:
		 * - Value - a left-hand-side (exported, if closure goes right after it).
		 * - Body - the closure.
		 *
		 * Closures, nested into expressionsSequence, prefixExpression and inOperator, are also supported.
		 *
		 * Useful for unwrapping trailing bodies and completing preconditional statements, such as if or for.
		 */
		bodyTrailedValue: (node, valueKey, bodyKey, statementTrailed = true, body) => {
			body ??= this.rules.functionBody;
			node[bodyKey] = body();

			if(node[valueKey] == null || node[bodyKey] != null) {
				return;
			}

			let end;

			let parse = (n) => {
				if(n === node) {
					parse(n[valueKey]);

					if(end != null) {
						n[bodyKey] = body();
					}
				} else
				if(n.type === 'expressionsSequence') {
					parse(n.values.at(-1));
				} else
				if(n.type === 'prefixExpression') {
					parse(n.value);
				} else
				if(n.type === 'inOperator') {
					parse(n.composite);
				} else
				if(n.closure != null && n.closure.signature == null) {
					this.position = n.closure.range.start;
					n.closure = undefined;
					end = this.position-1;

					let lhs = n.callee ?? n.composite,
						exportable = lhs.range.end === end;

					if(exportable) {
						for(let k in n) {
							delete n[k]
						}

						Object.assign(n, lhs);
					}
				}

				if(end != null) {
					n.range.end = end;
				}
			}

			parse(node);

			if(statementTrailed) {
				node[bodyKey] ??= this.rules.functionStatement();
			}
		},
		/*
		 * Returns a list of nodes of the types in sequential order like [1, 2, 3, 1...].
		 *
		 * Additionally a separator between the nodes can be set.
		 * Also types can be marked subsequential and therefore have no impact on offset in iterations.
		 *
		 * Stops when an unsupported type occurs.
		 *
		 * Useful for precise sequences lookup.
		 */
		sequentialNodes: (types, separating, subsequentialTypes) => {
			let nodes = [],
				offset = 0;

			while(!this.tokensEnd) {
				let type = types[offset%types.length],
					node = this.rules[type]?.();

				if(node == null) {
					break;
				}

				nodes.push(node);

				if(!subsequentialTypes?.includes(node.type)) {
					offset++;
				}

				if(separating != null && offset > 0) {
					if(separating()) {
						this.position++;
					} else {
						break;
					}
				}
			}

			return nodes;
		},
		/*
		 * Returns a node of the type or the "unsupported" node if not closed immediately.
		 *
		 * Stops if node found, at 0 (relatively to 1 at start) scope level or at the .tokensEnd.
		 *
		 * Warns about "unsupported" nodes.
		 *
		 * Useful for imprecise single enclosed(ing) nodes lookup.
		 */
		skippableNode: (type, opening, closing) => {
			let node = this.rules[type]?.(),
				scopeLevel = 1;

			if(node != null || closing() || this.tokensEnd) {
				return node;
			}

			node = {
				type: 'unsupported',
				range: {
					start: this.position,
					end: this.position
				},
				tokens: []
			}

			while(!this.tokensEnd) {
				scopeLevel += opening();
				scopeLevel -= closing();

				if(scopeLevel === 0) {
					break;
				}

				node.tokens.push(this.token);
				node.range.end = this.position++;
			}

			let range = node.range.start !== node.range.end,
				message = range ? 'range of tokens ['+node.range.start+':'+node.range.end+']' : 'token ['+node.range.start+']';

			this.report(1, node.range.start, node.type, 'At '+message+'.');

			return node;
		},
		/*
		 * Returns a list of nodes of the types, including the "unsupported" nodes if any occur.
		 *
		 * Additionally a (optional) separator between the nodes can be set.
		 *
		 * Stops at 0 (relatively to 1 at start) scope level or at the .tokensEnd.
		 *
		 * Warns about invalid separators and "unsupported" nodes.
		 *
		 * Useful for imprecise multiple enclosed(ing) nodes lookup.
		 */
		skippableNodes: (types, opening, closing, separating, optionalSeparator) => {
			let nodes = [],
				scopeLevel = 1;

			while(!this.tokensEnd) {
				let node;

				if(separating == null || nodes.length === 0 || nodes.at(-1).type === 'separator' || optionalSeparator) {
					for(let type of types) {
						node = this.rules[type]?.();

						if(node != null) {
							nodes.push(node);

							break;
						}
					}
				}

				if(closing() && scopeLevel === 1 || this.tokensEnd) {
					break;
				}

				if(separating?.()) {
					node = nodes.at(-1);

					if(node?.type !== 'separator') {
						node = {
							type: 'separator',
							range: {
								start: this.position,
								end: this.position
							}
						}

						nodes.push(node);
					} else {
						node.range.end = this.position;
					}

					this.position++;
				}

				if(node != null) {
					continue;
				}

				node = nodes.at(-1);

				if(node?.type !== 'unsupported') {
					node = {
						type: 'unsupported',
						range: {
							start: this.position,
							end: this.position
						},
						tokens: []
					}
				}

				scopeLevel += opening();
				scopeLevel -= closing();

				if(scopeLevel === 0) {
					break;
				}

				node.tokens.push(this.token);
				node.range.end = this.position++;

				if(node !== nodes.at(-1)) {
					nodes.push(node);
				}
			}

			for(let key in nodes) {
				let node = nodes[key],
					range = node.range.start !== node.range.end;

				if(node.type === 'separator') {
					if(key == 0) {
						this.report(0, node.range.start, node.type, 'Met before any supported type at token ['+node.range.start+'].');
					}
					if(range) {
						this.report(0, node.range.start, node.type, 'Sequence at range of tokens ['+node.range.start+':'+node.range.end+'].');
					}
					if(key == nodes.length-1) {
						this.report(0, node.range.start, node.type, 'Excess at token ['+node.range.start+'].');
					}
				}
				if(node.type === 'unsupported') {
					let message = range ? 'range of tokens ['+node.range.start+':'+node.range.end+']' : 'token ['+node.range.start+']';

					this.report(1, node.range.start, node.type, 'At '+message+'.');
				}
			}

			nodes = nodes.filter(v => v.type !== 'separator');

			return nodes;
		}
	}

	static get position() {
		return this.#position;
	}

	static set position(value) {
		if(value < this.#position) {  // Rollback global changes
			this.reports = this.reports.filter(v => v.position < value);
		}

		this.#position = value;
	}

	static get token() {
		return this.tokens[this.position] ?? {
			position: undefined,
			location: undefined,
			type: '',
			value: ''
		}
	}

	static get tokensEnd() {
		return this.position === this.tokens.length;
	}

	static report(level, position, type, string) {
		let location = this.tokens[position].location;

		string = type+' -> '+string;

		if(this.reports.find(v =>
			v.location.line === location.line &&
			v.location.column === location.column &&
			v.string === string) != null
		) {
			return;
		}

		this.reports.push({
			level: level,
			position: position,
			location: location,
			string: string
		});
	}

	static reset() {
		this.tokens = []
		this.#position = 0;
		this.reports = []
	}

	static parse(lexerResult) {
		this.reset();

		this.tokens = lexerResult.tokens;

		let result = {
			tree: this.rules.module(),
			reports: this.reports
		}

		this.reset();

		return result;
	}
}