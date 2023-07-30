class Interpreter {
	static tokens;
	static tree;
	static position;
	static composites;
	static calls;
	static scopes;
	static controlTransfers;
	static preferences;
	static reports;

	static rules = {
		argument: (n) => {
			let value = this.executeNode(n.value);

			if(!this.threw) {
				return {
					label: n.label?.value,
					value: value
				}
			}
		},
		arrayLiteral: (n) => {
			let value = this.createValue('dictionary', new Map());

			for(let i = 0; i < n.values.length; i++) {
				let value_ = this.executeNode(n.values[i]);

				if(value_ != null) {
					value.primitiveValue.set(i, value_);
				}
			}

			return this.helpers.wrapValue('Array', value);
		},
		arrayType: (n, t, tp) => {
			this.rules.collectionType(n, t, tp, 'array');
		},
		booleanLiteral: (n) => {
			let value = this.createValue('boolean', n.value === 'true');

			return this.helpers.wrapValue('Boolean', value);
		},
		breakStatement: (n) => {
			let value = n.label != null ? this.createValue('string', n.label.value) : undefined;

			this.setControlTransfer(value, 'break');

			return value;
		},
		callExpression: (n) => {
			let gargs = [],  // (Generic) arguments
				args = []

			for(let garg of n.genericArguments) {
				let type = [],
					typePart = this.helpers.createTypePart(type, undefined, garg);

				gargs.push(type);
			}
			for(let arg of n.arguments) {
				arg = this.executeNode(arg);

				if(this.threw) {
					return;
				}

				if(arg != null) {
					args.push(arg);
				}
			}

			let callee = n.callee,
				value,
				MOSP = this.helpers.getMemberOverloadSearchParameters(callee);

			if(MOSP == null) {
				value = this.executeNode(callee);

				if(this.threw) {
					return;
				}
			}

			if((MOSP?.composite == null || MOSP?.identifier == null) && value == null) {
				this.report(1, n, 'Cannot search for function using anything but a valid (in particular, chain) expression.');

				return;
			}

			let function_ = this.findFunction(MOSP?.composite, MOSP?.identifier, MOSP?.internal, value, args);

			if(function_ == null) {
				this.report(2, n, 'Function or initializer with specified signature wasn\'t found.');

				return;
			}

			return this.callFunction(function_, gargs, args);
		},
		chainExpression: (n) => {
			let composite = this.getValueComposite(this.executeNode(n.composite));

			if(composite == null) {
				this.report(2, n, 'Composite wasn\'t found.');

				return;
			}

			let identifier = n.member;

			if(identifier.type === 'stringLiteral') {
				identifier = this.rules.stringLiteral(n.member);
			} else {
				identifier = identifier.value;
			}

			return this.findMemberOverload(composite, identifier, undefined, true)?.value;
		},
		classDeclaration: (n) => {
			this.rules.compositeDeclaration(n);
		},
		classExpression: (n) => {
			return this.rules.compositeDeclaration(n, true);
		},
		collectionType: (n, type, typePart, title) => {
			let capitalizedTitle = title[0].toUpperCase()+title.slice(1),
				composite = this.getValueComposite(this.findMemberOverload(this.scope, capitalizedTitle)?.value);

			if(composite != null) {
				typePart.reference = this.getOwnID(composite);
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { [composite != null ? 'genericArguments' : title]: true });

			if(title === 'dictionary') {
				this.helpers.createTypePart(type, typePart, n.key);
			}

			this.helpers.createTypePart(type, typePart, n.value);
		},
		combiningType: (n, type, typePart, title) => {
			if(n.subtypes.length === 0) {
				return;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { [title]: true });

			for(let subtype of n.subtypes) {
				this.helpers.createTypePart(type, typePart, subtype);
			}
		},
		compositeDeclaration: (n, anonymous) => {
			let modifiers = n.modifiers,
				identifier = n.identifier?.value;

			if(!anonymous && identifier == null) {
				return;
			}

			let title = n.type.replace('Declaration', '')
							  .replace('Expression', ''),
				capitalizedTitle = title[0].toUpperCase()+title.slice(1),
				genericParameters = n.genericParameters,
				inheritedTypes = n.inheritedTypes,
				composite = this['create'+capitalizedTitle](identifier, this.scope);

			if(genericParameters?.length > 0) {
				let typePart = this.helpers.createOrSetCollectionTypePart(composite.type, composite.type[0], { genericParameters: true });

				for(let genericParameter of genericParameters) {
					this.rules.genericParameter(genericParameter, composite.type, typePart);
				}
			}
			if(inheritedTypes?.length > 0) {
				let typePart = this.helpers.createOrSetCollectionTypePart(composite.type, composite.type[0], { inheritedTypes: true });

				for(let inheritedType of inheritedTypes) {
					this.helpers.createTypePart(composite.type, typePart, inheritedType, false);

					// TODO: Protocol conformance checking (if not conforms, remove from type and report)
				}
			}
			for(let typePart of composite.type) {
				this.retainComposite(composite, this.getComposite(typePart.reference));
			}

			if(title === 'class') {
				let superComposite = this.getComposite(this.getTypeInheritedID(composite.type));

				if(superComposite != null) {
					this.setSuperID(composite, superComposite);
				}
			}

			let type = [this.createTypePart(undefined, undefined, { predefined: capitalizedTitle, self: true })],
				value = this.createValue('reference', this.getOwnID(composite)),
				observers = [],
				statements = n.body?.statements ?? [],
				objectStatements = []

			if(['class', 'structure'].includes(title)) {
				this.helpers.separateStatements(statements, objectStatements);
				this.setStatements(composite, objectStatements);
			}

			if(!anonymous) {
				this.setMemberOverload(this.scope, identifier, modifiers, type, value, observers);
			}

			this.addScope(composite);
			this.executeNodes(statements);
			this.removeScope(false);

			if(anonymous) {
				return value;
			}
		},
		continueStatement: (n) => {
			let value = n.label != null ? this.createValue('string', n.label.value) : undefined;

			this.setControlTransfer(value, 'continue');

			return value;
		},
		defaultType: (n, t, tp) => {
			this.rules.optionalType(n, t, tp, ['default', 'nillable']);
		},
		deinitializerDeclaration: (n) => {
			let identifier = 'deinit',
				function_ = this.createFunction(identifier, n.body?.statements, this.scope),
				signature = this.rules.functionSignature({
					type: 'functionSignature',
					genericParameters: [],
					parameters: [],
					deinits: 1,
					awaits: -1,  // 1?
					throws: -1,  // 1?
					returnType: undefined
				}),
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value = this.createValue('reference', this.getOwnID(function_)),
				observers = []

			function_.type = signature;

			this.setMemberOverload(this.scope, identifier, [], type, value, observers, () => {});
		},
		dictionaryLiteral: (n) => {
			let value = this.createValue('dictionary', new Map());

			for(let entry of n.entries) {
				entry = this.rules.entry(entry);

				if(entry != null) {
					value.primitiveValue.set(entry.key, entry.value);
				}
			}

			return this.helpers.wrapValue('Dictionary', value);
		},
		dictionaryType: (n, t, tp) => {
			this.rules.collectionType(n, t, tp, 'dictionary');
		},
		entry: (n) => {
			return {
				key: this.executeNode(n.key),
				value: this.executeNode(n.value)
			}
		},
		enumerationDeclaration: (n) => {
			this.rules.compositeDeclaration(n);
		},
		enumerationExpression: (n) => {
			return this.rules.compositeDeclaration(n, true);
		},
		expressionsSequence: (n) => {
			if(n.values.length === 3 && n.values[1].type === 'infixOperator' && n.values[1].value === '=') {
				let lhs = n.values[0],
					lhsMOSP = this.helpers.getMemberOverloadSearchParameters(lhs);

				if(lhsMOSP != null && lhsMOSP.composite != null && lhsMOSP.identifier != null) {
					lhs = this.findMemberOverload(lhsMOSP.composite, lhsMOSP.identifier, undefined, lhsMOSP.internal);
				} else {
					lhs = undefined;
				}

				// TODO: Create member with default type if not exists

				if(lhs == null) {
					this.report(1, n, 'Cannot assign to anything but a valid identifier or chain expression.');

					return;
				}

				let rhs = this.executeNode(n.values[2]);

				this.setMemberOverload(lhsMOSP.composite, lhsMOSP.identifier, lhs.modifiers, lhs.type, rhs, lhs.observers, undefined, lhsMOSP.internal);

				return rhs;
			}
			if(n.values.length === 3 && n.values[1].type === 'infixOperator' && n.values[1].value === '==') {
				let lhs = this.executeNode(n.values[0]),
					rhs = this.executeNode(n.values[2]),
					value = lhs?.primitiveType === rhs?.primitiveType && lhs?.primitiveValue === rhs?.primitiveValue;

				if(!value) {
				//	debugger;
				}

				return this.createValue('boolean', value);
			}
			if(n.values.length === 3 && n.values[1].type === 'infixOperator' && n.values[1].value === '<') {
				let lhs = this.executeNode(n.values[0]),
					rhs = this.executeNode(n.values[2]),
					value = lhs?.primitiveValue < rhs?.primitiveValue;

				return this.createValue('boolean', value);
			}
		},
		floatLiteral: (n) => {
			let value = this.createValue('float', n.value*1);

			return this.helpers.wrapValue('Float', value);
		},
		functionDeclaration: (n) => {
			let modifiers = n.modifiers,
				identifier = n.identifier?.value;

			if(identifier == null) {
				return;
			}

			let function_,
				statements = Array.from(n.body?.statements ?? []),
				objectStatements = [],
				signature,
				functionScope,
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value,
				observers = [],
				object = this.compositeIsObject(this.scope),
				staticDeclaration = modifiers.includes('static') || !object && !this.compositeIsInstantiable(this.scope);

			this.helpers.separateStatements(statements, objectStatements);

			if(staticDeclaration) {
				if(!object) {  // Static in non-object
					signature = n.signature;
					functionScope = this.scope;
				} else {  // Static in object
					return;
				}
			} else {
				if(object) {  // Non-static in object
					statements = []
					signature = n.signature;
					functionScope = this.findMemberOverload(this.scope, identifier, (v) => this.findValueFunction(v.value, []), true)?.matchingValue ?? this.scope;
				} else {  // Non-static in non-object
					if(statements.length === 0) {
						return;
					}

					objectStatements = []
					functionScope = this.scope;
				}
			}

			function_ = this.createFunction(identifier, objectStatements, functionScope);
			function_.type = this.rules.functionSignature(signature);
			value = this.createValue('reference', this.getOwnID(function_));

			this.setMemberOverload(this.scope, identifier, modifiers, type, value, observers, () => {});
			this.addScope(function_);
			this.executeNodes(statements);
			this.removeScope(false);
		},
		functionExpression: (n) => {
			let signature = this.rules.functionSignature(n.signature),
				function_ = this.createFunction(undefined, n.body?.statements, this.scope);

			function_.type = signature;

			return this.createValue('reference', this.getOwnID(function_));
		},
		functionSignature: (n) => {
			let type = [],
				typePart = this.createTypePart(type, undefined, { predefined: 'Function' });

			typePart.inits = n?.inits ?? -1;
			typePart.deinits = n?.deinits ?? -1;
			typePart.awaits = n?.awaits ?? -1;
			typePart.throws = n?.throws ?? -1;

			for(let v of ['genericParameters', 'parameters']) {
				if(n?.[v].length > 0) {
					let typePart_ = this.createTypePart(type, typePart, { [v]: true });

					for(let n_ of n[v]) {
						this.executeNode(n_, type, typePart_);
					}
				}
			}

			this.helpers.createTypePart(type, typePart, n?.returnType);

			return type;
		},
		functionType: (n, type, typePart) => {
			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { predefined: 'Function' });

			typePart.awaits = n.awaits ?? 0;
			typePart.throws = n.throws ?? 0;

			for(let v of ['genericParameter', 'parameter']) {
				if(n[v+'Types'].length > 0) {
					let typePart_ = this.createTypePart(type, typePart, { [v+'s']: true });

					for(let node_ of n[v+'Types']) {
						this.helpers.createTypePart(type, typePart_, node_);
					}
				}
			}

			this.helpers.createTypePart(type, typePart, n.returnType);
		},
		genericParameter: (n, type, typePart) => {
			typePart = this.helpers.createTypePart(type, typePart, n.type_);
			typePart.identifier = n.identifier.value;
		},
		identifier: (n) => {
			return this.findMemberOverload(this.scope, n.value)?.value;
		},
		ifStatement: (n) => {
			if(n.condition == null) {
				return;
			}

			let namespace = this.createNamespace('Local<'+(this.scope.title ?? '#'+this.getOwnID(this.scope))+', If>', this.scope, null),
				condition,
				elseif = n.else?.type === 'ifStatement';

			this.addScope(namespace);

			condition = this.executeNode(n.condition);
			condition = condition?.primitiveType === 'boolean' ? condition.primitiveValue : condition != null;

			if(condition || !elseif) {
				let branch = n[condition ? 'then' : 'else']

				if(branch?.type === 'functionBody') {
					this.executeNodes(branch.statements);
				} else {
					this.setControlTransfer(this.executeNode(branch));
				}
			}

			this.removeScope();

			if(!condition && elseif) {
				this.rules.ifStatement(n.else);
			}

			return this.controlTransfer?.value;
		},
		initializerDeclaration: (n) => {
			let modifiers = n.modifiers,
				identifier = 'init',
				function_ = this.createFunction(identifier, n.body?.statements, this.scope),
				signature = n.signature,
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value = this.createValue('reference', this.getOwnID(function_)),
				observers = []

			signature ??= {
				type: 'functionSignature',
				genericParameters: [],
				parameters: [],
				inits: undefined,
				awaits: -1,
				throws: -1,
				returnType: undefined
			}
			signature.inits = 1;
			signature.returnType = {
				type: 'typeIdentifier',
				identifier: {
					type: 'identifier',
					value: 'Self'
				},
				genericArguments: []
			}

			if(n.nillable) {
				signature.returnType = {
					type: 'nillableType',
					value: signature.returnType
				}
			}

			signature = this.rules.functionSignature(signature);
			function_.type = signature;

			if(function_.statements.at(-1)?.type !== 'returnStatement') {
				function_.statements.push({
					type: 'returnStatement',
					value: {
						type: 'identifier',
						value: 'self'
					}
				});
			}

			this.setMemberOverload(this.scope, identifier, modifiers, type, value, observers, () => {});
		},
		inoutExpression: (n) => {
			let value = this.executeNode(n.value);

			if(value?.primitiveType === 'pointer') {
				return value;
			}
			if(value?.primitiveType !== 'reference') {
				this.report(2, n, 'Non-reference value ("'+(value?.primitiveType ?? 'nil')+'") can\'t be used as a pointer.');

				return;
			}

			return this.createValue('pointer', value.primitiveValue);
		},
		inoutType: (n, t, tp) => {
			this.rules.optionalType(n, t, tp, ['inout']);
		},
		integerLiteral: (n) => {
			let value = this.createValue('integer', n.value*1);

			return this.helpers.wrapValue('Integer', value);
		},
		intersectionType: (n, t, tp) => {
			this.rules.combiningType(n, t, tp, 'intersection');
		},
		module: () => {
			this.addControlTransfer();
			this.addScope(this.getComposite(0) ?? this.createNamespace('Global'));
			this.addDefaultMembers(this.scope);
			this.executeNodes(this.tree?.statements, (t) => t !== 'throw' ? 0 : -1);

			if(this.threw) {
				this.report(2, undefined, this.getValueString(this.controlTransfer.value));
			}

			this.removeScope();
			this.removeControlTransfer();
		},
		namespaceDeclaration: (n) => {
			this.rules.compositeDeclaration(n);
		},
		namespaceExpression: (n) => {
			return this.rules.compositeDeclaration(n, true);
		},
		nillableExpression: (n) => {
			let value;

			try {
				value = this.executeNode(n);
			} catch(error) {
				/*
				if(error !== 0) {
					throw error;
				}
				*/
			}

			return value;
		},
		nillableType: (n, t, tp) => {
			this.rules.optionalType(n, t, tp, ['default', 'nillable'], 1);
		},
		nilLiteral: () => {},
		operatorDeclaration: (n) => {
			let operator = n.operator?.value,
				precedence,
				associativity;

			if(operator == null) {
				return;
			}

			for(let entry of n.body?.statements ?? []) {
				if(entry.type === 'entry') {
					if(entry.key.type === 'identifier') {
						if(entry.key.value === 'precedence' && entry.value.type === 'integerLiteral') {
							precedence = entry.value.value;
						}
						if(entry.key.value === 'associativity' && entry.value.type === 'identifier' && ['left', 'right', 'none'].includes(entry.value.value)) {
							associativity = entry.value.value;
						}
					}
				}
			}

			if(precedence == null || associativity == null) {
				return;
			}

			this.setOperatorOverload(this.scope, n.operator.value, n.modifiers, precedence, associativity);
		},
		optionalType: (n, type, typePart, titles, mainTitle) => {
			if(!titles.some(v => v in typePart)) {
				typePart[titles[mainTitle ?? 0]] = true;
			}

			this.executeNode(n.value, type, typePart);
		},
		parameter: (n, type, typePart) => {
			typePart = this.helpers.createTypePart(type, typePart, n.type_);

			if(n.label != null) {
				typePart.label = n.label.value;
			}

			typePart.identifier = n.identifier.value;

			if(n.value != null) {
				typePart.value = n.value;
			}
		},
		parenthesizedExpression: (n) => {
			return this.executeNode(n.value);
		},
		parenthesizedType: (n, type, typePart) => {
			this.executeNode(n.value, type, typePart);
		},
		postfixExpression: (n) => {
			let value = this.executeNode(n.value);

			if(value == null) {
				return;
			}

			if(['float', 'integer'].includes(value.primitiveType)) {
				if(n.operator?.value === '++') {
					value.primitiveValue = value.primitiveValue*1+1;

					return this.createValue(value.primitiveType, value.primitiveValue-1);
				}
				if(n.operator?.value === '--') {
					value.primitiveValue = value.primitiveValue*1-1;

					return this.createValue(value.primitiveType, value.primitiveValue+1);
				}
			}

			// TODO: Dynamic operators lookup

			return value;
		},
		predefinedType: (n, type, typePart) => {
			typePart.predefined = n.value;
		},
		prefixExpression: (n) => {
			let value = this.executeNode(n.value);

			if(value == null) {
				return;
			}

			if(n.operator?.value === '!' && value.primitiveType === 'boolean') {
				return this.createValue('boolean', !value.primitiveValue);
			}
			if(['float', 'integer'].includes(value.primitiveType)) {
				if(n.operator?.value === '-') {
					return this.createValue(value.primitiveType, -value.primitiveValue);
				}
				if(n.operator?.value === '++') {
					value.primitiveValue = value.primitiveValue*1+1;

					return this.createValue(value.primitiveType, value.primitiveValue);
				}
				if(n.operator?.value === '--') {
					value.primitiveValue = value.primitiveValue*1-1;

					return this.createValue(value.primitiveType, value.primitiveValue);
				}
			}

			// TODO: Dynamic operators lookup

			return value;
		},
		protocolDeclaration: (n) => {
			this.rules.compositeDeclaration(n);
		},
		protocolExpression: (n) => {
			return this.rules.compositeDeclaration(n, true);
		},
		protocolType: (n, type, typePart) => {
			// TODO: This
		},
		returnStatement: (n) => {
			let value = this.executeNode(n.value);

			if(this.threw) {
				return;
			}

			this.setControlTransfer(value, 'return');

			return value;
		},
		stringLiteral: (n, primitive) => {
			let string = '';

			for(let segment of n.segments) {
				if(segment.type === 'stringSegment') {
					string += segment.value;
				} else {
					string += this.getValueString(this.executeNode(segment.value));
				}
			}

			let value = this.createValue('string', string);

			return this.helpers.wrapValue('String', value);
		},
		structureDeclaration: (n) => {
			this.rules.compositeDeclaration(n);
		},
		structureExpression: (n) => {
			return this.rules.compositeDeclaration(n, true);
		},
		throwStatement: (n) => {
			let value = this.executeNode(n.value);

			this.setControlTransfer(value, 'throw');

			return value;
		},
		typeExpression: (n) => {
			let type = [],
				typePart = this.helpers.createTypePart(type, undefined, n.type_);

			return this.createValue('type', type);
		},
		typeIdentifier: (n, type, typePart) => {
			let composite = this.getValueComposite(this.rules.identifier(n.identifier));

			if(composite == null || this.compositeIsObject(composite)) {
				typePart.predefined = 'Any';

				this.report(2, n, 'Composite is an object or wasn\'t found.');
			} else {
				typePart.reference = this.getOwnID(composite, true);
			}

			if(n.genericArguments.length === 0) {
				return;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { genericArguments: true });

			for(let genericArgument of n.genericArguments) {
				this.helpers.createTypePart(type, typePart, genericArgument);
			}
		},
		unionType: (n, t, tp) => {
			this.rules.combiningType(n, t, tp, 'union');
		},
		variableDeclaration: (n) => {
			let modifiers = n.modifiers;

			for(let declarator of n.declarators) {
				let identifier = declarator.identifier.value,
					type = [],
					typePart = this.helpers.createTypePart(type, undefined, declarator.type_),
					value = this.executeNode(declarator.value),
					observers = []

				// Type-related checks

				this.setMemberOverload(this.scope, identifier, modifiers, type, value, observers);
			}
		},
		variadicGenericParameter: () => {
			return {
				identifier: undefined,
				type: [this.createTypePart(undefined, undefined, { variadic: true })]
			}
		},
		variadicType: (n, t, tp) => {
			this.rules.optionalType(n, t, tp, ['variadic']);
		},
		whileStatement: (n) => {
			if(n.condition == null) {
				return;
			}

			let namespace = this.createNamespace('Local<'+(this.scope.title ?? '#'+this.getOwnID(this.scope))+', While>', this.scope, null),
				condition;

			this.addScope(namespace);

			while(true) {
				this.removeMembers(namespace);

				condition = this.executeNode(n.condition);
				condition = condition?.primitiveType === 'boolean' ? condition.primitiveValue : condition != null;

				if(!condition) {
					break;
				}

				if(n.value?.type === 'functionBody') {
					this.executeNodes(n.value.statements);
				} else {
					this.setControlTransfer(this.executeNode(n.value));
				}

				let CTT = this.controlTransfer?.type;

				if(['break', 'continue'].includes(CTT)) {
					this.resetControlTransfer();
				}
				if(['break', 'return'].includes(CTT)) {
					break;
				}
			}

			this.removeScope();

			return this.controlTransfer?.value;
		}
	}

	static helpers = {
		createOrSetCollectionTypePart: (type, typePart, collectionFlag) => {
			let collectionFlags = {
				array: true,
				dictionary: true,
				genericArguments: true,
				genericParameters: true,
				intersection: true,
				parameters: true,
				predefined: 'Function',
				union: true
			}

			for(let flag in typePart) {
				if(
					flag in collectionFlags &&
					typePart[flag] === collectionFlags[flag]
				) {
					return this.createTypePart(type, typePart, collectionFlag);
				}
			}

			return Object.assign(typePart, collectionFlag);
		},
		createTypePart: (type, typePart, node, fallback = true) => {
			typePart = this.createTypePart(type, typePart);

			this.executeNode(node, type, typePart);

			if(fallback) {
				for(let flag in typePart) {
					if(flag !== 'super') {
						return typePart;
					}
				}

				typePart.predefined = '_';
				typePart.nillable = true;
			}

			return typePart;
		},
		separateStatements: (statements, objectStatements) => {
			let excluded = ['initializerDeclaration'],	// Static only
				preserved = ['functionDeclaration']		//		  or both

			for(let i = 0; i < statements.length; i++) {
				let statement = statements[i]

				if(excluded.includes(statement.type)) {
					continue;
				}

				if(!statement.modifiers?.includes('static')) {
					objectStatements.push(statement);

					if(preserved.includes(statement.type)) {
						continue;
					}

					statements.splice(i, 1);
					i--;
				}
			}
		},
		getMemberOverloadSearchParameters: (node) => {
			if(node.type === 'identifier') {
				return {
					composite: this.scope,
					identifier: node.value,
					internal: false
				}
			} else
			if(node.type === 'chainExpression') {
				return {
					composite: this.getValueComposite(this.executeNode(node.composite)),
					identifier: node.member.type === 'identifier' ? node.member.value : this.rules.stringLiteral(node.member, true).primitiveValue,
					internal: true
				}
			}
		},
		wrapValue: (wrapperIdentifier, value) => {
			let args = [{ label: undefined, value: value }],
				function_ = this.findFunction(this.scope, wrapperIdentifier, false, undefined, args);

			if(function_ != null) {
				return this.callFunction(function_, undefined, args);
			}

			return value;
		}
	}

	static getSave() {
		// https://isocpp.org/wiki/faq/serialization

		// Can be useful as the optimization step for imports (kind of precompliation)
		// Then source code changes should be supported using hashes or anything

		/*
		return {
			tokens: JSON.stringify(this.tokens),
			tree: JSON.stringify(this.tree),
			composites: JSON.stringify(this.composites),
			calls: JSON.stringify(this.scopes),
			preferences: JSON.stringify(this.preferences),
			reports: JSON.stringify(this.reports)
		}
		*/

		return JSON.stringify(this.composites, (k, v) => {
			if(v instanceof Map) {
				return {
					__TYPE__: 'Map',
					__VALUE__: Array.from(v.entries())
				};
			} else
			if(v == null) {
				return null;
			} else {
				return v;
			}
		});
	}

	static restoreSave(save) {}

	static getComposite(ID) {
		return this.composites[ID]
	}

	static createComposite(title, type, scope) {
		if(!this.typeIsComposite(type)) {
			return;
		}

		let composite = {
			title: title,
			IDs: {
				own: this.composites.length,
				super: undefined,
				Super: undefined,
				self: undefined,
				Self: undefined,
				sub: undefined,
				Sub: undefined,
				scope: undefined,
				retainers: []
			},
			life: 1,  // 0 - Creation (, Initialization?), 1 - Idle (, Deinitialization?), 2 - Destruction
			type: type,
			statements: [],
			imports: {},
			operators: {},
			members: {},
			observers: []
		}

		this.composites.push(composite);

		if(scope != null) {
			this.setScopeID(composite, scope);
		}

	//	this.print('cr: '+composite.title+', '+this.getOwnID(composite));

		return composite;
	}

	static destroyComposite(composite) {
		if(composite.life === 2) {
			return;
		}

	//	this.print('ds: '+composite.title+', '+this.getOwnID(composite));

		composite.life = 2;

		if(this.compositeIsObject(composite)) {
			let function_ = this.getMemberOverload(composite, 'deinit', (v) => {
				v = this.findValueFunction(v.value, []);

				if(v != null && this.getTypeFunctionProperties(v.type).deinits === 1) {
					return v;
				}
			})?.matchingValue;

			if(function_ != null) {
				this.addControlTransfer();
				this.callFunction(function_);

				if(this.threw) {
				//	return;  Let the objects destroy no matter of errors
				}

				this.removeControlTransfer();
			}
		}

		let ID = this.getOwnID(composite),
			retainersIDs = this.getRetainersIDs(composite);

		for(let composite_ of this.composites) {
			if(this.getRetainersIDs(composite_, true)?.includes(ID)) {
				this.releaseComposite(composite, composite_);

				if(this.threw) {
				//	return;  Let the objects destroy no matter of errors
				}
			}
		}

		delete this.composites[ID]

		let aliveRetainers = 0;

		for(let retainerID of retainersIDs) {
			let composite_ = this.getComposite(retainerID);

			if(composite_?.life < 2) {
				aliveRetainers++;

				// TODO: Notify retainers about destroy
			}
		}

		if(aliveRetainers > 0) {
			this.report(1, undefined, 'Composite #'+this.getOwnID(composite)+' was destroyed with a non-empty retainer list.');
		}

	//	this.print('de: '+JSON.stringify(composite));
	}

	static destroyReleasedComposite(composite) {
		if(composite != null && !this.compositeRetained(composite)) {
			this.destroyComposite(composite);
		}
	}

	static retainComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null || retainedComposite === retainingComposite) {
			return;
		}

		let retainersIDs = this.getRetainersIDs(retainedComposite),
			retainingID = this.getOwnID(retainingComposite);

		if(!retainersIDs.includes(retainingID)) {
			retainersIDs.push(retainingID);
		}
	}

	static releaseComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null || retainedComposite === retainingComposite) {
			return;
		}

		let retainersIDs = this.getRetainersIDs(retainedComposite),
			retainingID = this.getOwnID(retainingComposite);

		if(retainersIDs.includes(retainingID)) {
//			this.print('rl: '+this.getOwnID(retainingComposite)+', '+this.getOwnID(retainedComposite)+', '+this.getRetainersIDs(retainedComposite));

			retainersIDs.splice(retainersIDs.indexOf(retainingID), 1);

			this.destroyReleasedComposite(retainedComposite);
		}
	}

	/*
	 * Automatically retains or releases composite.
	 *
	 * Use of this function is highly recommended when real retainment state is unknown
	 * at the moment, otherwise basic functions can be used in favour of performance.
	 */
	static retainOrReleaseComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null || retainedComposite === retainingComposite) {
			return;
		}

		if(this.compositeRetains(retainingComposite, retainedComposite)) {
			this.retainComposite(retainingComposite, retainedComposite);
		} else {
			this.releaseComposite(retainingComposite, retainedComposite);
		}
	}

	/*
	 * Returns a real state of direct retainment.
	 *
	 * Composite considered really retained if it is used by an at least one of
	 * the retainer's IDs (excluding own and retainers list), type, import, member or observer.
	 */
	static compositeRetains(retainingComposite, retainedComposite) {
		if(retainingComposite?.life < 2) return (
			this.IDsRetain(retainingComposite, retainedComposite) ||
			this.typeRetains(retainingComposite.type, retainedComposite) ||
			this.importsRetain(retainingComposite, retainedComposite) ||
			this.membersRetain(retainingComposite, retainedComposite) ||
			this.observersRetain(retainingComposite, retainedComposite)
		);
	}

	/*
	 * Returns a formal state of direct or indirect retainment.
	 *
	 * Composite considered formally retained if it or at least one of
	 * its retainers list members (recursively) can be as recognized as the retainer.
	 *
	 * retainersIDs is used to exclude a retain cycles and redundant passes
	 * from the lookup and meant to be set internally only.
	 */
	static compositeRetainsDistant(retainingComposite, retainedComposite, retainersIDs = []) {
		/*
		if(Array.isArray(retainingComposite)) {
			for(let retainingComposite_ of retainingComposite) {
				if(this.compositeRetainsDistant(retainingComposite_, retainedComposite)) {
					return true;
				}
			}

			return false;
		}
		*/

		if(retainedComposite == null || retainingComposite == null) {
			return;
		}
		if(retainedComposite === retainingComposite) {
			return true;
		}

		for(let retainerID of this.getRetainersIDs(retainedComposite)) {
			if(retainersIDs.includes(retainerID)) {
				continue;
			}

			retainersIDs.push(retainerID);

			if(this.compositeRetainsDistant(retainingComposite, this.getComposite(retainerID), retainersIDs)) {
				return true;
			}
		}
	}

	/*
	 * Returns a significant state of general retainment.
	 *
	 * Composite considered significantly retained if it is formally retained by
	 * the global namespace, a current scope composite or a current control trasfer value.
	 */
	static compositeRetained(composite) {
		return (
			this.compositeRetainsDistant(this.getComposite(0), composite) ||
			this.compositeRetainsDistant(this.scope, composite) ||
		//	this.compositeRetainsDistant(this.scopes, composite) ||  // May be useful when "with" syntax construct will be added
			this.compositeRetainsDistant(this.getValueComposite(this.controlTransfer?.value), composite)
		);
	}

	static createClass(title, scope) {
		let class_ = this.createComposite(title, [{ predefined: 'Class' }], scope);

		this.setSelfID(class_, class_);

		return class_;
	}

	static createEnumeration(title, scope) {
		let enumeration = this.createComposite(title, [{ predefined: 'Enumeration' }], scope);

		this.setSelfID(enumeration, enumeration);

		return enumeration;
	}

	static createFunction(title, statements, scope) {
		let function_ = this.createComposite(title, [{ predefined: 'Function' }], scope);

		this.setInheritedLevelIDs(function_, scope);
		this.setStatements(function_, statements);

		return function_;
	}

	/*
	 * Levels such as super, self or sub, can be self-defined (self only), inherited from another composite or missed.
	 * If levels are intentionally missed, Scope will prevail over Object and Inheritance chains at member overload search.
	 */
	static createNamespace(title, scope, levels) {
		let namespace = this.createComposite(title, [{ predefined: 'Namespace' }], scope);

		if(levels != null) {
			this.setInheritedLevelIDs(namespace, levels);
		} else
		if(levels === null) {
			this.setMissedLevelIDs(namespace);
		} else {
			this.setSelfID(namespace, namespace);
		}

		return namespace;
	}

	static createObject(superObject, selfComposite, subObject) {
		let title = 'Object<'+(selfComposite.title ?? '#'+this.getOwnID(selfComposite))+'>',
			type = [{ predefined: 'Object', inheritedTypes: true }, { super: 0, reference: this.getOwnID(selfComposite) }],
			object = this.createComposite(title, type, selfComposite);

		object.life = 0;

		if(superObject != null) {
			this.setSuperID(object, superObject);
		}

		this.setSelfID(object, object);

		if(subObject != null) {
			this.setSubID(object, subObject);
		}

		return object;
	}

	static createProtocol(title, scope) {
		let protocol = this.createComposite(title, [{ predefined: 'Protocol' }], scope);

		this.setSelfID(protocol, protocol);

		return protocol;
	}

	static createStructure(title, scope) {
		let structure = this.createComposite(title, [{ predefined: 'Structure' }], scope);

		this.setSelfID(structure, structure);

		return structure;
	}

	static compositeIsFunction(composite) {
		return this.typeIsComposite(composite?.type, 'Function');
	}

	static compositeIsNamespace(composite) {
		return this.typeIsComposite(composite?.type, 'Namespace');
	}

	static compositeIsObject(composite) {
		return this.typeIsComposite(composite?.type, 'Object');
	}

	static compositeIsInstantiable(composite) {
		return (
			this.typeIsComposite(composite?.type, 'Class') ||
			this.typeIsComposite(composite?.type, 'Structure')
		);
	}

	static compositeIsCallable(composite) {
		return (
			this.compositeIsFunction(composite) ||
			this.compositeIsInstantiable(composite)
		);
	}

	static addCall(function_) {
		this.calls.push({
			function: function_,
			location: this.tokens[this.position]?.location
		});
	}

	static removeCall() {
		this.calls.pop();
	}

	static getCallsString() {
		let result = '';

		for(let i = this.calls.length-1, j = 0; i >= 0 && j < 8; i--) {
			let call = this.calls[i],
				function_ = call.function,
				location = call.location;

			if(j > 0) {
				result += '\n';
			}

			result += j+': ';

			let composite = this.getComposite(function_.IDs.Self);

			if(function_.title != null && composite != null) {
				result += (composite.title ?? '#'+this.getOwnID(composite))+'.';
			}

			result += function_.title ?? '#'+this.getOwnID(function_);

			if(location != null) {
				result += ':'+(location.line+1)+':'+(location.column+1);
			}

			j++;
		}

		return result;
	}

	static get scope() {
		return this.getScope();
	}

	static getScope(offset = 0) {
		return this.scopes[this.scopes.length-1+offset]
	}

	/*
	 * Should be used for a bodies before executing its contents,
	 * as ARC utilizes a current scope at the moment.
	 */
	static addScope(composite) {
		this.scopes.push(composite);
	//	this.print('crs: '+composite.title+', '+this.getOwnID(composite));
	}

	/*
	 * Removes from the stack and optionally automatically destroys a last scope.
	 */
	static removeScope(destroy = true) {
	//	this.print('dss: '+this.scope.title+', '+this.getOwnID(this.scope));
		let composite = this.scopes.pop();

		if(destroy) {
			this.destroyReleasedComposite(composite);
		}
	//	this.print('des: '+composite.title+', '+this.getOwnID(composite));
	}

	static get controlTransfer() {
		return this.controlTransfers.at(-1);
	}

	static get threw() {
		return this.controlTransfer?.type === 'throw';
	}

	static addControlTransfer() {
		this.controlTransfers.push({
			value: undefined,
			type: undefined
		});

	//	this.print('act');
	}

	static removeControlTransfer() {
		this.controlTransfers.pop();

	//	this.print('rct: '+JSON.stringify(this.controlTransfer?.value)+', '+JSON.stringify(this.controlTransfer?.type));
	}

	/*
	 * Should be used for a return values before releasing its scopes,
	 * as ARC utilizes a control trasfer value at the moment.
	 *
	 * Specifying a type means explicit control transfer.
	 */
	static setControlTransfer(value, type) {
		let CT = this.controlTransfers.at(-1);

		if(CT != null) {
			CT.value = value;
			CT.type = type;
		}

	//	this.print('cts: '+JSON.stringify(value)+', '+JSON.stringify(type));
	}

	static resetControlTransfer() {
		let CT = this.controlTransfers.at(-1);

		if(CT != null) {
			CT.value =
			CT.type = undefined;
		}

	//	this.print('ctr');
	}

	static executeNode(node, ...arguments_) {
		let OP = this.position,  // Old/new position
			NP = node?.range?.start,
			value;

		this.position = NP;
		value = this.rules[node?.type]?.(node, ...arguments_);
		this.position = OP;

		return value;
	}

	/*
	 * Last statement in a body will be treated like a local return (overwritable by subsequent
	 * outer statements) if there was no explicit control transfer previously. ECT should
	 * be implemented by rules.
	 *
	 * Control transfer result can be discarded (fully - 1, value only - 0).
	 */
	static executeNodes(nodes, CTDiscarded) {
		let OP = this.position;  // Old position

		for(let node of nodes ?? []) {
			let NP = node.range?.start,  // New position
				start = this.composites.length,
				value = this.executeNode(node),
				end = this.composites.length;

			if(node === nodes.at(-1) && this.controlTransfer?.type == null) {  // Implicit control transfer
				this.setControlTransfer(value);
			}

			switch(CTDiscarded?.(this.controlTransfer?.type, this.controlTransfer?.value)) {
				case 0: this.setControlTransfer(undefined, this.controlTransfer?.type); break;
				case 1: this.resetControlTransfer();									break;
			}

			this.position = NP;  // Consider deinitializers

			for(start; start < end; start++) {
				this.destroyReleasedComposite(this.getComposite(start));

				if(this.threw) {
					break;
				}
			}

			if(this.controlTransfer?.type != null) { // Explicit control transfer is done
				break;
			}
		}

		this.position = OP;
	}

	/*
	 * Levels such as super, self or sub, can be inherited from another composite (function self object).
	 * If that composite is an unitialized object, it will be initialized with statements stored by its Self.
	 *
	 * Scope, that statements will be executed within, will be created automatically.
	 */
	static callFunction(function_, gargs = [], args = []) {
		if(this.calls.length === this.preferences.callStackSize) {
			this.report(2, undefined, 'Maximum call stack size exceeded.\n'+this.getCallsString());

			return;
		}

		if(typeof function_.statements === 'function') {
			return function_.statements(args);
		}

		let properties = this.getTypeFunctionProperties(function_.type),
			inits = properties.inits === 1,
			deinits = properties.deinits === 1;

		if(inits && deinits) {
			this.report(2, undefined, 'Function has ambiguous properties.');

			return;
		}

		let FSC, FSO;  // Function self composite/object

		if(inits) {
			FSC = this.getComposite(function_.IDs.Self);

			if(gargs.length === 0) {  // Find initializer's object in scope's object chain
				let SSSO = this.getComposite(this.scope?.IDs.self);  // Scope self/super object

				if(this.compositeIsObject(SSSO)) {
					while(SSSO != null) {
						if(FSC === this.getComposite(SSSO.IDs.Self)) {
							if(SSSO.life === 0) {
								FSO = SSSO;
							}

							break;
						}

						SSSO = this.getComposite(SSSO.IDs.super);
					}
				}
			}

			if(FSO == null && this.compositeIsInstantiable(FSC)) {  // Or create new object chain
				let FSSC = FSC,  // Function self/super composite
					objects = []

				while(FSSC != null) {
					let object = this.createObject(undefined, FSSC, objects[0]);

					objects.unshift(object);

					if(objects.length === 1) {
						let gparams = this.getTypeGenericParameters(FSSC.type);  // Generic parameters

						for(let i = 0; i < gparams.length; i++) {
							if(gargs.length-1 < i) {
								break;
							}

							this.setMemberOverload(object, gparams[i].identifier, [], [{ predefined: 'type' }], gargs[i], []);
						}
					}
					if(objects.length > 1) {
						this.setSuperID(objects[1], objects[0]);
					}

					FSSC = this.getComposite(this.getTypeInheritedID(FSSC.type));
				}

				FSO = objects.at(-1);
			}
		}

		let title = 'Call<'+(function_.title ?? '#'+this.getOwnID(function_))+'>',
			namespace = this.createNamespace(title, function_, FSO ?? function_),
			SSC = this.getComposite(this.scope?.IDs.Self),  // Scope self composite
			params = this.getTypeFunctionParameters(function_.type),
		//	awaits = properties.awaits === 1,
			throws = properties.throws === 1;

	//	this.report(0, undefined, 'ca: '+function_.title+', '+this.getOwnID(function_));
		this.addScope(namespace);
		this.addCall(function_);

		if(SSC != null) {
			let type = [{ predefined: 'Any', nillable: true }],
				value = this.createValue('reference', this.getOwnID(SSC));

			this.setMemberOverload(this.scope, 'caller', [], type, value, []);
		}

		for(let i = 0; i < args.length; i++) {
			let arg = args[i],
				paramType = this.getSubtype(params, params[i]),
				identifier = paramType[0]?.identifier ?? '$'+i;

			this.setMemberOverload(this.scope, identifier, [], paramType, arg.value, []);
		}

		if(FSO?.life === 0) {
			this.addScope(FSO);
			this.executeNodes(FSC.statements);
			this.removeScope(false);
		}

		this.executeNodes(function_.statements, (t) => (![undefined, 'return', 'throw'].includes(t) || deinits && t !== 'throw') ? 1 : -1);

		if(FSO?.life === 0) {
			FSO.life = 1;
		}

		this.removeCall();
		this.removeScope();

		let CTV = this.controlTransfer?.value;

	//	this.report(0, undefined, 'ke: '+JSON.stringify(CTV));
		if(this.threw) {
			if(!throws) {
				this.report(1, undefined, 'Throw from non-throwing function.');
			}

			return;
		}

		this.resetControlTransfer();

		return CTV;
	}

	static setCompositeType(composite, type) {
		if(!this.typeIsComposite(type)) {
			return;
		}

		composite.type = type;
	}

	static setID(composite, key, value) {
		if(['own', 'retainers'].includes(key)) {
			return;
		}

		let OV, NV;  // Old/new value

		OV = composite.IDs[key]
		NV = composite.IDs[key] = value;

		if(OV !== NV) {
			if(key.toLowerCase() !== 'self' && NV !== -1) {  // ID chains should not be cyclic, intentionally missed IDs can't create cycles
				let composites = [],
					composite_ = composite;

				while(composite_ != null) {
					if(composites.includes(composite_)) {
						composite.IDs[key] = OV;

						return;
					}

					composites.push(composite_);

					composite_ = this.getComposite(composite_.IDs[key]);
				}
			}

			this.retainOrReleaseComposite(composite, this.getComposite(OV));
			this.retainComposite(composite, this.getComposite(NV));
		}
	}

	static getOwnID(composite, nillable) {
		return !nillable ? composite.IDs.own : composite?.IDs.own;
	}

	static getSelfCompositeID(composite) {
		return !this.compositeIsObject(composite) ? this.getOwnID(composite) : this.getTypeInheritedID(composite.type);
	}

	static getScopeID(composite) {
		return composite.IDs.scope;
	}

	static getRetainersIDs(composite, nillable) {
		return !nillable ? composite.IDs.retainers : composite?.IDs.retainers;
	}

	static setSuperID(composite, superComposite) {
		this.setID(composite, 'super', this.getOwnID(superComposite));
		this.setID(composite, 'Super', this.getSelfCompositeID(superComposite));
	}

	static setSelfID(composite, selfComposite) {
		this.setID(composite, 'self', this.getOwnID(selfComposite));
		this.setID(composite, 'Self', this.getSelfCompositeID(selfComposite));
	}

	static setSubID(object, subObject) {
		if(!this.compositeIsObject(object)) {
			return;
		}

		if(this.compositeIsObject(subObject)) {
			this.setID(object, 'sub', this.getOwnID(subObject));
			this.setID(object, 'Sub', this.getTypeInheritedID(subObject.type));
		}
	}

	static setInheritedLevelIDs(inheritingComposite, inheritedComposite) {
		if(inheritedComposite === inheritingComposite) {
			return;
		}

		let excluded = ['own', 'scope', 'retainers']

		for(let key in inheritingComposite.IDs) {
			if(!excluded.includes(key)) {
				this.setID(inheritingComposite, key, inheritedComposite?.IDs[key]);
			}
		}
	}

	static setMissedLevelIDs(missingComposite) {
		let excluded = ['own', 'scope', 'retainers']

		for(let key in missingComposite.IDs) {
			if(!excluded.includes(key)) {
				this.setID(missingComposite, key, -1);
			}
		}
	}

	static setScopeID(composite, scopeComposite) {
		this.setID(composite, 'scope', this.getOwnID(scopeComposite, true));
	}

	static IDsRetain(retainingComposite, retainedComposite) {
		let excluded = ['own', 'retainers']

		for(let key in retainingComposite.IDs) {
			if(!excluded.includes(key) && retainingComposite.IDs[key] === this.getOwnID(retainedComposite)) {
				return true;
			}
		}
	}

	static createTypePart(type, superPart, flags = {}) {
		let part = { ...flags }

		if(type != null) {
			if(superPart != null) {
				part.super = type.indexOf(superPart);
			}

			type.push(part);
		}

		return part;
	}

	static getTypeInheritedID(type) {
		let index = type.findIndex(v => v.inheritedTypes);

		if(index === -1) {
			return;
		}

		return type.find(v => v.super === index)?.reference;
	}

	static getTypeGenericParameters(type) {
		return this.getSubtype(type, type.find(v => v.genericParameters)).slice(1);
	}

	static getTypeFunctionProperties(type) {
		return {
			inits: type[0]?.inits,
			deinits: type[0]?.deinits,
			awaits: type[0]?.awaits,
			throws: type[0]?.throws
		}
	}

	static getTypeFunctionParameters(type) {
		return this.getSubtype(type, type.find(v => v.parameters)).slice(1);
	}

	static getSubtype(type, part, offset) {
		offset ??= type.indexOf(part);

		if(offset < 0) {
			return []
		}
		if(offset === 0) {
			return structuredClone(type);
		}

		let type_ = [],
			part_ = structuredClone(part);

		part_.super -= offset;

		if(part_.super < 0) {
			delete part_.super;
		}

		type_.push(part_);

		for(let part_ of type) {
			if(part_.super === type.indexOf(part)) {
				type_.push(...this.getSubtype(type, part_, offset));
			}
		}

		return type_;
	}

	static typeIsComposite(type, wantedValue, any) {
		if(type == null) {
			return;
		}

		let currentValue = type[0]?.predefined;

		if(wantedValue != null && currentValue !== wantedValue) {
			return;
		}

		let possibleValues = [
			'Class',
			'Enumeration',
			'Function',
			'Namespace',
			'Object',
			'Protocol',
			'Structure'
		]

		if(any) {
			possibleValues.push('Any');
		}

		if(!possibleValues.includes(currentValue)) {
			return;
		}

		return true;
	}

	static typeAccepts(acceptingType, acceptedType) {
		// TODO: Type equality check

		return true;
	}

	static typeRetains(retainingType, retainedComposite) {
		return retainingType.some(v => v.reference === this.getOwnID(retainedComposite));
	}

	static createValue(primitiveType, primitiveValue) {
		return {
			primitiveType: primitiveType,	// 'boolean', 'dictionary', 'float', 'integer', 'pointer', 'reference', 'string', 'type'
			primitiveValue: primitiveValue	// boolean, integer, map (object), string, type
		}
	}

	static getValueType(value) {
		if(value == null) {
			return []
		}

		let type = [],
			part = this.createTypePart(type);

		if(!['pointer', 'reference'].includes(value.primitiveType)) {
			part.predefined = value.primitiveType;
		} else {
			if(value.primitiveType === 'pointer') {
				part.inout = true;
			}

			part.reference = value.primitiveValue;

			let composite = this.getValueComposite(value);

			while(composite != null) {
				if(!this.compositeIsObject(composite)) {
					if(this.getOwnID(composite) === value.primitiveValue) {
						part.self = true;
					} else {
						part.reference = this.getOwnID(composite);
					}

					composite = undefined;
				} else {
					composite = this.getComposite(this.getScopeID(composite));
				}
			}
		}

		return type;
	}

	static getValueComposite(value) {
		if(!['pointer', 'reference'].includes(value?.primitiveType)) {
			return;
		}

		return this.getComposite(value.primitiveValue);
	}

	static getValueString(value) {
		if(['boolean', 'float', 'integer', 'string'].includes(value?.primitiveType)) {
			return value.primitiveValue+'';
		}

		let composite = this.getValueComposite(value);

		if(composite != null) {
			return JSON.stringify(composite);
		}

		if(value != null) {
			return JSON.stringify(value, (k, v) => {
				if(v instanceof Map) {
					return {
						__TYPE__: 'Map',
						__VALUE__: Array.from(v.entries())
					}
				} else
				if(v == null) {
					return null;
				} else {
					return v;
				}
			});
		}

		return 'nil';
	}

	static getValueFunction(value, arguments_) {
		let function_ = this.getValueComposite(value);

		if(!this.compositeIsFunction(function_)) {
			return;
		}

		let parameters = this.getTypeFunctionParameters(function_.type);

		if(arguments_.length !== parameters.filter(v => v.super === 0).length) {
			// TODO: Variadic parameters support

			return;
		}

		for(let i = 0; i < (arguments_ ?? []).length; i++) {
			let argument = arguments_[i],
				parameter = parameters[i]

			if(parameter.label != null && argument.label !== parameter.label) {
				return;
			}
			if(!this.typeAccepts(parameter, this.getValueType(argument.value))) {
				return;
			}
		}

		return function_;
	}

	static findValueFunction(value, arguments_) {
		let function_;

		if(value?.primitiveType === 'dictionary') {
			for(let [key, value_] of value.primitiveValue) {
				if((function_ = this./*find*/getValueFunction(value_, arguments_)) != null) {
					break;
				}
			}
		} else {
			function_ = this.getValueFunction(value, arguments_);
		}

		return function_;
	}

	static retainOrReleaseValueComposites(retainingComposite, retainingValue) {
		if(retainingValue?.primitiveType === 'dictionary') {
			for(let [retainingKey, retainingValue_] of retainingValue.primitiveValue) {
				this.retainOrReleaseValueComposites(retainingComposite, retainingKey);
				this.retainOrReleaseValueComposites(retainingComposite, retainingValue_);
			}
		} else {
			this.retainOrReleaseComposite(retainingComposite, this.getValueComposite(retainingValue));
		}
	}

	static valueRetains(retainingValue, retainedComposite) {
		if(retainingValue?.primitiveType === 'dictionary') {
			for(let [retainingKey, retainingValue_] of retainingValue.primitiveValue) {
				if(
					this.valueRetains(retainingKey, retainedComposite) ||
					this.valueRetains(retainingValue_, retainedComposite)
				) {
					return true;
				}
			}
		} else {
			return this.getValueComposite(retainingValue) === retainedComposite;
		}
	}

	static setStatements(callableComposite, statements) {
		if(!this.compositeIsCallable(callableComposite)) {
			return;
		}

		callableComposite.statements = statements ?? []
	}

	static getImport(composite, identifier) {
		return composite.imports[identifier]
	}

	static findImport(composite, identifier) {
		while(composite != null) {
			let import_ = this.getImport(composite, identifier);

			if(import_ != null) {
				return import_;
			}

			composite = this.getComposite(this.getScopeID(composite));
		}
	}

	static setImport(namespace, identifier, value) {
		if(!this.compositeIsNamespace(namespace)) {
			return;
		}

		namespace.imports[identifier] = value;
	}

	static deleteImport(composite, identifier) {
		let value = this.getComposite(this.getImport(composite, identifier));

		delete composite.imports[identifier]

		this.retainOrReleaseComposite(composite, value);
	}

	static importsRetain(retainingComposite, retainedComposite) {
		for(let identifier in retainingComposite.imports) {
			if(retainingComposite.imports[identifier] === this.getOwnID(retainedComposite)) {
				return true;
			}
		}
	}

	static getOperator(composite, identifier) {
		return composite.operators[identifier]
	}

	static addOperator(namespace, identifier) {
		if(!this.compositeIsNamespace(namespace)) {
			return;
		}

		return this.getOperator(namespace, identifier) ?? (namespace.operators[identifier] = []);
	}

	static removeOperator(composite, identifier) {
		delete composite.operators[identifier]
	}

	static getOperatorOverload(composite, identifier, matching) {
		let operator = this.getOperator(composite, identifier);

		if(operator != null) {
			for(let overload of operator) {
				if(matching == null || matching(overload)) {
					return overload;
				}
			}
		}
	}

	static findOperatorOverload(composite, identifier, matching) {
		while(composite != null) {
			let overload = this.getOperatorOverload(composite, identifier, matching);

			if(overload != null) {
				return overload;
			}

			composite = this.getComposite(this.getScopeID(composite));
		}
	}

	static setOperatorOverload(namespace, identifier, modifiers, associativity, precedence) {
		let type = modifiers.find(v => ['prefix', 'postfix', 'infix'].includes(v));

		if(type == null) {
			return;
		}

		let operator = this.addOperator(namespace, identifier);

		if(operator == null) {
			return;
		}

		let overload = this.getOperatorOverload(namespace, identifier, (v) => v.modifiers.includes(type)) ?? (operator[operator.push({})-1]);

		overload.modifiers = modifiers;
		overload.associativity = associativity;
		overload.precedence = precedence;
	}

	static getMember(composite, identifier) {
		return composite.members[identifier]
	}

	static addMember(composite, identifier) {
		return this.getMember(composite, identifier) ?? (composite.members[identifier] = []);
	}

	static addDefaultMembers(composite) {
		let print = this.createFunction('print', (args) => {
				this.print(this.getValueString(args[0].value));
			}, composite),
			getComposite = this.createFunction('getComposite', (args) => {
				return this.createValue('reference', args[0].value.primitiveValue);
			}, composite),
			getCallsString = this.createFunction('getCallsString', () => {
				return this.createValue('string', this.getCallsString());
			}, composite);

		print.type = [
			{ predefined: 'Function', inits: -1, deinits: -1, awaits: -1, throws: -1 },
			{ super: 0, parameters: true },
			{ super: 1, predefined: '_', nillable: true, identifier: 'value' },
			{ super: 0, predefined: '_', nillable: true }
		]
		getComposite.type = [
			{ predefined: 'Function', inits: -1, deinits: -1, awaits: -1, throws: -1 },
			{ super: 0, parameters: true },
			{ super: 1, predefined: 'integer', identifier: 'value' },
			{ super: 0, predefined: 'Any', nillable: true }
		]
		getCallsString.type = [
			{ predefined: 'Function', inits: -1, deinits: -1, awaits: -1, throws: -1 },
			{ super: 0, predefined: 'string' }
		]

		this.setMemberOverload(composite, 'print', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(print)), []);
		this.setMemberOverload(composite, 'getComposite', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(getComposite)), []);
		this.setMemberOverload(composite, 'getCallsString', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(getCallsString)), []);
	}

	static removeMember(composite, identifier) {
		let member = this.getMember(composite, identifier);

		if(member == null) {
			return;
		}

		delete composite.members[identifier]

		for(let overload of member) {
			this.retainOrReleaseValueComposites(composite, overload.value);
		}
	}

	static removeMembers(composite) {
		for(let identifier in composite.members) {
			this.removeMember(composite, identifier);
		}
	}

	static getMemberOverload(composite, identifier, matching, default_ = true) {
		let member = this.getMember(composite, identifier);

		if(member == null && default_) {
			let IDs = {
			//	global: () => undefined,			  Global-object is no thing
				Global: () => 0,				   // Global-type
				super: () => composite.IDs.super,  // Super-object or a type
				Super: () => composite.IDs.Super,  // Super-type
				self: () => composite.IDs.self,	   // Self-object or a type
				Self: () => composite.IDs.Self,	   // Self-type
				sub: () => composite.IDs.sub,	   // Sub-object
				Sub: () => composite.IDs.Sub	   // Sub-type
			//	metaSelf: () => undefined,			  Self-object or a type (descriptor)
			//	arguments: () => undefined			  Function arguments array (should be in callFunction() if needed)
			}

			if(identifier in IDs) {
				let ID = IDs[identifier]();

				if(ID !== -1) {  // Intentionally missed IDs should be treated like non-existent
					member = [{
						modifiers: ['final'],
						type: [{ predefined: 'Any', nillable: true }],
						value: ID != null ? this.createValue('reference', ID) : undefined,
						observers: []
					}]
				}
			}
		}

		if(member != null) {
			for(let overload of member) {
				if(matching == null) {
					return overload;
				}

				let matchingValue = matching(overload);

				if(matchingValue != null) {
					return this.getMemberOverloadProxy(overload, { matchingValue: matchingValue });
				}
			}
		}
	}

	static getMemberOverloadProxy(overload, additions) {
		if(overload.additions != null) {
			for(let key in additions) {
				overload.additions[key] = additions[key]
			}

			return overload;
		}

		return new Proxy(overload, {
			get(target, key, receiver) {
				if(key === 'additions') {
					return additions;
				}
				if(key in additions) {
					return additions[key]
				}

				return overload[key]
			},
			set(target, key, value) {
				if(key === 'additions') {
					return true;
				}
				if(key in additions) {
					additions[key] = value;

					return true;
				}

				overload[key] = value;

				return true;
			}
		});
	}

	/*
	 * Looking for function among member overloads or in a value.
	 * Tries to find intitializer for instantiable composites.
	 */
	static findFunction(composite, identifier, internal, value, args) {
		let overload = composite != null && identifier != null,
			function_ = overload
					  ? this.findMemberOverload(composite, identifier, (v) => this.findValueFunction(v.value, args), internal)?.matchingValue
					  : this.findValueFunction(value, args);

		if(function_ != null) {
			return function_;
		}

		composite = overload
				  ? this.findMemberOverload(composite, identifier, (v) => this.getValueComposite(v.value), internal)?.matchingValue
				  : this.getValueComposite(value);

		if(this.compositeIsObject(composite) && ['super', 'self', 'sub'].includes(identifier)) {
			composite = this.getComposite(composite.IDs.Self);
		}
		if(this.compositeIsInstantiable(composite)) {
			function_ = this.getMemberOverload(composite, 'init', (v) => {
				v = this.findValueFunction(v.value, args);

				if(v != null && this.getTypeFunctionProperties(v.type).inits === 1) {
					return v;
				}
			})?.matchingValue;
		}

		return function_;
	}

	/*
	 * Looking for member overload in Scope chain (scope).
	 *
	 * If search is internal, only first composite in chain will be checked.
	 */
	static findMemberOverload(composite, identifier, matching, internal) {
		while(composite != null) {
			let overload =
				this.findMemberOverloadInComposite(composite, identifier, matching) ??
				this.findMemberOverloadInObjectChain(composite, identifier, matching) ??
				this.findMemberOverloadInInheritanceChain(composite, identifier, matching);

			if(overload != null) {
				return overload;
			}

			if(internal) {
				break;
			}

			composite = this.getComposite(this.getScopeID(composite));
		}
	}

	/*
	 * Looking for member overload in a function or namespace.
	 *
	 * This types are slightly different from other. They can inherit or miss levels and eventually aren't
	 * inheritable nor instantiable themself. But they are still used as members storage
	 * and can participate in plain scope chains.
	 */
	static findMemberOverloadInComposite(composite, identifier, matching) {
		if(
			!this.compositeIsFunction(composite) &&
			!this.compositeIsNamespace(composite)
		) {
			return;
		}

		let overload = this.getMemberOverload(composite, identifier, matching);

		if(overload != null) {
			return this.getMemberOverloadProxy(overload, { owner: composite });
		}

		// TODO: Imports lookup
	}

	/*
	 * Looking for member overload in Object chain (super, self, sub).
	 *
	 * When a "virtual" overload is found, search oncely displaces to a lowest sub-object.
	 */
	static findMemberOverloadInObjectChain(object, identifier, matching) {
		if(!this.compositeIsObject(object)) {
			let object_ = this.getComposite(object.IDs.self);

			if(object_ === object || !this.compositeIsObject(object_)) {
				return;
			}

			object = object_;
		}

		let virtual;

		while(object != null) {  // Higher
			let overload = this.getMemberOverload(object, identifier, matching);

			if(overload != null) {
				if(!virtual && overload.modifiers.includes('virtual')) {
					virtual = true;

					let object_;

					while((object_ = this.getComposite(object.IDs.sub)) != null) {  // Lowest
						object = object_;
					}

					continue;
				}

				return this.getMemberOverloadProxy(overload, { owner: object });
			}

			object = this.getComposite(object.IDs.super);
		}
	}

	/*
	 * Looking for member overload in Inheritance chain (Super, Self).
	 */
	static findMemberOverloadInInheritanceChain(composite, identifier, matching) {
		if(this.compositeIsObject(composite)) {
			composite = this.getComposite(composite.IDs.Self);
		}

		while(composite != null) {
			let overload = this.getMemberOverload(composite, identifier, matching);

			if(overload != null) {
				return this.getMemberOverloadProxy(overload, { owner: composite });
			}

			composite = this.getComposite(composite.IDs.Super);
		}
	}

	/*
	 * Not specifying "internal" means use of direct declaration without search.
	 */
	static setMemberOverload(composite, identifier, modifiers, type, value, observers, matching, internal) {
		let overload = internal == null ? this.getMemberOverload(composite, identifier, matching, false) : this.findMemberOverload(composite, identifier, matching, internal);

		if(overload == null) {
			/*
			if(internal == null) {
				return;
			}
			*/

			let member = this.addMember(composite, identifier);

			overload = member[member.push({})-1]
		} else
		if(internal != null) {
			composite = overload.owner;
		}

		let OT = overload.type ?? [],  // Old/new type
			NT = type ?? [],
			OV = overload.value,  // Old/new value
			NV = value;

		overload.modifiers = modifiers;
		overload.type = type;
		overload.value = value;
		overload.observers = observers;

		if(OT !== NT) {
			let IDs = new Set([...OT, ...NT].map(v => v.reference));

			for(let ID of IDs) {
				this.retainOrReleaseComposite(composite, this.getComposite(ID));
			}
		}
		if(OV !== NV) {
			this.retainOrReleaseValueComposites(composite, OV);
			this.retainOrReleaseValueComposites(composite, NV);
		}
	}

	static membersRetain(retainingComposite, retainedComposite) {
		for(let identifier in retainingComposite.members) {
			for(let overload of retainingComposite.members[identifier] ?? []) {
				if(
					this.typeRetains(overload.type, retainedComposite) ||
					this.valueRetains(overload.value, retainedComposite) ||
					overload.observers.some(v => v.value === this.getOwnID(retainedComposite))
				) {
					return true;
				}
			}
		}
	}

	static getObserver() {}

	static findObserver() {}

	static setObserver(composite) {
		if(!['Class', 'Function', 'Namespace', 'Structure'].includes(composite.type[0]?.predefined)) {
			return;
		}
	}

	static deleteObserver(composite, observer) {
		composite.observers = composite.observers.filter(v => v !== observer);

		this.retainOrReleaseComposite(composite, this.getComposite(observer.value));
	}

	static observersRetain(retainingComposite, retainedComposite) {
		return retainingComposite.observers.some(v => v.value === this.getOwnID(retainedComposite));
	}

	static report(level, node, string) {
		let position = this.position,
			location = this.tokens[position]?.location ?? {
				line: 0,
				column: 0
			}

		this.reports.push({
			level: level,
			position: position,
			location: location,
			string: (node != null ? node.type+' -> ' : '')+string
		});
	}

	static print(string) {
		this.reports.push({
			level: 0,
			position: undefined,
			location: undefined,
			string: string
		});
	}

	static reset(composites, preferences) {
		this.tokens = []
		this.tree = undefined;
		this.position = undefined;
		this.composites = composites ?? []
		this.calls = []
		this.scopes = []
		this.controlTransfers = []
		this.preferences = {
			callStackSize: 128,
			allowedReportLevel: 2,
			metaprogrammingLevel: 3,
			arbitaryPrecisionArithmetics: true,
			...(preferences ?? {})
		}
		this.reports = []
	}

	static interpret(lexerResult, parserResult, composites = [], preferences = {}) {
		this.reset(composites, preferences);

		this.tokens = lexerResult.tokens;
		this.tree = structuredClone(parserResult.tree);

		this.rules.module();

		let result = {
			composites: this.composites,
			reports: this.reports
		}

		this.reset();

		return result;
	}
}