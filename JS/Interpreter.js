class Interpreter {
	static tokens;
	static tree;
	static position;
	static contexts;
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
					value.primitiveValue.set(i, value_);  // TODO: Copy or link value in accordance to type
				}
			}

			return this.getValueWrapper(value, 'Array');
		},
		arrayType: (n, t, tp) => {
			this.rules.collectionType(n, t, tp, 'array');
		},
		booleanLiteral: (n) => {
			let value = this.createValue('boolean', n.value === 'true');

			return this.getValueWrapper(value, 'Boolean');
		},
		breakStatement: (n) => {
			let value = n.label != null ? this.createValue('string', n.label.value) : undefined;

			this.setControlTransfer(value, 'break');

			return value;
		},
		callExpression: (n, inner) => {
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

			this.addContext({ args: args }, ['chainExpression', 'identifier', 'implicitChainExpression']);

			let outer = ['callExpression', 'chainExpression', 'subscriptExpression'].includes(n.callee.type),
				value = this.executeNode(n.callee, outer);

			this.removeContext();

			if(this.threw) {
				let value = this.controlTransfer.value;

				if(!inner && typeof value === 'string' && value.startsWith('Nillable')) {
					this.resetControlTransfer();
				}

				return;
			}

			let function_ = this.getValueFunction(value, args);  // TODO: Remove overhead from excess checks

			if(function_ == null) {
				this.report(2, n, 'Type: Value is not a function or initializer of specified signature.', 'throw');

				return;
			}

			return this.callFunction(function_, gargs, args);
		},
		chainDeclaration: (n) => {
			let observers = this.helpers.createObservers(n);

			this.scope.observers = observers;
		//	this.setObserver(...)
		},
		chainExpression: (n, inner) => {
			this.addContext({ args: undefined, subscript: undefined }, ['chainExpression', 'identifier', 'implicitChainExpression']);  // Prevent unwanted context pass

			let outer = ['callExpression', 'chainExpression', 'subscriptExpression'].includes(n.composite.type),
				value = this.executeNode(n.composite, outer);

			this.removeContext();

			if(this.threw) {
				let value = this.controlTransfer.value;

				if(!inner && typeof value === 'string' && value.startsWith('Nillable')) {
					this.resetControlTransfer();
				}

				return;
			}

			let identifier = n.member;

			if(identifier.type === 'identifier') {
				identifier = identifier.value;
			} else {  // stringLiteral
				identifier = this.executeNode(identifier, true)?.primitiveValue;

				if(this.threw) {
					return;
				}
			}

			let composite = this.getValueComposite(value);

			if(composite == null) {
				this.report(2, n, 'Type: Value is not a composite (\''+(value?.primitiveType ?? 'nil')+'\') (accessing chained \''+identifier+'\').', 'throw');

				return;
			}

			let overload = this.helpers.findMemberOverload(n, composite, identifier, true);

			if(overload == null) {
				this.report(1, n, 'Member overload wasn\'t found (accessing chained \''+identifier+'\').');

				return;
			}

			let getterValue = this.helpers.callObservers(overload.observers, this.getContext(n.type)?.assignment);

			return getterValue != null ? getterValue[0] : overload.value;
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

			typePart = this.createOrSetCollectionTypePart(type, typePart, { [composite != null ? 'genericArguments' : title]: true });

			if(title === 'dictionary') {
				this.helpers.createTypePart(type, typePart, n.key);
			}

			this.helpers.createTypePart(type, typePart, n.value);
		},
		combiningType: (n, type, typePart, title) => {
			if(n.subtypes.length === 0) {
				return;
			}

			typePart = this.createOrSetCollectionTypePart(type, typePart, { [title]: true });

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
				let typePart = this.createOrSetCollectionTypePart(composite.type, composite.type[0], { genericParameters: true });

				for(let genericParameter of genericParameters) {
					this.rules.genericParameter(genericParameter, composite.type, typePart);
				}
			}
			if(inheritedTypes?.length > 0) {
				let typePart = this.createOrSetCollectionTypePart(composite.type, composite.type[0], { inheritedTypes: true });

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
				statements = n.body?.statements ?? [],
				objectStatements = []

			if(['class', 'structure'].includes(title)) {
				this.helpers.separateStatements(statements, objectStatements);
				this.setStatements(composite, objectStatements);
			}

			if(!anonymous) {
				this.setMemberOverload(this.scope, identifier, modifiers, type, value);
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
					returnType: { type: 'predefinedType', value: 'void' }
				}),
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value = this.createValue('reference', this.getOwnID(function_));

			function_.type = signature;

			this.setMemberOverload(this.scope, identifier, [], type, value, undefined, () => {});
		},
		dictionaryLiteral: (n) => {
			let value = this.createValue('dictionary', new Map());

			for(let entry of n.entries) {
				entry = this.rules.entry(entry);

				if(entry != null) {
					value.primitiveValue.set(entry.key, entry.value);  // TODO: Copy or link value in accordance to type
				}
			}

			return this.getValueWrapper(value, 'Dictionary');
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
			this.addContext({ args: undefined });

			let v = n.values;

			if(v.length === 3 && v[1].type === 'infixOperator') {
				if(v[1].value === '=') {
					let lhs = v[0],
						composite,
						identifier,
						internal;

				//	this.addContext({ assignment: true }, [
				//		'chainExpression',
				//		'subscriptExpression',
				//		'identifier',
				//		'implicitChainExpression'
				//	]);

					if(lhs.type === 'identifier') {
						composite = this.scope;
						identifier = lhs.value;
						internal = false;
					} else
					if(lhs.type === 'chainExpression') {
						composite = this.getValueComposite(this.executeNode(lhs.composite));
						identifier = lhs.member.type === 'identifier'
								   ? lhs.member.value
								   : this.rules.stringLiteral(lhs.member, true).primitiveValue;
						internal = true;
					}

					if(composite != null && identifier != null) {
						lhs = this.findMemberOverload(composite, identifier, undefined, internal);
					} else {
						lhs = undefined;
					}

				//	this.removeContext();

					// TODO: Create member with default type if not exists

					if(lhs == null) {
						this.report(1, n, 'Cannot assign to anything but a valid identifier or chain expression.');

						return;
					}

					let rhs = this.executeNode(v[2]);

					this.setMemberOverload(composite, identifier, lhs.modifiers, lhs.type, rhs, lhs.observers, undefined, internal);

					return rhs;
				}

				let lhs = this.executeNode(v[0]),
					rhs = this.executeNode(v[2]);

				if(v[1].value === '==')	return this.createValue('boolean', lhs?.primitiveType === rhs?.primitiveType && lhs?.primitiveValue === rhs?.primitiveValue);
				if(v[1].value === '<=')	return this.createValue('boolean', lhs?.primitiveValue <= rhs?.primitiveValue);
				if(v[1].value === '>=')	return this.createValue('boolean', lhs?.primitiveValue >= rhs?.primitiveValue);
				if(v[1].value === '<')	return this.createValue('boolean', lhs?.primitiveValue < rhs?.primitiveValue);
				if(v[1].value === '>')	return this.createValue('boolean', lhs?.primitiveValue > rhs?.primitiveValue);

				if(v[1].value === '-') {
					if(lhs?.primitiveType !== 'integer' || rhs?.primitiveType !== 'integer') {
						return;
					}

					return this.createValue('integer', lhs?.primitiveValue-rhs?.primitiveValue);
				}
				if(v[1].value === '+') {
					if(lhs?.primitiveType !== 'integer' || rhs?.primitiveType !== 'integer') {
						return;
					}

					return this.createValue('integer', lhs?.primitiveValue+rhs?.primitiveValue);
				}
			}

			this.removeContext();
		},
		floatLiteral: (n) => {
			let value = this.createValue('float', n.value*1);

			return this.getValueWrapper(value, 'Float');
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
					functionScope = this.findMemberOverload(this.scope, identifier, (v) => this.findValueFunction(v.value, []), true)?.match ?? this.scope;
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

			this.setMemberOverload(this.scope, identifier, modifiers, type, value, undefined, () => {});
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
			typePart = this.createOrSetCollectionTypePart(type, typePart, { predefined: 'Function' });

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
			let composite = this.scope,
				identifier = n.value,
				overload = this.helpers.findMemberOverload(n, composite, identifier);

			if(overload == null) {
				this.report(1, n, 'Member overload wasn\'t found (accessing \''+identifier+'\').');

				return;
			}

			let getterValue = this.helpers.callObservers(overload.observers, this.getContext(n.type)?.assignment);

			return getterValue != null ? getterValue[0] : overload.value;
		},
		ifStatement: (n) => {
			if(n.condition == null) {
				return;
			}

			// TODO: Align namespace/scope creation/destroy close to functionBody execution (it will allow single statements to affect current scope)
			// Edit: What did I mean by "single statements", inline declarations like "if var a = b() {}"? That isn't even implemented in the parser right now
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
					this.setControlTransfer(this.executeNode(branch), this.controlTransfer?.type);
				}
			}

			this.removeScope();

			if(!condition && elseif) {
				this.rules.ifStatement(n.else);
			}

			return this.controlTransfer?.value;
		},
		implicitChainExpression: (n) => {
			let type = this.getContext(n.type)?.type,
				composite = this.getComposite(type?.[0]?.reference);

			this.report(0, n, 'Implicit type: '+JSON.stringify(type));

			let identifier = n.member;

			if(identifier.type === 'stringLiteral') {
				identifier = this.executeNode(n.member, true)?.primitiveValue;
			} else {
				identifier = identifier.value;
			}

			if(composite == null) {
				this.report(2, n, 'Type: Cannot get a composite from the implicit type (accessing implicitly chained \''+identifier+'\').', 'throw');

				return;
			}

			let overload = this.helpers.findMemberOverload(n, composite, identifier, true);

			if(overload == null) {
				this.report(1, n, 'Member overload wasn\'t found (accessing implicitly chained \''+identifier+'\').');

				return;
			}

			let getterValue = this.helpers.callObservers(overload.observers, this.getContext(n.type)?.assignment);

			return getterValue != null ? getterValue[0] : overload.value;
		},
		initializerDeclaration: (n) => {
			let modifiers = n.modifiers,
				identifier = 'init',
				function_ = this.createFunction(identifier, n.body?.statements, this.scope),
				signature = n.signature,
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value = this.createValue('reference', this.getOwnID(function_));

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

			this.setMemberOverload(this.scope, identifier, modifiers, type, value, undefined, () => {});
		},
		inoutExpression: (n) => {
			let value = this.executeNode(n.value);

			if(value?.primitiveType === 'pointer') {
				return value;
			}
			if(value?.primitiveType !== 'reference') {
				this.report(2, n, 'Type: Non-reference value (\''+(value?.primitiveType ?? 'nil')+'\') can\'t be used as a pointer.', 'throw');

				return;
			}

			return this.createValue('pointer', value.primitiveValue);
		},
		inoutType: (n, t, tp) => {
			this.rules.optionalType(n, t, tp, ['inout']);
		},
		integerLiteral: (n) => {
			let value = this.createValue('integer', n.value*1);

			return this.getValueWrapper(value, 'Integer');
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
			let value = this.executeNode(n.value);

			if(this.threw) {
				return;
			}

			if(value == null) {
				this.setControlTransfer('Nillable: Expected to be handled by an outer call, chain or subscript expression.', 'throw');
			}

			return value;
		},
		nillableType: (n, t, tp) => {
			this.rules.optionalType(n, t, tp, ['default', 'nillable'], 1);
		},
		nilLiteral: () => {},
		observerDeclaration: (n, type, observers) => {
			let identifier = n.identifier.value,
				statements = n.body?.statements,
				function_ = this.createFunction(identifier, statements, this.scope);

			function_.type = this.rules.functionSignature({
				type: 'functionSignature',
				genericParameters: [],
				parameters: [],
				inits: -1,
				deinits: -1,
				awaits: -1,  // 1?
				throws: -1,  // 1?
				returnType: identifier === 'get' ? type : { type: 'predefinedType', value: 'void' }
			});

			observers[identifier] = this.getOwnID(function_);
		},
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

			// TODO: Dynamic operators lookup, check for values mutability, observers notification

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

			// TODO: Dynamic operators lookup, check for values mutability, observers notification

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
		stringLiteral: (n) => {
			let string = '';

			for(let segment of n.segments) {
				if(segment.type === 'stringSegment') {
					string += segment.value;
				} else {
					string += this.getValueString(this.executeNode(segment.value));
				}
			}

			let value = this.createValue('string', string);

			return this.getValueWrapper(value, 'String');
		},
		structureDeclaration: (n) => {
			this.rules.compositeDeclaration(n);
		},
		structureExpression: (n) => {
			return this.rules.compositeDeclaration(n, true);
		},
		subscriptDeclaration: (n) => {
			let modifiers = n.modifiers,
				identifier = 'subscript',
				signature = n.signature;

			if(signature == null) {
				return;
			}

			let type = this.rules.functionSignature(signature),
				observers = this.helpers.createObservers(n, signature.returnType);

			this.setMemberOverload(this.scope, identifier, modifiers, type, undefined, observers, () => {});
		},
		subscriptExpression: (n, inner) => {
			let args = []  // Arguments

			for(let arg of n.arguments) {
				arg = this.executeNode(arg);

				if(this.threw) {
					return;
				}

				if(arg != null) {
					args.push(arg);
				}
			}


			let outer = ['callExpression', 'chainExpression', 'subscriptExpression'].includes(n.composite.type),
				value = this.executeNode(n.composite, outer);

			if(this.threw) {
				let value = this.controlTransfer.value;

				if(!inner && typeof value === 'string' && value.startsWith('Nillable')) {
					this.resetControlTransfer();
				}

				return;
			}

			let composite = this.getValueComposite(value);

			if(composite == null) {
				this.report(2, n, 'Type: Value is not a composite (\''+(value?.primitiveType ?? 'nil')+'\') (accessing subscript).', 'throw');

				return;
			}

			this.addContext({ args: args });

			let overload = this.helpers.findMemberOverload(n, composite, 'subscript', true);

			this.removeContext();

			if(overload == null) {
				this.report(1, n, 'Member overload wasn\'t found (accessing subscript).');

				return;
			}

			let getterValue = this.helpers.callObservers(overload.observers, this.getContext(n.type)?.assignment);

			return getterValue != null ? getterValue[0] : overload.value;
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

				this.report(1, n, 'Composite is an object or wasn\'t found.');
			} else {
				typePart.reference = this.getOwnID(composite, true);
			}

			if(n.genericArguments.length === 0) {
				return;
			}

			typePart = this.createOrSetCollectionTypePart(type, typePart, { genericArguments: true });

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
					value,
					observers = this.helpers.createObservers(declarator, declarator.type_);

				this.addContext({ type: type }, ['implicitChainExpression']);

				value = this.executeNode(declarator.value);

				this.removeContext();

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

			// TODO: Align namespace/scope creation/destroy close to functionBody execution (it will allow single statements to affect current scope)
			// Edit: What did I mean by "single statements", inline declarations like "if var a = b() {}"? That isn't even implemented in the parser right now
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
					this.setControlTransfer(this.executeNode(n.value), this.controlTransfer?.type);
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
		callObservers: (observers, set, args) => {
			let types = set
					  ? ['willSet', 'set', 'didSet']
					  : ['willGet', 'get', 'didGet'],
				value;

			for(let identifier of types) {
				let function_ = this.getComposite(observers[identifier]);

				if(!this.compositeIsFunction(function_)) {
					continue;
				}

				this.addControlTransfer();

				let value_ = this.callFunction(function_, undefined, args);

				if(identifier === 'get') {
					value = [value_]
				}

				this.removeControlTransfer();
			}

			return value;
		},
		createObservers: (node, type) => {
			let observers = {}

			if(node.body?.type === 'functionBody') {
				this.rules.observerDeclaration({
					type: 'observerDeclaration',
					identifier: {
						type: 'identifier',
						value: 'get'
					},
					body: node.body
				}, type, observers);
			}
			if(node.body?.type === 'observersBody') {
				for(let statement of node.body.statements) {
					this.rules.observerDeclaration(statement, type, observers);
				}
			}

			return observers;
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
		findMemberOverload: (node, composite, identifier, internal) => {
			let context = this.getContext(node.type),
				args = context?.args,
				overload = args == null && identifier !== 'subscript'
						 ? this.findMemberOverload(composite, identifier, undefined, internal)
						 : identifier !== 'subscript'
						 ? this.findFunctionalMemberOverload(composite, identifier, internal, args)
						 : this.findSubscriptedMemberOverload(composite, identifier, internal, args);

			return overload;
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
		}
	}

	static getContext(tag) {
		let flags = {}

		for(let context of this.contexts) {
			if(context.tags == null || context.tags.includes(tag)) {
				Object.assign(flags, context.flags);
			}
		}

		return flags;
	}

	/*
	 * Should be used to pass local state between super-rule and ambiguous sub-rules.
	 *
	 * Local means everything that does not belong to global execution process
	 * (calls, scopes, controlTransfers, etc), but rather to concrete rules.
	 *
	 * Rule can be considered ambiguous if it is e.g. subrule of a subrule and cannot be recognized directly.
	 *
	 * Specifying tags allows to filter current context(s).
	 */
	static addContext(flags, tags) {
		this.contexts.push({
			flags: flags,
			tags: tags
		});
	}

	static removeContext() {
		this.contexts.pop();
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
			observers: {}
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

		let deinitializer = this.findDeinitializingMemberOverload(composite)?.match;

		if(deinitializer != null) {
			this.addControlTransfer();
			this.callFunction(deinitializer);

			/*  Let the objects destroy no matter of errors
			if(this.threw) {
				return;
			}
			*/

			this.removeControlTransfer();
		}

		let ID = this.getOwnID(composite),
			retainersIDs = this.getRetainersIDs(composite);

		for(let composite_ of this.composites) {
			if(this.getRetainersIDs(composite_)?.includes(ID)) {
				this.releaseComposite(composite, composite_);

				/*  Let the objects destroy no matter of errors
				if(this.threw) {
					return;
				}
				*/
			}
		}

		this.composites[ID] = undefined;

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
			this.observersRetain(retainingComposite.observers, retainedComposite)
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

						if(gargs.length === gparams.length) {
							for(let i = 0; i < gargs.length; i++) {
								let garg = gargs[i],
									gparam = gparams[i],
									identifier = gparam.find(v => v != null).identifier,
									type = [{ predefined: 'type' }]

								this.setMemberOverload(object, identifier, [], type, garg);
							}
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

			this.setMemberOverload(this.scope, 'caller', [], type, value);
		}

		for(let i = 0; i < args.length; i++) {
			let arg = args[i],
				param = params[i],
				paramStart = param.find(v => v != null),
				identifier = paramStart?.identifier ?? '$'+i,
				type = this.getSubtype(param, paramStart);

			this.setMemberOverload(this.scope, identifier, [], type, arg.value);
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

	static getRetainersIDs(composite) {
		return composite?.IDs.retainers;
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

	static createTypePart(type, superpart, flags = {}) {
		let part = { ...flags }

		if(type != null) {
			if(superpart != null) {
				part.super = type.indexOf(superpart);
			}

			type.push(part);
		}

		return part;
	}

	static createOrSetCollectionTypePart(type, part, collectionFlag) {
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

		for(let flag in part) {
			if(
				flag in collectionFlags &&
				part[flag] === collectionFlags[flag]
			) {
				return this.createTypePart(type, part, collectionFlag);
			}
		}

		return Object.assign(part, collectionFlag);
	}

	static getTypeInheritedID(type) {
		let index = type.findIndex(v => v.inheritedTypes);

		if(index === -1) {
			return;
		}

		return type.find(v => v.super === index)?.reference;
	}

	static getTypeGenericParameters(type) {
		let parameters = [],
			superindex = type.findIndex(v => v.genericParameters);

		for(let index in type) {
			if(type[index].super === superindex) {
				parameters.push(this.getShallowSubtype(type, index));
			}
		}

		return parameters;
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
		let parameters = [],
			superindex = type.findIndex(v => v.parameters);

		for(let index in type) {
			if(type[index].super === superindex) {
				parameters.push(this.getShallowSubtype(type, index));
			}
		}

		return parameters;
	}

	static getShallowSubtype(type, superindex, subtype = []) {
		subtype[superindex] = type[superindex]

		for(let index = +superindex+1; index < type.length; index++) {
			if(type[index].super == superindex) {
				this.getShallowSubtype(type, index, subtype);
			}
		}

		return subtype;
	}

	/*
	static getShallowSubtype_(type, superpart, subtype = []) {
		let superindex = -1;

		for(let index in type) {
			let part = type[index]

			if(part === superpart) {
				subtype[index] = part;
				superindex = index;
			} else
			if(part.super == superindex) {
				this.getShallowSubtype_(type, part, subtype);
			}
		}

		return subtype;
	}
	*/

	static getSubtype(type, superpart) {
		let superindex = type.indexOf(superpart),
			parts = this.getShallowSubtype(type, superindex),
			subtype = structuredClone(parts);

		subtype.splice(0, superindex);

		for(let index in subtype) {
			let part = subtype[index]

			if('super' in part) {
				part.super -= superindex;

				if(part.super < 0) {
					delete part.super;
				}
			}
		}

		return subtype;
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

	static getValueString(value) {
		if(value == null) {
			return 'nil';
		}

		if(['boolean', 'float', 'integer', 'string'].includes(value?.primitiveType)) {
			return value.primitiveValue+'';
		}

		let composite = this.getValueComposite(value);

		if(composite != null) {
			return JSON.stringify(composite);
		}

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

	/*
	 * Returns result of identifier(value) call or plain value if no appropriate function found in current scope.
	 */
	static getValueWrapper(value, identifier) {
		let arguments_ = [{ label: undefined, value: value }],
			function_ = this.findFunctionalMemberOverload(this.scope, identifier, false, arguments_)?.match;

		if(function_ != null) {
			return this.callFunction(function_, undefined, arguments_);
		}

		return value;
	}

	static getValueComposite(value) {
		if(!['pointer', 'reference'].includes(value?.primitiveType)) {
			return;
		}

		return this.getComposite(value.primitiveValue);
	}

	static getValueFunction(value, arguments_) {
		let function_ = this.getValueComposite(value);

		if(!this.compositeIsFunction(function_)) {
			return;
		}

		let parameters = this.getTypeFunctionParameters(function_.type);

		if(arguments_.length !== parameters.length) {
			// TODO: Variadic parameters support

			return;
		}

		for(let i = 0; i < (arguments_ ?? []).length; i++) {
			let argument = arguments_[i],
				parameter = parameters[i],
				parameterStart = parameter.find(v => v != null);

			if(parameterStart.label != null && argument.label !== parameterStart.label) {
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
		if(retainingValue == null) {
			return;
		}

		if(retainingValue.primitiveType === 'dictionary') {
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
		let ID = this.getOwnID(retainedComposite);

		for(let identifier in retainingComposite.imports) {
			if(retainingComposite.imports[identifier] === ID) {
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
			}, composite),
			getTimestamp = this.createFunction('getTimestamp', () => {
				return this.createValue('integer', Date.now().toString());
			});

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
		getTimestamp.type = [
			{ predefined: 'Function', inits: -1, deinits: -1, awaits: -1, throws: -1 },
			{ super: 0, predefined: 'integer' }
		]

		this.setMemberOverload(composite, 'print', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(print)));
		this.setMemberOverload(composite, 'getComposite', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(getComposite)));
		this.setMemberOverload(composite, 'getCallsString', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(getCallsString)));
		this.setMemberOverload(composite, 'getTimestamp', [], [{ predefined: 'Function' }], this.createValue('reference', this.getOwnID(getTimestamp)));
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

	static getMemberOverload(composite, identifier, matching) {
		let member = this.getMember(composite, identifier);

		if(member != null) {
			for(let overload of member) {
				overload = this.getMemberOverloadMatch(overload, matching);

				if(overload != null) {
					return overload;
				}
			}
		}
	}

	static getMemberOverloadMatch(overload, matching) {
		if(matching == null) {
			return overload;
		}

		let match = matching(overload);

		if(match != null) {
			return this.getMemberOverloadProxy(overload, { match: match });
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

	static findFunctionalMemberOverload(composite, identifier, internal, args) {
		let overload = this.findMemberOverload(composite, identifier, (v) => this.findValueFunction(v.value, args), internal);

		if(overload != null) {
			return overload;
		}

		composite = this.findMemberOverload(composite, identifier, (v) => this.getValueComposite(v.value), internal)?.match;

		if(composite != null) {
			return this.findInitializingMemberOverload(composite, identifier, args);
		}
	}

	static findInitializingMemberOverload(composite, identifier, args) {
		if(this.compositeIsObject(composite) && ['super', 'self', 'sub'].includes(identifier)) {
			composite = this.getComposite(composite.IDs.Self);
		}
		if(this.compositeIsInstantiable(composite)) {
			return this.getMemberOverload(composite, 'init', (v) => {
				v = this.findValueFunction(v.value, args);

				if(v != null && this.getTypeFunctionProperties(v.type).inits === 1) {
					return v;
				}
			});
		}
	}

	static findDeinitializingMemberOverload(object) {
		if(this.compositeIsObject(object)) {
			return this.getMemberOverload(object, 'deinit', (v) => {
				v = this.findValueFunction(v.value, []);

				if(v != null && this.getTypeFunctionProperties(v.type).deinits === 1) {
					return v;
				}
			});
		}
	}

	static findSubscriptedMemberOverload(composite, identifier, internal, args) {}

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
	 * Looking for member overload in a composite itself.
	 *
	 * Used to find member overloads (primarily of functions and namespaces),
	 * pseudovariables (such as "Global" or "self"), and imports within namespaces.
	 *
	 * Functions and namespaces are slightly different from other types. They can inherit or miss levels and
	 * eventually aren't inheritable nor instantiable themself, but they are still used as members storage
	 * and can participate in plain scope chains.
	 */
	static findMemberOverloadInComposite(composite, identifier, matching) {
		let overload = this.getMemberOverload(composite, identifier, matching);

		if(overload == null) {
			let IDs = {
			//	global: undefined,				Global-object is no thing
				Global: 0,					 // Global-type
				super: composite.IDs.super,  // Super-object or a type
				Super: composite.IDs.Super,  // Super-type
				self: composite.IDs.self,	 // Self-object or a type
				Self: composite.IDs.Self,	 // Self-type
				sub: composite.IDs.sub,		 // Sub-object
				Sub: composite.IDs.Sub		 // Sub-type
			//	metaSelf: undefined,			Self-object or a type (descriptor) (probably not a simple ID but some kind of proxy)
			//	arguments: undefined			Function arguments array (should be in callFunction() if needed)
			}

			if(identifier in IDs) {
				let ID = IDs[identifier]

				if(ID !== -1) {  // Intentionally missed IDs should be treated like non-existent
					overload = this.getMemberOverloadMatch({
						modifiers: ['final'],
						type: [{ predefined: 'Any', nillable: true }],
						value: ID != null ? this.createValue('reference', ID) : undefined,
						observers: {}
					}, matching);
				}
			}
		}

		if(overload == null && this.compositeIsNamespace(composite)) {
			let IDs = composite.imports;

			if(identifier in IDs) {
				overload = this.getMemberOverloadMatch({
					modifiers: ['final'],
					type: [{ predefined: 'Any' }],
					value: this.createValue('reference', IDs[identifier]),
					observers: {}
				}, matching);
			} else {
				for(let identifier in IDs) {
					composite = this.getComposite(IDs[identifier]);

					if(composite == null) {
						continue;
					}

					overload = this.getMemberOverload(composite, identifier, matching);

					if(overload != null) {
						break;
					}
				}
			}
		}

		if(overload != null) {
			return this.getMemberOverloadProxy(overload, { owner: composite });
		}
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
		let overload = internal == null
					 ? this.getMemberOverload(composite, identifier, matching)
					 : this.findMemberOverload(composite, identifier, matching, internal);

		if(overload == null) {
			/*
			if(internal != null) {
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
			NV = value,
			OO = overload.observers ?? {},  // Old/new observers
			NO = observers ?? {}

		overload.modifiers = modifiers;
		overload.type = NT;
		overload.value = value;
		overload.observers = NO;

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
		if(OO !== NO) {
			let IDs = new Set([...Object.values(OO), ...Object.values(NO)]);

			for(let ID of IDs) {
				this.retainOrReleaseComposite(composite, this.getComposite(ID));
			}
		}
	}

	static membersRetain(retainingComposite, retainedComposite) {
		for(let identifier in retainingComposite.members) {
			for(let overload of retainingComposite.members[identifier] ?? []) {
				if(
					this.typeRetains(overload.type, retainedComposite) ||
					this.valueRetains(overload.value, retainedComposite) ||
					this.observersRetain(overload.observers, retainedComposite)
				) {
					return true;
				}
			}
		}
	}

	static getObserver() {}

	static findObserver() {}

	static setObserver(composite, identifier, function_) {
		if(!['Class', 'Function', 'Namespace', 'Structure'].includes(composite.type[0]?.predefined)) {
			return;
		}

		composite.observers[identifier] = this.getOwnID(function_);
	}

	static deleteObserver(composite, identifier) {
		let ID = composite.observers[identifier]

		delete composite.observers[identifier]

		this.retainOrReleaseComposite(composite, this.getComposite(ID));
	}

	static observersRetain(retainingObservers, retainedComposite) {
		let ID = this.getOwnID(retainedComposite);

		for(let identifier in retainingObservers) {
			if(retainingObservers[identifier] === ID) {
				return true;
			}
		}
	}

	static importPath(path) {
		let code = typeof require !== 'undefined' ? require('fs').readFileSync(global.__dirname+path).toString() : '';

		if(code.length === 0) {
			return;
		}

		return this.interpretRaw(code, this.composites, this.preferences);
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
		this.contexts = []
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

	static interpretRaw(code, composites = [], preferences = {}) {
 		let lexerResult = Lexer.tokenize(code),
	 		parserResult = Parser.parse(lexerResult),
	 		interpreterResult = Interpreter.interpret(lexerResult, parserResult, composites, preferences);

	 	return interpreterResult;
	}
}