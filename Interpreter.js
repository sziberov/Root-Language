class Interpreter {
	static tokens;
	static tree;
	static composites;
	static scopes;
	static controlTransfer;
	static preferences;
	static reports;

	static rules = {
		argument: (node, scope) => {
			return {
				label: node.label?.value,
				value: this.rules[node.value?.type]?.(node.value, scope)
			}
		},
		arrayLiteral: (node, scope) => {
			let result,
				values = []

			for(let value of node.values) {
				value = this.rules[value.type]?.(value, scope);

				if(value != null) {
					values.push(value);
				}
			}

			let composite = this.getValueComposite(this.findMemberOverload(scope, 'Array')?.value);

			if(composite != null) {
				// TODO: Instantinate Array()
			} else {
				result = this.createValue('dictionary', values);
			}

			return result;
		},
		arrayType: (n, s, t, tp) => {
			this.rules.collectionType(n, s, t, tp, 'array');
		},
		booleanLiteral: (node) => {
			// TODO: Instantinate Boolean()

			return this.createValue('boolean', node.value === 'true');
		},
		callExpression: (node, scope) => {
			let gargs = [],  // (Generic) arguments
				args = []

			for(let garg of node.genericArguments) {
				let type = [],
					typePart = this.helpers.createTypePart(type, undefined, garg, scope);

				gargs.push(type);
			}
			for(let arg of node.arguments) {
				arg = this.rules[arg.type]?.(arg, scope);

				if(arg != null) {
					args.push(arg);
				}
			}

			let callee = node.callee,
				calleeMOSP = this.helpers.getMemberOverloadSearchParameters(callee, scope);

			if(calleeMOSP == null) {
				callee = this.rules[callee?.type]?.(callee, scope);
			} else
			if(calleeMOSP.composite == null || calleeMOSP.identifier == null) {
				this.report(1, node, 'Cannot call anything but a valid identifier or chain expression if no other expression is used.');

				return;
			}

			for(let i = 0; i < 2; i++) {
				let function_ = calleeMOSP == null ? this.findValueFunction(callee, args) : this.findMemberOverload(calleeMOSP.composite, calleeMOSP.identifier, (v) => this.findValueFunction(v.value, args), calleeMOSP.internal)?.matchingValue,
					FSC = this.getComposite(function_?.addresses.Self) ?? calleeMOSP.composite,  // Function self composite
					initializer = (function_?.title === 'init' || calleeMOSP?.identifier === 'init') && (this.typeIsComposite(FSC?.type, 'Class') || this.typeIsComposite(FSC?.type, 'Structure'));

				if(function_ == null) {
					if(i === 0 && !initializer) {  // Fallback to search of an initializer
						calleeMOSP = {
							composite: calleeMOSP == null ? this.getValueComposite(callee) : this.findMemberOverload(calleeMOSP.composite, calleeMOSP.identifier, (v) => this.getValueComposite(v.value), calleeMOSP.internal)?.matchingValue,
							identifier: 'init',
							internal: true
						}

						if(this.typeIsComposite(calleeMOSP.composite?.type, 'Object')) {
							calleeMOSP.composite = this.getComposite(calleeMOSP.composite.addresses.Self);
						}

						continue;
					}

					this.report(2, node, (!initializer ? 'Function' : 'Initializer')+' with specified signature wasn\'t found.');

					return;
				}

			//	this.report(0, node, 'ca: '+function_.title+', '+function_.addresses.ID);
				let FSO;  // Function self object

				if(initializer) {
					let SSO = this.getComposite(scope.addresses.self);  // Scope self (super) object

					if(this.typeIsComposite(SSO?.type, 'Object')) {  // Find initializer's object in scope
						while(SSO != null) {
							if(FSC === this.getComposite(SSO.addresses.Self)) {  // TODO: Ignore object if it is not in the initialization state
								FSO = SSO;

								break;
							}

							SSO = this.getComposite(SSO.addresses.super);
						}
					}

					if(FSO == null) {  // Or create new object chain
						let objects = []

						while(FSC != null) {
							let object = this.createObject(undefined, FSC, objects[0]);

							objects.unshift(object);

							if(objects.length === 1) {
								let gparams = this.getTypeGenericParameters(FSC.type);  // Generic parameters

								for(let i = 0; i < gparams.length; i++) {
									if(gargs.length-1 < i) {
										break;
									}

									this.setMemberOverload(object, gparams[i].identifier, [], [{ predefined: 'type' }], gargs[i], []);
								}
							}
							if(objects.length > 1) {
								this.setSuperAddress(objects[1], objects[0]);
							}

							FSC = this.getComposite(this.getTypeInheritedAddress(FSC.type));
						}

						FSO = objects.at(-1);
					}
				}

				let SSC = this.getComposite(scope.addresses.Self),  // Scope self composite
					location = this.tokens[node.range.start]?.location,
					value = this.callFunction(function_, args, FSO, undefined, SSC, location);
			//	this.report(0, node, 'ke: '+JSON.stringify(value));

				return value;
			}
		},
		chainExpression: (node, scope) => {
			let composite = this.getValueComposite(this.rules[node.composite?.type](node.composite, scope));

			if(composite == null) {
				this.report(2, node, 'Composite wasn\'t found.');

				return;
			}

			let identifier = node.member;

			if(identifier.type === 'stringLiteral') {
				identifier = this.rules.stringLiteral(node.member, scope);
			} else {
				identifier = identifier.value;
			}

			return this.helpers.findMemberOverload(scope, composite, identifier, undefined, true)?.value;
		},
		classDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		classExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		collectionType: (node, scope, type, typePart, title) => {
			let capitalizedTitle = title[0].toUpperCase()+title.slice(1),
				composite = this.getValueComposite(this.findMemberOverload(scope, capitalizedTitle)?.value);

			if(composite != null) {
				typePart.reference = composite.addresses.ID;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { [composite != null ? 'genericArguments' : title]: true });

			if(title === 'dictionary') {
				this.helpers.createTypePart(type, typePart, node.key, scope);
			}

			this.helpers.createTypePart(type, typePart, node.value, scope);
		},
		combiningType: (node, scope, type, typePart, title) => {
			if(node.subtypes.length === 0) {
				return;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { [title]: true });

			for(let subtype of node.subtypes) {
				this.helpers.createTypePart(type, typePart, subtype, scope);
			}
		},
		compositeDeclaration: (node, scope, anonymous) => {
			let modifiers = node.modifiers,
				identifier = node.identifier?.value;

			if(!anonymous && identifier == null) {
				return;
			}

			let title = node.type.replace('Declaration', '')
								 .replace('Expression', ''),
				capitalizedTitle = title[0].toUpperCase()+title.slice(1),
				genericParameters = node.genericParameters,
				inheritedTypes = node.inheritedTypes,
				composite = this['create'+capitalizedTitle](identifier, scope);

			if(genericParameters?.length > 0) {
				let typePart = this.helpers.createOrSetCollectionTypePart(composite.type, composite.type[0], { genericParameters: true });

				for(let genericParameter of genericParameters) {
					this.rules.genericParameter(genericParameter, scope, composite.type, typePart);
				}
			}
			if(inheritedTypes?.length > 0) {
				let typePart = this.helpers.createOrSetCollectionTypePart(composite.type, composite.type[0], { inheritedTypes: true });

				for(let inheritedType of inheritedTypes) {
					this.helpers.createTypePart(composite.type, typePart, inheritedType, scope, false);

					// TODO: Protocol conformance checking (if not conforms, remove from type and report)
				}
			}
			for(let typePart of composite.type) {
				this.retainComposite(composite, this.getComposite(typePart.reference));
			}

			if(title === 'class') {
				let superComposite = this.getComposite(this.getTypeInheritedAddress(composite.type));

				if(superComposite != null) {
					this.setSuperAddress(composite, superComposite);
				}
			}

			let type = [this.createTypePart(undefined, undefined, { predefined: capitalizedTitle, self: true })],
				value = this.createValue('reference', composite.addresses.ID),
				observers = [],
				statements = node.body?.statements ?? [],
				objectStatements = []

			if(['class', 'structure'].includes(title)) {
				this.helpers.separateStatements(statements, objectStatements);
				this.setStatements(composite, objectStatements);
			}

			if(!anonymous) {
				this.setMemberOverload(scope, identifier, modifiers, type, value, observers);
			} else {
				this.addScope(composite);
			}

			this.executeStatements(statements, composite);

			if(anonymous) {
				this.removeScope(false);

				return value;
			}
		},
		defaultType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['default', 'nillable']);
		},
		deinitializerDeclaration: (node, scope) => {
			let identifier = 'deinit',
				function_ = this.createFunction(identifier, node.body?.statements, scope),
				signature = this.rules.functionSignature({
					type: 'functionSignature',
					genericParameters: [],
					parameters: [],
					awaits: -1,  // 1?
					throws: -1,  // 1?
					returnType: undefined
				}, scope),
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value = this.createValue('reference', function_.addresses.ID),
				observers = []

			function_.type = signature;

			this.setMemberOverload(scope, identifier, [], type, value, observers, () => {});
		},
		dictionaryLiteral: (node, scope) => {
			let result,
				entries = new Map();

			for(let entry of node.entries) {
				entry = this.rules.entry(entry, scope);

				if(entry != null) {
					entries.set(entry.key, entry.value);
				}
			}

			let composite = this.getValueComposite(this.findMemberOverload(scope, 'Dictionary')?.value);

			if(composite != null) {
				// TODO: Instantinate Dictionary()
			} else {
				result = this.createValue('dictionary', entries);
			}

			return result;
		},
		dictionaryType: (n, s, t, tp) => {
			this.rules.collectionType(n, s, t, tp, 'dictionary');
		},
		entry: (node, scope) => {
			return {
				key: this.rules[node.key.type]?.(node.key, scope),
				value: this.rules[node.value?.type]?.(node.value, scope)
			}
		},
		enumerationDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		enumerationExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		expressionsSequence: (node, scope) => {
			if(node.values.length === 3 && node.values[1].type === 'infixOperator' && node.values[1].value === '=') {
				let lhs = node.values[0],
					lhsMOSP = this.helpers.getMemberOverloadSearchParameters(lhs, scope);

				if(lhsMOSP != null && lhsMOSP.composite != null && lhsMOSP.identifier != null) {
					lhs = this.findMemberOverload(lhsMOSP.composite, lhsMOSP.identifier, undefined, lhsMOSP.internal);
				} else {
					lhs = undefined;
				}

				// TODO: Create member with default type if not exists

				if(lhs == null) {
					this.report(1, node, 'Cannot assign to anything but a valid identifier or chain expression.');

					return;
				}

				let rhs = this.rules[node.values[2].type]?.(node.values[2], scope);

				this.setMemberOverload(lhsMOSP.composite, lhsMOSP.identifier, lhs.modifiers, lhs.type, rhs, lhs.observers, undefined, lhsMOSP.internal);

				return rhs;
			}
			if(node.values.length === 3 && node.values[1].type === 'infixOperator' && node.values[1].value === '==') {
				let lhs = this.rules[node.values[0].type]?.(node.values[0], scope),
					rhs = this.rules[node.values[2].type]?.(node.values[2], scope),
					value = lhs?.primitiveType === rhs?.primitiveType && lhs?.primitiveValue === rhs?.primitiveValue;

				if(!value) {
				//	debugger;
				}

				return this.createValue('boolean', value);
			}
		},
		floatLiteral: (node) => {
			// TODO: Instantinate Float()

			return this.createValue('float', node.value*1);
		},
		functionDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier?.value;

			if(identifier == null) {
				return;
			}

			let function_,
				statements = Array.from(node.body?.statements ?? []),
				objectStatements = [],
				signature,
				functionScope,
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value,
				observers = [],
				object = this.typeIsComposite(scope.type, 'Object'),
				staticDeclaration = modifiers.includes('static') || !object && !this.typeIsComposite(scope.type, 'Class') && !this.typeIsComposite(scope.type, 'Structure');

			this.helpers.separateStatements(statements, objectStatements);

			if(staticDeclaration) {
				if(!object) {  // Static in non-object
					signature = node.signature;
					functionScope = scope;
				} else {  // Static in object
					return;
				}
			} else {
				if(object) {  // Non-static in object
					statements = []
					signature = node.signature;
					functionScope = this.findMemberOverload(scope, identifier, (v) => this.findValueFunction(v.value, []), true)?.matchingValue ?? scope;
				} else {  // Non-static in non-object
					if(statements.length === 0) {
						return;
					}

					objectStatements = []
					functionScope = scope;
				}
			}

			function_ = this.createFunction(identifier, objectStatements, functionScope);
			function_.type = this.rules.functionSignature(signature, scope);
			value = this.createValue('reference', function_.addresses.ID);

			this.setMemberOverload(scope, identifier, modifiers, type, value, observers, () => {});
			this.executeStatements(statements, function_);
		},
		functionExpression: (node, scope) => {
			let signature = this.rules.functionSignature(node.signature, scope),
				function_ = this.createFunction(undefined, node.body?.statements, scope);

			function_.type = signature;

			return this.createValue('reference', function_.addresses.ID);
		},
		functionSignature: (node, scope) => {
			let type = [],
				typePart = this.createTypePart(type, undefined, { predefined: 'Function' });

			typePart.awaits = node?.awaits ?? -1;
			typePart.throws = node?.throws ?? -1;

			for(let v of ['genericParameters', 'parameters']) {
				if(node?.[v].length > 0) {
					let typePart_ = this.createTypePart(type, typePart, { [v]: true });

					for(let node_ of node[v]) {
						this.rules[node_.type]?.(node_, scope, type, typePart_);
					}
				}
			}

			this.helpers.createTypePart(type, typePart, node?.returnType, scope);

			return type;
		},
		functionType: (node, scope, type, typePart) => {
			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { predefined: 'Function' });

			typePart.awaits = node.awaits ?? 0;
			typePart.throws = node.throws ?? 0;

			for(let v of ['genericParameter', 'parameter']) {
				if(node[v+'Types'].length > 0) {
					let typePart_ = this.createTypePart(type, typePart, { [v+'s']: true });

					for(let node_ of node[v+'Types']) {
						this.helpers.createTypePart(type, typePart_, node_, scope);
					}
				}
			}

			this.helpers.createTypePart(type, typePart, node.returnType, scope);
		},
		genericParameter: (node, scope, type, typePart) => {
			typePart = this.helpers.createTypePart(type, typePart, node.type_, scope);
			typePart.identifier = node.identifier.value;
		},
		identifier: (node, scope) => {
			return this.helpers.findMemberOverload(scope, scope, node.value)?.value;
		},
		ifStatement: (node, scope) => {
			if(node.condition == null) {
				return;
			}

			let namespace = this.createNamespace('Local<'+(scope.title ?? '#'+scope.addresses.ID)+', If>', scope),
				condition;

			this.addScope(namespace);

			condition = this.rules[node.condition.type]?.(node.condition, namespace);
			condition = condition?.primitiveType === 'boolean' ? condition.primitiveValue : condition != null;

			if(condition || node.else?.type !== 'ifStatement') {
				let branch = node[condition ? 'then' : 'else']

				if(branch?.type === 'functionBody') {
					this.executeStatements(branch.statements, namespace);
				} else {
					this.setControlTransfer(this.rules[branch?.type]?.(branch, namespace));
				}
			}

			this.removeScope();

			if(this.controlTransfer?.type === 'returnStatement') {
				return this.controlTransfer.value;
			}

			if(!condition && node.else?.type === 'ifStatement') {
				this.rules.ifStatement(node.else, scope);
			}

			return this.controlTransfer?.value;
		},
		initializerDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = 'init',
				function_ = this.createFunction(identifier, node.body?.statements, scope),
				signature = node.signature,
				type = [this.createTypePart(undefined, undefined, { predefined: 'Function' })],
				value = this.createValue('reference', function_.addresses.ID),
				observers = []

			signature ??= {
				type: 'functionSignature',
				genericParameters: [],
				parameters: [],
				awaits: -1,
				throws: -1,
				returnType: undefined
			}
			signature.returnType = {
				type: 'typeIdentifier',
				identifier: {
					type: 'identifier',
					value: 'Self'
				},
				genericArguments: []
			}

			if(node.nillable) {
				signature.returnType = {
					type: 'nillableType',
					value: signature.returnType
				}
			}

			signature = this.rules.functionSignature(signature, scope);
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

			this.setMemberOverload(scope, identifier, modifiers, type, value, observers, () => {});
		},
		inoutExpression: (node, scope) => {
			let value = this.rules[node.value?.type]?.(node.value, scope);

			if(value?.primitiveType === 'pointer') {
				return value;
			}
			if(value?.primitiveType !== 'reference') {
				this.report(2, node, 'Non-reference value ("'+(value?.primitiveType ?? 'nil')+'") can\'t be used as a pointer.');

				return;
			}

			return this.createValue('pointer', value.primitiveValue);
		},
		inoutType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['inout']);
		},
		integerLiteral: (node) => {
			// TODO: Instantinate Integer()

			return this.createValue('integer', node.value*1);
		},
		intersectionType: (n, s, t, tp) => {
			this.rules.combiningType(n, s, t, tp, 'intersection');
		},
		module: () => {
			let namespace = this.getComposite(0) ?? this.createNamespace('Global');

			let print = this.createFunction('print', (arguments_) => {
					this.print(this.getValueString(arguments_[0].value));
				}, namespace),
				getCallsString = this.createFunction('getCallsString', () => this.createValue('string', this.getCallsString()), namespace);

			print.type = [
				{
					predefined: 'Function',
					awaits: -1,
					throws: -1
				},
				{
					parameters: true,
					super: 0
				},
				{
					super: 1,
					predefined: '_',
					nillable: true,
					identifier: 'value'
				},
				{
					super: 0,
					predefined: '_',
					nillable: true
				}
			]
			getCallsString.type = [
				{
					predefined: 'Function',
					awaits: -1,
					throws: -1
				},
				{
					super: 0,
					predefined: 'string'
				}
			]

			this.setMemberOverload(namespace, 'print', [], [{ predefined: 'Function' }], this.createValue('reference', print.addresses.ID), []);
			this.setMemberOverload(namespace, 'getCallsString', [], [{ predefined: 'Function' }], this.createValue('reference', getCallsString.addresses.ID), []);

			this.addScope(namespace);
			this.executeStatements(this.tree?.statements, namespace);
			this.removeScope();
			this.resetControlTransfer();
		},
		namespaceDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		namespaceExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		nillableExpression: (node, scope) => {
			let value;

			try {
				value = this.rules[node?.type]?.(node, scope);
			} catch(error) {
				/*
				if(error !== 0) {
					throw error;
				}
				*/
			}

			return value;
		},
		nillableType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['default', 'nillable'], 1);
		},
		nilLiteral: () => {},
		operatorDeclaration: (node, scope) => {
			let operator = node.operator?.value,
				precedence,
				associativity;

			if(operator == null) {
				return;
			}

			for(let entry of node.body?.statements ?? []) {
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

			this.setOperatorOverload(scope, node.operator.value, node.modifiers, precedence, associativity);
		},
		optionalType: (node, scope, type, typePart, titles, mainTitle) => {
			if(!titles.some(v => v in typePart)) {
				typePart[titles[mainTitle ?? 0]] = true;
			}

			this.rules[node.value?.type]?.(node.value, scope, type, typePart);
		},
		parameter: (node, scope, type, typePart) => {
			typePart = this.helpers.createTypePart(type, typePart, node.type_, scope);

			if(node.label != null) {
				typePart.label = node.label.value;
			}

			typePart.identifier = node.identifier.value;

			if(node.value != null) {
				typePart.value = node.value;
			}
		},
		parenthesizedExpression: (node, scope) => {
			return this.rules[node.value?.type]?.(node.value, scope);
		},
		parenthesizedType: (node, scope, type, typePart) => {
			this.rules[node.value?.type]?.(node.value, scope, type, typePart);
		},
		postfixExpression: (node, scope) => {
			let value = this.rules[node.value?.type]?.(node.value, scope);

			if(value == null) {
				return;
			}

			if(['float', 'integer'].includes(value.primitiveType)) {
				if(node.operator?.value === '++') {
					value.primitiveValue = value.primitiveValue*1+1;

					return this.createValue(value.primitiveType, value.primitiveValue-1);
				}
				if(node.operator?.value === '--') {
					value.primitiveValue = value.primitiveValue*1-1;

					return this.createValue(value.primitiveType, value.primitiveValue+1);
				}
			}

			// TODO: Dynamic operators lookup

			return value;
		},
		predefinedType: (node, scope, type, typePart) => {
			typePart.predefined = node.value;
		},
		prefixExpression: (node, scope) => {
			let value = this.rules[node.value?.type]?.(node.value, scope);

			if(value == null) {
				return;
			}

			if(node.operator?.value === '!' && value.primitiveType === 'boolean') {
				return this.createValue('boolean', !value.primitiveValue);
			}
			if(['float', 'integer'].includes(value.primitiveType)) {
				if(node.operator?.value === '-') {
					return this.createValue(value.primitiveType, -value.primitiveValue);
				}
				if(node.operator?.value === '++') {
					value.primitiveValue = value.primitiveValue*1+1;

					return this.createValue(value.primitiveType, value.primitiveValue);
				}
				if(node.operator?.value === '--') {
					value.primitiveValue = value.primitiveValue*1-1;

					return this.createValue(value.primitiveType, value.primitiveValue);
				}
			}

			// TODO: Dynamic operators lookup

			return value;
		},
		protocolDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		protocolExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		protocolType: (node, scope, type, typePart) => {
			// TODO: This
		},
		returnStatement: (node, scope) => {
			return this.rules[node.value?.type]?.(node.value, scope);
		},
		stringLiteral: (node, scope, primitive) => {
			let string = '';

			for(let segment of node.segments) {
				if(segment.type === 'stringSegment') {
					string += segment.value;
				} else {
					string += this.getValueString(this.rules[segment.value?.type]?.(segment.value, scope));
				}
			}

			// TODO: Instantinate String()

			return this.createValue('string', string);
		},
		structureDeclaration: (node, scope) => {
			this.rules.compositeDeclaration(node, scope);
		},
		structureExpression: (node, scope) => {
			return this.rules.compositeDeclaration(node, scope, true);
		},
		typeExpression: (node, scope) => {
			let type = [],
				typePart = this.helpers.createTypePart(type, undefined, node.type_, scope);

			return this.createValue('type', type);
		},
		typeIdentifier: (node, scope, type, typePart) => {
			let composite = this.getValueComposite(this.rules.identifier(node.identifier, scope));

			if(composite == null || this.typeIsComposite(composite.type, 'Object')) {
				typePart.predefined = 'Any';

				this.report(2, node, 'Composite is an object or wasn\'t found.');
			} else {
				typePart.reference = composite?.addresses.ID;
			}

			if(node.genericArguments.length === 0) {
				return;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { genericArguments: true });

			for(let genericArgument of node.genericArguments) {
				this.helpers.createTypePart(type, typePart, genericArgument, scope);
			}
		},
		unionType: (n, s, t, tp) => {
			this.rules.combiningType(n, s, t, tp, 'union');
		},
		variableDeclaration: (node, scope) => {
			let modifiers = node.modifiers;

			for(let declarator of node.declarators) {
				let identifier = declarator.identifier.value,
					type = [],
					typePart = this.helpers.createTypePart(type, undefined, declarator.type_, scope),
					value = this.rules[declarator.value?.type]?.(declarator.value, scope),
					observers = []

				// Type-related checks

				this.setMemberOverload(scope, identifier, modifiers, type, value, observers);
			}
		},
		variadicGenericParameter: (node, scope) => {
			return {
				identifier: undefined,
				type: [this.createTypePart(undefined, undefined, { variadic: true })]
			}
		},
		variadicType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['variadic']);
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
		createTypePart: (type, typePart, node, scope, fallback = true) => {
			typePart = this.createTypePart(type, typePart);

			this.rules[node?.type]?.(node, scope, type, typePart);

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
			let staticTypes = ['deinitializerDeclaration', 'initializerDeclaration'],
				bothTypes = ['functionDeclaration']

			for(let i = 0; i < statements.length; i++) {
				let statement = statements[i]

				if(staticTypes.includes(statement.type)) {
					continue;
				}

				if(!statement.modifiers?.includes('static')) {
					objectStatements.push(statement);

					if(bothTypes.includes(statement.type)) {
						continue;
					}

					statements.splice(i, 1);
					i--;
				}
			}
		},
		findMemberOverload: (scope, composite, identifier, matching, internal) => {
			// TODO: Access-related checks

			return this.findMemberOverload(composite, identifier, matching, internal);
		},
		getMemberOverloadSearchParameters: (node, scope) => {
			if(node.type === 'identifier') {
				return {
					composite: scope,
					identifier: node.value,
					internal: false
				}
			} else
			if(node.type === 'chainExpression') {
				return {
					composite: this.getValueComposite(this.rules[node.composite.type]?.(node.composite, scope)),
					identifier: node.member.type === 'identifier' ? node.member.value : this.rules.stringLiteral(node.member, scope, true).primitiveValue,
					internal: true
				}
			}
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

	static getComposite(address) {
		return this.composites[address]
	}

	static createComposite(title, type, scope) {
		if(!this.typeIsComposite(type)) {
			return;
		}

		let composite = {
			title: title,
			addresses: {
				ID: this.composites.length,
				super: undefined,
				Super: undefined,
				self: undefined,
				Self: undefined,
				sub: undefined,
				Sub: undefined,
				scope: undefined,
				retainers: []
			},
			alive: true,
		//	state: 0  // (0 - Create, 1 - Initialize), 2 - Idle, (3 - Deinitialize, 4 - Destroy)
			type: type,
			statements: [],
			imports: {},
			operators: {},
			members: {},
			observers: []
		}

		this.composites.push(composite);

		if(scope != null) {
			this.setScopeAddress(composite, scope);
		}
	//	this.print('cr: '+composite.title+', '+composite.addresses.ID);

		return composite;
	}

	static destroyComposite(composite) {
		if(!composite.alive) {
			return;
		}

	//	this.print('ds: '+composite.title+', '+composite.addresses.ID);

		composite.alive = false;

		if(this.typeIsComposite(composite.type, 'Object')) {
			let function_ = this.findMemberOverload(composite, 'deinit', (v) => this.findValueFunction(v.value, []), true)?.matchingValue;

			if(function_ != null) {
				this.callFunction(function_, [], composite);
			}
		}

		for(let composite_ of this.composites) {
			if(composite_?.addresses.retainers.includes(composite.addresses.ID)) {
				this.releaseComposite(composite, composite_);
			}
		}

		delete this.composites[composite.addresses.ID]

		if(composite.addresses.retainers.length > 0) {
		// TODO: Notify all retainers about destroying

			this.report(1, undefined, 'Composite #'+composite.addresses.ID+' was destroyed with a non-empty retainer list.');
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

		if(!retainedComposite.addresses.retainers.includes(retainingComposite.addresses.ID)) {
			retainedComposite.addresses.retainers.push(retainingComposite.addresses.ID);
		}
	}

	static releaseComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null || retainedComposite === retainingComposite) {
			return;
		}

		if(retainedComposite.addresses.retainers.includes(retainingComposite.addresses.ID)) {
			retainedComposite.addresses.retainers = retainedComposite.addresses.retainers.filter(v => v !== retainingComposite.addresses.ID);

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
	 * the retainer's addresses (excluding "ID" and retainers list), type, import, member or observer.
	 */
	static compositeRetains(retainingComposite, retainedComposite) {
		if(retainingComposite?.alive) return (
			this.addressesRetain(retainingComposite, retainedComposite) ||
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
	 * retainersAddresses is used to exclude a retain cycles and redundant passes
	 * from the lookup and meant to be set internally only.
	 */
	static compositeRetainsDistant(retainingComposite, retainedComposite, retainersAddresses = []) {
		if(retainedComposite == null || retainingComposite == null) {
			return;
		}
		if(retainedComposite.addresses.ID === retainingComposite.addresses.ID) {
			return true;
		}

		for(let retainerAddress of retainedComposite.addresses.retainers) {
			if(retainersAddresses.includes(retainerAddress)) {
				continue;
			}

			retainersAddresses.push(retainerAddress);

			if(this.compositeRetainsDistant(retainingComposite, this.getComposite(retainerAddress), retainersAddresses)) {
				return true;
			}
		}
	}

	/*
	 * Returns a significant state of general retainment.
	 *
	 * Composite considered significantly retained if it is formally retained by
	 * the global namespace, a current scope namespace or a control trasfer value.
	 */
	static compositeRetained(composite) {
		return (
			this.compositeRetainsDistant(this.getComposite(0), composite) ||
			this.compositeRetainsDistant(this.getScope()?.namespace, composite) ||
			this.compositeRetainsDistant(this.getValueComposite(this.controlTransfer?.value), composite)
		);
	}

	static createClass(title, scope) {
		let class_ = this.createComposite(title, [{ predefined: 'Class' }], scope);

		this.setSelfAddress(class_, class_);

		return class_;
	}

	static createEnumeration(title, scope) {
		let enumeration = this.createComposite(title, [{ predefined: 'Enumeration' }], scope);

		this.setSelfAddress(enumeration, enumeration);

		return enumeration;
	}

	static createFunction(title, statements, scope) {
		let function_ = this.createComposite(title, [{ predefined: 'Function' }], scope);

		this.setInheritedLevelAddresses(function_, scope);
		this.setStatements(function_, statements);

		return function_;
	}

	static createNamespace(title, scope) {
		let namespace = this.createComposite(title, [{ predefined: 'Namespace' }], scope);

		this.setSelfAddress(namespace, namespace);

		return namespace;
	}

	static createObject(superObject, selfComposite, subObject) {
		let title = 'Object<'+(selfComposite.title ?? '#'+selfComposite.addresses.ID)+'>',
			type = [{ predefined: 'Object', inheritedTypes: true }, { super: 0, reference: selfComposite.addresses.ID }],
			object = this.createComposite(title, type, selfComposite);

		if(superObject != null) {
			this.setSuperAddress(object, superObject);
		}

		this.setSelfAddress(object, object);

		if(subObject != null) {
			this.setSubAddress(object, subObject);
		}

		return object;
	}

	static createProtocol(title, scope) {
		let protocol = this.createComposite(title, [{ predefined: 'Protocol' }], scope);

		this.setSelfAddress(protocol, protocol);

		return protocol;
	}

	static createStructure(title, scope) {
		let structure = this.createComposite(title, [{ predefined: 'Structure' }], scope);

		this.setSelfAddress(structure, structure);

		return structure;
	}

	static getScope(offset = 0) {
		return this.scopes[this.scopes.length-1+offset]
	}

	/*
	 * Should be used for a bodies before executing its contents.
	 *
	 * ARC utilize a current scope to be aware of retainments by the execution state
	 * along with a control transfer value.
	 *
	 * Additionally function and its call location can be specified for debugging purposes.
	 */
	static addScope(namespace, function_, location) {
		this.scopes.push({
			namespace: namespace,
			function: function_,
			location: location
		});
	}

	/*
	 * Removes from the stack and optionally automatically destroys its last scope.
	 */
	static removeScope(destroy = true) {
		let namespace = this.scopes.pop()?.namespace;

		if(destroy) {
			this.destroyReleasedComposite(namespace);
		}
	}

	static getCallsString() {
		let result = '';

		for(let i = this.scopes.length-1, j = 0; i >= 0 && j < 8; i--) {
			let scope = this.scopes[i],
				function_ = scope.function,
				location = scope.location;

			if(function_ == null) {
				continue;
			}

			if(j > 0) {
				result += '\n';
			}

			result += j+': ';

			let composite = this.getComposite(function_.addresses.Self);

			if(function_.title != null && composite != null) {
				result += (composite.title ?? '#'+composite.addresses.ID)+'.';
			}

			result += function_.title ?? '#'+function_.addresses.ID;

			if(location != null) {
				result += ':'+(location.line+1)+':'+(location.column+1);
			}

			j++;
		}

		return result;
	}

	/*
	 * Should be used for a return values before releasing its scopes.
	 *
	 * ARC utilize a control trasfer value to be aware of retainments by the execution state
	 * along with a current scope.
	 */
	static setControlTransfer(value, type) {
		this.controlTransfer = {
			value: value,
			type: type
		}
	}

	static resetControlTransfer() {
		this.controlTransfer = undefined;
	}

	/*
	 * Last statement in a body will be treated like a returning one even if it's not an explicit return.
	 * Manual control transfer and additional control transfer types is supported but should be also implemented by rules.
	 */
	static executeStatements(nodes, scope, additionalCTT = []) {
		let globalScope = this.getScope(-1) == null,
			CTT = [  // Control transfer types
			//	'breakStatement',
			//	'continueStatement',
			//	'fallthroughStatement',
				'returnStatement',
				'throwStatement',
				...additionalCTT
			]

		for(let node of nodes ?? []) {
			let start = this.composites.length,
				value = this.rules[node.type]?.(node, scope),
				end = this.composites.length,
				CTed = this.controlTransfer != null,
				explicitlyCTed = CTed && CTT.includes(this.controlTransfer.type),
				valueCTed = CTed && this.controlTransfer.value === value,
				CTing = CTT.includes(node.type) || node === nodes.at(-1),
				valueCTing = !explicitlyCTed && !valueCTed && CTing;

			if(valueCTing) {
				this.setControlTransfer(!globalScope ? value : undefined, node.type);
			}

			for(start; start < end; start++) {
				this.destroyReleasedComposite(this.getComposite(start));
			}

			if(explicitlyCTed || CTing) {
				break;
			}
		}
	}

	/*
	 * Levels such as super, self or sub, can be inherited from another composite.
	 * If that composite is an object, it will be initialized with statements stored by its Self.
	 *
	 * Specifying a scope means intent to execute function's statements directly into that one fellow composite.
	 *
	 * If no levels or scope is specified, original function ones is used.
	 */
	static callFunction(function_, arguments_ = [], levels, scope, caller, location) {
		if(typeof function_.statements === 'function') {
			return function_.statements(arguments_);
		}

		let namespaceTitle = 'Call<'+(function_.title ?? '#'+function_.addresses.ID)+'>',
			namespace = scope ?? this.createNamespace(namespaceTitle, function_),
			parameters = this.getTypeFunctionParameters(function_.type);

		this.setInheritedLevelAddresses(namespace, levels ?? function_);

		if(caller != null) {
			this.setMemberOverload(namespace, 'caller', [], [{ predefined: 'Any', nillable: true }], this.createValue('reference', caller.addresses.ID), []);
		}

		for(let i = 0; i < arguments_.length; i++) {
			let argument = arguments_[i],
				parameterType = this.getSubtype(parameters, parameters[i]),
				identifier = parameterType[0]?.identifier ?? '$'+i;

			this.setMemberOverload(namespace, identifier, [], parameterType, argument.value, []);
		}

		this.addScope(namespace, function_, location);

		if(this.typeIsComposite(levels?.type, 'Object')) {
			let selfComposite = this.getComposite(levels.addresses.Self);

			this.executeStatements(selfComposite?.statements, levels);
		}

		this.executeStatements(function_.statements, namespace);
		this.removeScope();

		let returnValue = this.controlTransfer?.value;

		// TODO: Return value type checking

		this.resetControlTransfer();

		return returnValue;
	}

	static setCompositeType(composite, type) {
		if(!this.typeIsComposite(type)) {
			return;
		}

		composite.type = type;
	}

	static setAddress(composite, key, value) {
		if(['ID', 'retainers'].includes(key)) {
			return;
		}

		let ov, nv;  // Old/new value

		ov = composite.addresses[key]
		nv = composite.addresses[key] = value;

		if(key.toLowerCase() !== 'self') {  // Address chains should not be cyclic
			let addressedComposites = [],
				addressedComposite = composite;

			while(addressedComposite != null) {
				if(addressedComposites.includes(addressedComposite)) {
					composite.addresses[key] = ov;

					return;
				}

				addressedComposites.push(addressedComposite);

				addressedComposite = this.getComposite(addressedComposite.addresses[key]);
			}
		}

		if(ov !== nv) {
			this.retainOrReleaseComposite(composite, this.getComposite(ov));
			this.retainComposite(composite, this.getComposite(nv));
		}
	}

	static setSuperAddress(composite, superComposite) {
		this.setAddress(composite, 'super', superComposite.addresses.ID);
		this.setAddress(composite, 'Super', !this.typeIsComposite(superComposite.type, 'Object') ? superComposite.addresses.ID : this.getTypeInheritedAddress(superComposite.type));
	}

	static setSelfAddress(composite, selfComposite) {
		this.setAddress(composite, 'self', selfComposite.addresses.ID);
		this.setAddress(composite, 'Self', !this.typeIsComposite(selfComposite.type, 'Object') ? selfComposite.addresses.ID : this.getTypeInheritedAddress(selfComposite.type));
	}

	static setSubAddress(object, subObject) {
		if(!this.typeIsComposite(object.type, 'Object')) {
			return;
		}

		if(this.typeIsComposite(subObject.type, 'Object')) {
			this.setAddress(object, 'sub', subObject.addresses.ID);
			this.setAddress(object, 'Sub', this.getTypeInheritedAddress(subObject.type));
		}
	}

	static setInheritedLevelAddresses(inheritingComposite, inheritedComposite) {
		if(inheritedComposite === inheritingComposite) {
			return;
		}

		let excluded = ['ID', 'scope', 'retainers']

		for(let key in inheritingComposite.addresses) {
			if(!excluded.includes(key)) {
				this.setAddress(inheritingComposite, key, inheritedComposite?.addresses[key]);
			}
		}
	}

	static setScopeAddress(composite, scopeComposite) {
		this.setAddress(composite, 'scope', scopeComposite?.addresses.ID);
	}

	static addressesRetain(retainingComposite, retainedComposite) {
		let excluded = ['ID', 'retainers']

		for(let key in retainingComposite.addresses) {
			if(!excluded.includes(key) && retainingComposite.addresses[key] === retainedComposite.addresses.ID) {
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

	static getTypeInheritedAddress(type) {
		let index = type.findIndex(v => v.inheritedTypes);

		if(index === -1) {
			return;
		}

		return type.find(v => v.super === index)?.reference;
	}

	static getTypeGenericParameters(type) {
		return this.getSubtype(type, type.find(v => v.genericParameters)).slice(1);
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
		return retainingType.some(v => v.reference === retainedComposite.addresses.ID);
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
				if(!this.typeIsComposite(composite.type, 'Object')) {
					if(composite.addresses.ID === value.primitiveValue) {
						part.self = true;
					} else {
						part.reference = composite.addresses.ID;
					}

					composite = undefined;
				} else {
					composite = this.getComposite(composite.addresses.scope);
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

		return value != null ? JSON.stringify(value) : 'nil';
	}

	static getValueFunction(value, arguments_) {
		let function_ = this.getValueComposite(value);

		if(!this.typeIsComposite(function_?.type, 'Function')) {
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
			for(let key in value.primitiveValue) {
				if((function_ = this./*find*/getValueFunction(value.primitiveValue[key], arguments_)) != null) {
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
			for(let key in retainingValue.primitiveValue) {
				this.retainOrReleaseValueComposites(retainingComposite, retainingValue.primitiveValue[key]);
			}
		} else {
			this.retainOrReleaseComposite(retainingComposite, this.getValueComposite(retainingValue));
		}
	}

	static valueRetains(retainingValue, retainedComposite) {
		if(retainingValue?.primitiveType === 'dictionary') {
			for(let key in retainingValue.primitiveValue) {
				let retainingValue_ = retainingValue.primitiveValue[key]

				if(this.valueRetains(retainingValue_, retainedComposite)) {
					return true;
				}
			}
		} else {
			return this.getValueComposite(retainingValue) === retainedComposite;
		}
	}

	static setStatements(callableComposite, statements) {
		if(
			!this.typeIsComposite(callableComposite.type, 'Class') &&
			!this.typeIsComposite(callableComposite.type, 'Function') &&
			!this.typeIsComposite(callableComposite.type, 'Structure')
		) {
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

			composite = this.getComposite(composite.addresses.scope);
		}
	}

	static setImport(namespace, identifier, value) {
		if(!this.typeIsComposite(namespace.type, 'Namespace')) {
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
			if(retainingComposite.imports[identifier] === retainedComposite.addresses.ID) {
				return true;
			}
		}
	}

	static getOperator(composite, identifier) {
		return composite.operators[identifier]
	}

	static addOperator(namespace, identifier) {
		if(!this.typeIsComposite(namespace.type, 'Namespace')) {
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

			composite = this.getComposite(composite.addresses.scope);
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

	static getMemberOverload(composite, identifier, matching) {
		let member = this.getMember(composite, identifier);

		if(member == null) {  // Try to substitute pseudovariables
			let address = {
			//	global: () => undefined,				    Global-object is no thing
				Global: () => 0,						 // Global-type
				super: () => composite.addresses.super,  // Super-object or a type
				Super: () => composite.addresses.Super,  // Super-type
				self: () => composite.addresses.self,	 // Self-object or a type
				Self: () => composite.addresses.Self,	 // Self-type
				sub: () => composite.addresses.sub,		 // Sub-object
				Sub: () => composite.addresses.Sub		 // Sub-type
			//	metaSelf: () => undefined,				    Self-object or a type (descriptor)
			//	arguments: () => undefined				    Function arguments array (needed?)
			}[identifier]?.();

			if(address == null) {
				return;
			}

			member = [
				{
					modifiers: [],
					type: [{ predefined: 'Any' }],
					value: this.createValue('reference', address),
					observers: []
				}
			]
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

			composite = this.getComposite(composite.addresses.scope);
		}
	}

	/*
	 * Looking for member overload in a function or namespace.
	 *
	 * This types are slightly different from other. They inherit levels and eventually aren't
	 * instantiable nor inheritable themself. But they are still used as members storage
	 * and can participate in plain scope chains.
	 */
	static findMemberOverloadInComposite(composite, identifier, matching) {
		if(
			!this.typeIsComposite(composite.type, 'Function') &&
			!this.typeIsComposite(composite.type, 'Namespace')
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
		if(!this.typeIsComposite(object.type, 'Object')) {
			let object_ = this.getComposite(object.addresses.self);

			if(object_ === object || !this.typeIsComposite(object_?.type, 'Object')) {
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

					while((object_ = this.getComposite(object.addresses.sub)) != null) {  // Lowest
						object = object_;
					}

					continue;
				}

				return this.getMemberOverloadProxy(overload, { owner: object });
			}

			object = this.getComposite(object.addresses.super);
		}
	}

	/*
	 * Looking for member overload in Inheritance chain (Super, Self).
	 */
	static findMemberOverloadInInheritanceChain(composite, identifier, matching) {
		if(this.typeIsComposite(composite.type, 'Object')) {
			composite = this.getComposite(composite.addresses.Self);
		}

		while(composite != null) {
			let overload = this.getMemberOverload(composite, identifier, matching);

			if(overload != null) {
				return this.getMemberOverloadProxy(overload, { owner: composite });
			}

			composite = this.getComposite(composite.addresses.Super);
		}
	}

	/*
	 * Not specifying "internal" means use of direct declaration without search.
	 */
	static setMemberOverload(composite, identifier, modifiers, type, value, observers, matching, internal) {
		let overload = internal == null ? this.getMemberOverload(composite, identifier, matching) : this.findMemberOverload(composite, identifier, matching, internal);

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

		let ot = overload.type ?? [],  // Old/new type
			nt = type ?? [],
			ov = overload.value,  // Old/new value
			nv = value;

		overload.modifiers = modifiers;
		overload.type = type;
		overload.value = value;
		overload.observers = observers;

		if(ot !== nt) {
			let addresses = new Set([...ot, ...nt].map(v => v.reference));

			for(let address of addresses) {
				this.retainOrReleaseComposite(composite, this.getComposite(address));
			}
		}
		if(ov !== nv) {
			this.retainOrReleaseValueComposites(composite, ov);
			this.retainOrReleaseValueComposites(composite, nv);
		}
	}

	static membersRetain(retainingComposite, retainedComposite) {
		for(let identifier in retainingComposite.members) {
			for(let overload of retainingComposite.members[identifier] ?? []) {
				if(
					this.typeRetains(overload.type, retainedComposite) ||
					this.valueRetains(overload.value, retainedComposite) ||
					overload.observers.some(v => v.value === retainedComposite.addresses.ID)
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
		return retainingComposite.observers.some(v => v.value === retainedComposite.addresses.ID);
	}

	static report(level, node, string) {
		let position = node?.range?.start ?? 0,
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
		this.composites = composites ?? []
		this.scopes = []
		this.controlTransfer = undefined;
		this.preferences = {
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