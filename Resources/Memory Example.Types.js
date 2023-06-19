// Rules for a type parts:
// - One TP can have only one collection flag.
// - Collection TPs can have children and should adopt adjoining collections.
// - Flags that are not mutually exclusive can be combined.
// - Flags can not be ambigously combined (variadic array type / array of variadic type).
// - TPs should be minimalistic and can not be redundant.

//    inout A! | _? & Global<B>...
// 0: inout    |               ...
// 1:       A!
// 2:               &
// 3:            _?
// 4:                 Global< >
// 5:                        B

let memberType0 = [
	{
		inout: true,
		union: true,
		variadic: true
	},
	{
		super: 0,
		reference: 'A',  // Composite's address
		default: true
	},
	{
		super: 0,
		intersection: true
	},
	{
		super: 2,
		predefined: '_',
		nillable: true
	},
	{
		super: 2,
		reference: 'Global',
		genericArguments: true
	},
	{
		super: 4,
		reference: 'B'
	}
]

//    (A | B) & C
// 0:         &
// 1: (  |  )
// 2:  A
// 3:      B
// 4:           C

let memberType1 = [
	{
		intersection: true
	},
	{
		super: 0,
		union: true
	},
	{
		super: 1,
		reference: 'A'
	},
	{
		super: 1,
		reference: 'B'
	},
	{
		super: 0,
		reference: 'C'
	}
]

//    () / Function
// 0: () / Function

let memberType2 = [
	{
		predefined: 'Function'
	}
]

//    <...>(...) awaits? throws? -> _?
// 0:            awaits? throws?
// 1: <   >
// 2:  ...
// 3:      (   )
// 4:       ...
// 5:                            -> _?

let memberType3 = [
	{
		predefined: 'Function',
		awaits: undefined,
		throws: undefined
	},
	{
		super: 0,
		genericParameters: true
	},
	{
		super: 1,
		variadic: true
	},
	{
		super: 0,
		parameters: true
	},
	{
		super: 3,
		variadic: true
	},
	{
		super: 0,
		predefined: '_',
		nillable: true
	}
]

//    ([]..., ...) awaits throws -> _
// 0:              awaits throws
// 1: (     ,    )
// 2:  []...
// 3:         ...
// 4:                            -> _

let memberType3 = [
	{
		predefined: 'Function',
		awaits: true,
		throws: true
	},
	{
		super: 0,
		parameters: true
	},
	{
		super: 1,
		array: true,
		variadic: true
	},
	{
		super: 1,
		variadic: true
	},
	{
		super: 0,
		predefined: '_'
	}
]

//    (() -> _)?
// 0: (()     )?
// 1:     -> _

let memberType4 = [
	{
		predefined: 'Function',
		nillable: true
	},
	{
		super: 0,
		predefined: '_'
	}
]

//    <T>()
// 0:    ()
// 1: < >
// 2:  T

let memberType5 = [
	{
		predefined: 'Function'
	},
	{
		super: 0,
		genericParameters: true
	},
	{
		super: 1,
		predefined: '_',
		nillable: true
	}
]

//    [] / [_?] / Array<_?>
// 0: [] / [  ] / Array<  >
// 1:    /  _?  /       _?

let memberType6 = [  // Fallback
	{
		array: true
	},
	{
		super: 0,
		predefined: '_',
		nillable: true
	}
]

let memberType7 = [  // Preferable and default
	{
		reference: 'Array',
		genericArguments: true
	},
	{
		super: 0,
		predefined: '_',
		nillable: true
	}
]

//    (a: A, b c: B)
// 0: (    ,       )
// 1:  a: A
// 2:        b c: B

let tupleType0 = [
	{
		tuple: true
	},
	{
		super: 0,
		identifier: 'a',
		reference: 'A'
	},
	{
		super: 0,
		label: 'b',
		identifier: 'c',
		reference: 'B'
	}
]

//    class A: B<_?>
// 0: class A:
// 1:          B<  >
// 2:            _?

let compositeType0 = [
	{
		predefined: 'Class',
		identifier: 'A',
		inheritedTypes: true
	},
	{
		super: 0,
		reference: 'B',
		genericArguments: true
	},
	{
		super: 1,
		predefined: '_',
		nillable: true
	}
]