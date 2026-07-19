'use strict';
/**
 * core/typechecker.js — PlantLang Static Type Checker
 *
 * Walks the AST produced by core/parser.js and collects type errors
 * WITHOUT executing anything. Returns a list of Diagnostic objects.
 *
 * Usage:
 *   const { typecheck } = require('./core/typechecker');
 *   const { parse }     = require('./core/parser');
 *   const prog = parse(source);
 *   const diags = typecheck(prog, source);
 *   if (diags.length) diags.forEach(d => console.error(d.format()));
 */

// ── Type constants ──────────────────────────────────────────────────────────
const T = {
  NUM:      'NUM',
  SCL:      'SCL',
  TX:       'TX',
  FACT:     'FACT',
  LIST:     'LIST',
  MAP:      'MAP',
  INSTANCE: 'INSTANCE',
  VEIN:     'VEIN',
  VOID:     'VOID',
  UNKNOWN:  'UNKNOWN',   // could not be determined statically
  ANY:      'ANY',       // polymorphic / intentionally untyped
};

// Types that support arithmetic
const NUMERIC = new Set([T.NUM, T.SCL]);

  // Types that support + (concatenation)
const ADDABLE = new Set([T.NUM, T.SCL, T.TX, T.UNKNOWN, T.ANY]);

// Helper: extract inner type from array type string "[NUM]" → "NUM"
function arrayInnerType(typeStr) {
  if (typeof typeStr === 'string' && typeStr.startsWith('[') && typeStr.endsWith(']')) {
    return typeStr.slice(1, -1);
  }
  return null;
}

function isArrayType(typeStr) {
  return typeof typeStr === 'string' && typeStr.startsWith('[') && typeStr.endsWith(']') && typeStr.length >= 3;
}

// Helper: extract key/value types from MAP[K,V] → { keyType, valueType }
function isMapType(typeStr) {
  return typeof typeStr === 'string' && typeStr.startsWith('MAP[') && typeStr.endsWith(']');
}
function mapInnerTypes(typeStr) {
  if (!isMapType(typeStr)) return null;
  const inner = typeStr.slice(4, -1); // MAP[K,V] → K,V
  const parts = inner.split(',');
  if (parts.length !== 2) return null;
  return { keyType: parts[0].trim(), valueType: parts[1].trim() };
}
function mapValueType(typeStr) {
  const m = mapInnerTypes(typeStr);
  return m ? m.valueType : null;
}

// ── Diagnostic ──────────────────────────────────────────────────────────────
class Diagnostic {
  /**
   * @param {'error'|'warning'|'info'} severity
   * @param {string} code      short identifier e.g. "TYPE_MISMATCH"
   * @param {string} message   human-readable description
   * @param {number} line
   * @param {number} column
   */
  constructor(severity, code, message, line, column) {
    this.severity = severity;
    this.code     = code;
    this.message  = message;
    this.line     = line   || 0;
    this.column   = column || 0;
  }

  format() {
    const icon = this.severity === 'error' ? '✕' : this.severity === 'warning' ? '⚠' : 'ℹ';
    return `${icon} [${this.code}] ${this.message}  (line ${this.line}, col ${this.column})`;
  }
}

// ── TypeScope ────────────────────────────────────────────────────────────────
// Lightweight scope chain: maps identifier → { type, locked, line }
class TypeScope {
  constructor(parent = null) {
    this.parent  = parent;
    this._vars   = new Map();
    this._fns    = new Map();   // ACTION definitions
    this._species = new Map();  // SPECIES definitions
    this._structs = new Map();  // SHAPE definitions
    this._methods = new Map();  // type name -> Map(method name -> action info)
    this._choices = new Map();  // CHOICE name -> [{ name, type }]
  }

  child() { return new TypeScope(this); }

  // Variables
  setVar(name, type, opts = {}) {
    this._vars.set(name, { type, locked: opts.locked || false, line: opts.line || 0 });
  }

  getVar(name) {
    if (this._vars.has(name)) return this._vars.get(name);
    return this.parent ? this.parent.getVar(name) : null;
  }

  // Actions / functions
  setFn(name, { params, returnType }) {
    this._fns.set(name, { params, returnType: returnType || T.UNKNOWN });
  }

  getFn(name) {
    if (this._fns.has(name)) return this._fns.get(name);
    return this.parent ? this.parent.getFn(name) : null;
  }

  // Structs (SHAPE)
  setStruct(name, fields) {
    this._structs.set(name, fields);
  }

  getStruct(name) {
    if (this._structs.has(name)) return this._structs.get(name);
    return this.parent ? this.parent.getStruct(name) : null;
  }

  // Species / classes
  setSpecies(name, { fields, methods }) {
    this._species.set(name, { fields, methods });
  }

  getSpecies(name) {
    if (this._species.has(name)) return this._species.get(name);
    return this.parent ? this.parent.getSpecies(name) : null;
  }

  // Methods (receiver-bound actions)
  setMethod(typeName, methodName, info) {
    if (!this._methods.has(typeName)) this._methods.set(typeName, new Map());
    this._methods.get(typeName).set(methodName, info);
  }

  getMethod(typeName, methodName) {
    if (this._methods.has(typeName) && this._methods.get(typeName).has(methodName)) {
      return this._methods.get(typeName).get(methodName);
    }
    return this.parent ? this.parent.getMethod(typeName, methodName) : null;
  }

  hasMethodsFor(typeName) {
    if (this._methods.has(typeName) && this._methods.get(typeName).size > 0) return true;
    return this.parent ? this.parent.hasMethodsFor(typeName) : false;
  }

  // Choices / tagged unions (CHOICE)
  setChoice(name, variants) {
    this._choices.set(name, variants);
  }

  getChoice(name) {
    if (this._choices.has(name)) return this._choices.get(name);
    return this.parent ? this.parent.getChoice(name) : null;
  }
}

// ── Type inference from literal/expression strings ───────────────────────────
function inferLiteralType(node) {
  if (!node) return T.UNKNOWN;

  // Typed AST literals
  if (node.type === 'Literal') {
    if (node.literalType === 'NUMBER')  return T.NUM;
    if (node.literalType === 'STRING')  return T.TX;
    if (node.literalType === 'BOOLEAN') return T.FACT;
    if (node.literalType === 'RAW_EXPR') return T.UNKNOWN; // deferred
    return T.UNKNOWN;
  }

  if (node.type === 'Identifier') return T.UNKNOWN; // resolved at runtime

  return T.UNKNOWN;
}

// Heuristic: infer type from a raw expression string (best-effort)
function inferExprString(expr, scope) {
  if (!expr || typeof expr !== 'string') return T.UNKNOWN;
  const s = expr.trim();

  if (s === 'TRUE' || s === 'FALSE') return T.FACT;
  if (/^-?[0-9]+$/.test(s))         return T.NUM;
  if (/^-?[0-9]+\.[0-9]+$/.test(s)) return T.SCL;
  if (/^"[^"]*"$/.test(s))          return T.TX;

  // Single identifier → look up scope
  if (/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(s)) {
    const v = scope.getVar(s);
    return v ? v.type : T.UNKNOWN;
  }

  // map:"key" or map:prop access
  if (/^[a-zA-Z_]\w*:"[^"]*"$/.test(s) || /^[a-zA-Z_]\w*:\w+$/.test(s)) {
    return T.UNKNOWN; // MAP value type not statically known
  }

  // String concat: anything containing " + " with a TX operand → TX
  if (s.includes(' + ') && s.includes('"')) return T.TX;

  // Array literal [...] — return UNKNOWN (too complex for static inference)
  if (s.startsWith('[') && s.endsWith(']')) return T.LIST;

  // math:SQRT etc. → SCL
  if (/^math:\w+/.test(s)) return T.SCL;
  if (/^strings:\w+/.test(s)) return T.TX;
  if (/^lists:\w+/.test(s)) return T.UNKNOWN;

  return T.UNKNOWN;
}

// Infer type from a CreateStatement valueExpr
function inferCreateType(varType, valueExpr, scope) {
  if (varType && varType !== T.UNKNOWN) return varType;
  if (!valueExpr) return T.UNKNOWN;
  if (typeof valueExpr === 'string') return inferExprString(valueExpr, scope);
  return inferLiteralType(valueExpr);
}

// ── Built-in library return types ─────────────────────────────────────────────
const INNATE_RETURN = {
  'math:SQRT':   T.SCL, 'math:ABS':   T.NUM, 'math:ROUND': T.SCL,
  'math:FLOOR':  T.NUM, 'math:CEIL':  T.NUM, 'math:POW':   T.NUM,
  'math:LOG':    T.SCL, 'math:SIN':   T.SCL, 'math:COS':   T.SCL,
  'strings:TRIM':     T.TX, 'strings:UPPER':    T.TX, 'strings:LOWER': T.TX,
  'strings:LENGTH':   T.NUM, 'strings:REPLACE': T.TX, 'strings:CONCAT': T.TX,
  'strings:SPLIT':    T.LIST, 'strings:INCLUDES': T.TX,
  'lists:AVERAGE': T.SCL, 'lists:MEDIAN': T.SCL, 'lists:UNIQUE': T.LIST,
  'lists:FLATTEN': T.LIST, 'lists:CHUNK':  T.LIST, 'lists:RANGE':  T.LIST,
  'lists:SORT':   T.LIST, 'lists:REVERSE': T.LIST,
};

// ── Numeric aggregates on LIST ────────────────────────────────────────────────
const LIST_AGGREGATES = new Set(['SUM','MAX','MIN','COUNT','FIRST','LAST','REVERSE']);

// ── Main typechecker ──────────────────────────────────────────────────────────
class TypeChecker {
  constructor(source) {
    this.source = source || '';
    this.diags  = [];
    this.rootScope = new TypeScope();
    this._preloadBuiltins();
  }

  _preloadBuiltins() {
    // Built-in helpers with polymorphic (ANY) parameter types
    const anyParam = (name) => ({ name, type: T.ANY });
    this.rootScope.setFn('assert', {
      params: [anyParam('label'), anyParam('actual'), anyParam('expected')],
      returnType: T.VOID
    });
    // Register known library return types
    // (loaded dynamically via PLANT, but pre-declare for type inference)
  }

  error(code, message, line, column) {
    this.diags.push(new Diagnostic('error', code, message, line, column));
  }

  warn(code, message, line, column) {
    this.diags.push(new Diagnostic('warning', code, message, line, column));
  }

  info(code, message, line, column) {
    this.diags.push(new Diagnostic('info', code, message, line, column));
  }

  // ── Entry point ────────────────────────────────────────────────────────────
  check(programNode) {
    // Pass 1: register top-level declarations (ACTIONs, SPECIES, ROOT, PLANT)
    this._pass1(programNode.statements, this.rootScope);
    // Pass 2: check all statements
    this._checkBlock(programNode.statements, this.rootScope);
    return this.diags;
  }

  // ── Pass 1: hoist declarations ──────────────────────────────────────────────
  _pass1(stmts, scope) {
    for (const node of (stmts || [])) {
      if (!node) continue;
      if (node.type === 'ActionDeclaration') {
        this._registerAction(node, scope);
      } else if (node.type === 'SpeciesDeclaration') {
        this._registerSpecies(node, scope);
      } else if (node.type === 'RootStatement') {
        // ROOT NAME TO value — lock it
        scope.setVar(node.identifier, T.UNKNOWN, { locked: true, line: node.line });
      } else if (node.type === 'MissionStatement') {
        // no-op
      } else if (node.type === 'PlantStatement') {
        // Library loaded — mark as known
        scope.setVar(`__plant_${node.libName}`, T.FACT);
      } else if (node.type === 'RootScopeStatement') {
        scope.setVar(node.identifier, T.MAP, { locked: true, line: node.line });
      } else if (node.type === 'StructDeclaration') {
        scope.setStruct(node.name, node.fields);
      }
    }
  }

  _registerAction(node, scope) {
    let params = (node.params || []).map(p => ({ name: p.name, type: p.type || T.UNKNOWN }));
    // If ALL params are TX, mark them as ANY — common pattern for polymorphic helpers
    // like `assert(label(TX), actual(TX), expected(TX))` which accept any value type
    const allTX = params.length > 0 && params.every(p => p.type === T.TX);
    if (allTX && !node.receiver) params = params.map(p => ({ ...p, type: T.ANY }));
    // If this is a receiver-bound method, register as method instead
    if (node.receiver) {
      const returnType = this._inferActionReturn(node.body || node.bodyStatements || [], params, scope);
      scope.setMethod(node.receiver.type, node.name, { params, returnType, receiverName: node.receiver.name });
      return;
    }
    // Infer return type from body GiveStatements
    const returnType = this._inferActionReturn(node.body || node.bodyStatements || [], params, scope);
    scope.setFn(node.name, { params, returnType });
  }

  _inferActionReturn(body, params, parentScope) {
    if (!body || body.length === 0) return T.NUM; // external FFI defaults to NUM (i64)
    const fnScope = parentScope.child();
    for (const p of params) fnScope.setVar(p.name, p.type || T.UNKNOWN);
    let rt = T.VOID;
    for (const n of (body || [])) {
      if (n && n.type === 'GiveStatement') {
        rt = this._inferExprNode(n.valueExpr, fnScope);
        break;
      }
    }
    return rt;
  }

  _registerSpecies(node, scope) {
    const fields  = {};
    const methods = {};
    for (const m of (node.members || [])) {
      if (!m) continue;
      if (m.type === 'VarDeclaration') {
        fields[m.identifier] = m.varType || T.UNKNOWN;
      } else if (m.type === 'ActionDeclaration') {
        this._registerAction(m, scope); // also into parent scope
        methods[m.name] = scope.getFn(m.name);
      }
    }
    scope.setSpecies(node.name, { fields, methods });
  }

  // ── Block checker ───────────────────────────────────────────────────────────
  _checkBlock(stmts, scope) {
    for (const node of (stmts || [])) {
      if (!node) continue;
      this._checkNode(node, scope);
    }
  }

  _checkNode(node, scope) {
    if (!node || !node.type) return;
    switch (node.type) {
      case 'CreateStatement':    this._checkCreate(node, scope);    break;
      case 'MethodCall':         this._checkMethodCall(node, scope); break;
      case 'SetStatement':       this._checkSet(node, scope);       break;
      case 'IncreaseStatement':
      case 'DecreaseStatement':  this._checkArithmetic(node, scope);break;
      case 'ShowStatement':      this._checkShow(node, scope);      break;
      case 'ReapStatement':      this._checkReap(node, scope);      break;
      case 'IfStatement':        this._checkIf(node, scope);        break;
      case 'CycleStatement':     this._checkCycle(node, scope);     break;
      case 'SeasonStatement':    this._checkSeason(node, scope);    break;
      case 'MatchStatement':     this._checkMatch(node, scope);     break;
      case 'GiveStatement':      /* valid inside ACTION — checked via parent */ break;
      case 'ActionDeclaration':  this._checkActionBody(node, scope);break;
      case 'SpeciesDeclaration': this._checkSpeciesBody(node, scope);break;
      case 'BloomStatement':     this._checkBloom(node, scope);     break;
      case 'WeatherStatement':   this._checkWeather(node, scope);   break;
      case 'PutStatement':       this._checkPut(node, scope);       break;
      case 'TakeStatement':      this._checkTake(node, scope);      break;
      case 'BraidStatement':     this._checkBraid(node, scope);     break;
      case 'HarvestStatement':   this._checkHarvest(node, scope);   break;
      case 'VerifyStatement':    this._checkVerify(node, scope);    break;
      case 'SuiteStatement':     this._checkSuite(node, scope);     break;
      case 'LockStatement':      this._checkLock(node, scope);      break;
      case 'EvaporateStatement': this._checkEvaporate(node, scope); break;
      case 'StopIfStatement':    this._checkStopIf(node, scope);    break;
      // Pass-through (no type errors possible)
      case 'ImportStatement':
      case 'MissionStatement':
      case 'PlantStatement':
      case 'RootStatement':
      case 'RootScopeStatement':
      case 'StructDeclaration':
      case 'LinkStatement':
      case 'SortStatement':
      case 'ShakeStatement':
      case 'AnalyzeStatement':
      case 'WaitStatement':
      case 'WheneverStatement':
      case 'TapStatement':
      case 'InfuseStatement':
      case 'AbsorbStatement':
      case 'SealStatement':
      case 'ShowVerifySummary':
      case 'ListenBranchStatement':
      case 'ResponseStatement':
      case 'RawStatement':       break;
      case 'VariantDeclaration': this._checkVariantDeclaration(node, scope); break;
      default:                   break;
    }
  }

  // ── Statement checkers ──────────────────────────────────────────────────────

  _checkCreate(node, scope) {
    const declaredType = node.varType;
    const inferredType = this._inferExprNode(node.valueExpr, scope);

    // Handle array types: [NUM], [TX], [Point], etc.
    if (isArrayType(declaredType)) {
      const innerType = arrayInnerType(declaredType);
      scope.setVar(node.identifier, declaredType, { line: node.line });
      const ve = node.valueExpr;
      if (ve && ve.type === 'ArrayLiteral') {
        for (let i = 0; i < ve.elements.length; i++) {
          const elType = this._inferExprNode(ve.elements[i], scope);
          if (elType !== T.UNKNOWN && elType !== T.ANY && !this._compatible(innerType, elType)) {
            this.error('TYPE_MISMATCH',
              `Array element ${i}: expected ${innerType}, got ${elType}`,
              ve.elements[i].line, ve.elements[i].column);
          }
        }
      }
      return;
    }

    // Handle MAP[K,V] types
    if (isMapType(declaredType)) {
      scope.setVar(node.identifier, declaredType, { line: node.line });
      const mt = mapInnerTypes(declaredType);
      // Validate map literal if present
      const ve = node.valueExpr;
      if (ve && ve.type === 'MapLiteral') {
        for (let i = 0; i < ve.entries.length; i++) {
          const entry = ve.entries[i];
          const keyType = this._inferExprNode(entry.key, scope);
          const valType = this._inferExprNode(entry.value, scope);
          if (keyType !== T.UNKNOWN && keyType !== T.ANY && !this._compatible(mt.keyType, keyType)) {
            this.error('TYPE_MISMATCH',
              `Map entry ${i}: expected key type ${mt.keyType}, got ${keyType}`,
              entry.key.line, entry.key.column);
          }
          if (valType !== T.UNKNOWN && valType !== T.ANY && !this._compatible(mt.valueType, valType)) {
            this.error('TYPE_MISMATCH',
              `Map entry ${i}: expected value type ${mt.valueType}, got ${valType}`,
              entry.value.line, entry.value.column);
          }
        }
      }
      return;
    }

    // Check if this is a struct type
    const structDef = scope.getStruct(declaredType);
    if (structDef) {
      // Register as a struct instance variable
      scope.setVar(node.identifier, declaredType, { line: node.line });
      // Validate struct instantiation
      if (node.valueExpr && (node.valueExpr.type === 'StructInstantiation' || node.valueExpr.structName)) {
        const inst = node.valueExpr;
        if (inst.structName !== declaredType) {
          this.error('TYPE_MISMATCH',
            `CREATE "${node.identifier}": declared struct type ${declaredType} but instantiated ${inst.structName}`,
            node.line, node.column);
          return;
        }
        if (inst.args.length !== structDef.length) {
          this.error('ARITY_MISMATCH',
            `CREATE "${node.identifier}": struct ${declaredType} expects ${structDef.length} field(s), got ${inst.args.length}`,
            node.line, node.column);
          return;
        }
        for (let i = 0; i < inst.args.length; i++) {
          const argType = this._inferExprNode(inst.args[i], scope);
          const fieldDef = structDef[i];
          if (argType !== T.UNKNOWN && fieldDef.varType !== argType && argType !== T.ANY) {
            this.error('TYPE_MISMATCH',
              `CREATE "${node.identifier}": field "${fieldDef.name}" expects ${fieldDef.varType}, got ${argType}`,
              node.line, node.column);
          }
        }
      }
      return;
    }

    if (declaredType && inferredType !== T.UNKNOWN && inferredType !== T.ANY) {
      if (!this._compatible(declaredType, inferredType)) {
        this.error('TYPE_MISMATCH',
          `CREATE "${node.identifier}": declared type ${declaredType} but value is ${inferredType}`,
          node.line, node.column);
      }
    }
    scope.setVar(node.identifier, declaredType || inferredType, { line: node.line });
  }

  _checkSet(node, scope) {
    // SET x TO expr  or  SET x:prop TO expr  or  SET x.field TO expr
    const target = node.identifier;
    if (!target) return;

    // Struct member access: SET obj.field TO expr
    if (node.isMemberAccess) {
      const obj = scope.getVar(node.memberObject);
      if (!obj) {
        this.error('UNDEFINED_VAR',
          `SET: "${node.memberObject}" is not defined`,
          node.line, node.column);
        return;
      }
      const structDef = scope.getStruct(obj.type);
      if (!structDef) {
        this.error('TYPE_MISMATCH',
          `SET: "${node.memberObject}" is not a struct — cannot access .${node.memberField}`,
          node.line, node.column);
        return;
      }
      const field = structDef.find(f => f.name === node.memberField);
      if (!field) {
        this.error('TYPE_MISMATCH',
          `SET: struct "${node.memberObject}" has no field "${node.memberField}"`,
          node.line, node.column);
        return;
      }
      const valType = this._inferExprString(node.valueExpr, scope);
      if (valType !== T.UNKNOWN && field.varType !== valType && !NUMERIC.has(field.varType) === !NUMERIC.has(valType)) {
        if (field.varType !== valType) {
          this.error('TYPE_MISMATCH',
            `SET: field "${node.memberField}" expects ${field.varType}, got ${valType}`,
            node.line, node.column);
        }
      }
      return;
    }

    // Check target:prop form — object must exist
    if (node.propExpr || (target && target.includes(':'))) {
      const parts = target.split(':');
      const obj = scope.getVar(parts[0]);
      if (!obj) {
        this.error('UNDEFINED_VAR',
          `SET: "${parts[0]}" is not defined`,
          node.line, node.column);
        return;
      }
      if (obj.locked) {
        this.error('LOCK_VIOLATION',
          `SET: "${parts[0]}" is a locked ROOT — cannot be modified`,
          node.line, node.column);
      }
      return;
    }

    const existing = scope.getVar(target);
    if (!existing) {
      this.warn('UNDEFINED_VAR',
        `SET: "${target}" was not declared with CREATE — did you mean CREATE?`,
        node.line, node.column);
      return;
    }

    if (existing.locked) {
      this.error('LOCK_VIOLATION',
        `SET: "${target}" is a locked ROOT constant — cannot be modified`,
        node.line, node.column);
      return;
    }

    const newType = this._inferExprString(node.valueExpr, scope);
    if (existing.type !== T.UNKNOWN && newType !== T.UNKNOWN && newType !== T.ANY) {
      if (!this._compatible(existing.type, newType)) {
        this.error('TYPE_MISMATCH',
          `SET "${target}": variable is ${existing.type} but new value is ${newType}`,
          node.line, node.column);
      }
    }
  }

  _checkArithmetic(node, scope) {
    const varInfo = scope.getVar(node.identifier);
    if (!varInfo) {
      this.error('UNDEFINED_VAR',
        `${node.type === 'IncreaseStatement' ? 'INCREASE' : 'DECREASE'}: "${node.identifier}" is not defined`,
        node.line, node.column);
      return;
    }
    if (!NUMERIC.has(varInfo.type) && varInfo.type !== T.UNKNOWN) {
      this.error('TYPE_MISMATCH',
        `INCREASE/DECREASE: "${node.identifier}" is ${varInfo.type} — only NUM/SCL supported`,
        node.line, node.column);
    }
  }

  _checkShow(node, scope) {
    if (!node.expr) return;
    // Always infer expression type to validate method calls etc.
    this._inferExprNode(node.expr, scope);
    // Additional warning for plain Identifier nodes that may be undefined
    if (node.expr.type === 'Identifier') {
      const name = node.expr.identifier || node.expr.value;
      if (name && name !== 'undefined') {
        const v = scope.getVar(name);
        if (!v) {
          this.warn('UNDEFINED_VAR',
            `SHOW: "${name}" may not be defined`,
            node.line, node.column);
        }
      }
    }
  }

  _checkReap(node, scope) {
    if (!node.source) return;
    const varName = node.variable;

    // REAP x FROM NOW  — always valid
    if (node.source.kind === 'NOW') {
      if (varName && varName !== '_') scope.setVar(varName, T.TX, { line: node.line });
      return;
    }

    // REAP x FROM library:FN, args
    if (node.source.kind === 'LIB') {
      const key = `${node.source.lib}:${node.source.fn}`;
      const rt = INNATE_RETURN[key] || T.UNKNOWN;
      if (varName && varName !== '_') scope.setVar(varName, rt, { line: node.line });
      return;
    }

    // REAP x FROM action, args
    if (node.source.kind === 'ACTION') {
      const fnName = node.source.name;
      const fn = scope.getFn(fnName);

      if (!fn) {
        this.error('UNDEFINED_ACTION',
          `REAP: action "${fnName}" is not defined`,
          node.line, node.column);
        if (varName && varName !== '_') scope.setVar(varName, T.UNKNOWN, { line: node.line });
        return;
      }

      // Check argument count
      const expected = (fn.params || []).length;
      const got = (node.args || []).length;
      if (expected !== got) {
        this.error('ARITY_MISMATCH',
          `REAP: action "${fnName}" expects ${expected} argument(s), got ${got}`,
          node.line, node.column);
      }

      // Check argument types
      for (let i = 0; i < Math.min(expected, got); i++) {
        const param     = fn.params[i];
        const argExpr   = node.args[i];
        const argType   = this._inferExprString(argExpr, scope);
        if (param.type && param.type !== T.UNKNOWN && argType !== T.UNKNOWN && argType !== T.ANY) {
          if (!this._compatible(param.type, argType)) {
            this.error('TYPE_MISMATCH',
              `REAP "${fnName}" arg "${param.name}": expected ${param.type}, got ${argType}`,
              node.line, node.column);
          }
        }
      }

      const rt = fn.returnType || T.UNKNOWN;
      if (varName && varName !== '_') scope.setVar(varName, rt, { line: node.line });
    }

    // REAP x FROM obj:method
    if (node.source.kind === 'METHOD') {
      if (varName && varName !== '_') scope.setVar(varName, T.UNKNOWN, { line: node.line });
    }

    // REAP x FROM LITERAL "string" FLOW ...
    if (node.source.kind === 'LITERAL') {
      if (varName && varName !== '_') scope.setVar(varName, T.TX, { line: node.line });
    }
  }

  _checkVariantDeclaration(node, scope) {
    // Register the CHOICE type and its variants
    scope.setChoice(node.name, node.variants);
  }

  _checkIf(node, scope) {
    for (const branch of (node.branches || [])) {
      const branchScope = scope.child();
      this._checkBlock(branch.bodyStatements, branchScope);
    }
  }

  _checkCycle(node, scope) {
    const iterScope = scope.child();
    if (node.sourceExpr) {
      const listVar = scope.getVar(node.sourceExpr);
      if (listVar && listVar.type !== T.LIST && listVar.type !== T.UNKNOWN) {
        this.error('TYPE_MISMATCH',
          `CYCLE: "${node.sourceExpr}" is ${listVar.type} — IN requires a LIST`,
          node.line, node.column);
      }
      iterScope.setVar(node.iterVar, T.UNKNOWN);
    } else {
      // FROM n TO m — iterVar is NUM
      iterScope.setVar(node.iterVar, T.NUM);
    }
    this._checkBlock(node.bodyStatements, iterScope);
  }

  _checkSeason(node, scope) {
    this._checkBlock(node.bodyStatements, scope.child());
  }

  _checkMatch(node, scope) {
    // Legacy MATCH format — just check subject is defined
    if (node.clauses.length > 0 && node.clauses[0].clauseText !== undefined) {
      const subjectVar = scope.getVar(node.subjectExpr);
      if (!subjectVar && /^[a-zA-Z_]\w*$/.test(node.subjectExpr)) {
        this.warn('UNDEFINED_VAR',
          `MATCH: subject "${node.subjectExpr}" may not be defined`,
          node.line, node.column);
      }
      return;
    }

    // New pattern-matching MATCH
    const subjectType = this._inferExprNode(node.subjectExpr, scope);
    if (!subjectType || subjectType === T.UNKNOWN) {
      this.warn('UNKNOWN_TYPE',
        `MATCH: subject type is unknown`,
        node.line, node.column);
      return;
    }

    // Check if subject is a CHOICE type
    const choiceDef = scope.getChoice(subjectType);
    if (!choiceDef) {
      this.warn('TYPE_MISMATCH',
        `MATCH: "${subjectType}" is not a CHOICE type`,
        node.line, node.column);
      return;
    }

    // Exhaustiveness check: every variant must have a clause
    const covered = new Set(node.clauses.map(c => c.variantName.toUpperCase()));
    for (const variant of choiceDef) {
      if (!covered.has(variant.name.toUpperCase())) {
        this.warn('INCOMPLETE_MATCH',
          `MATCH: missing clause for variant "${variant.name}" in CHOICE "${subjectType}"`,
          node.line, node.column);
      }
    }

    // Check each clause — bind the payload variable if present
    for (const clause of node.clauses) {
      const variant = choiceDef.find(v => v.name.toUpperCase() === clause.variantName.toUpperCase());
      if (!variant) {
        this.error('UNDEFINED_VARIANT',
          `MATCH: "${clause.variantName}" is not a variant of "${subjectType}"`,
          node.line, node.column);
        continue;
      }
      if (clause.binding && !variant.type) {
        this.warn('UNUSED_BINDING',
          `MATCH: variant "${variant.name}" has no payload, but binding "${clause.binding}" provided`,
          node.line, node.column);
      }
      if (!clause.binding && variant.type) {
        this.warn('MISSING_BINDING',
          `MATCH: variant "${variant.name}" has payload type ${variant.type}, but no binding variable`,
          node.line, node.column);
      }
      // Check body statements in a child scope with the payload bound
      if (clause.binding && variant.type) {
        const clauseScope = scope.child();
        clauseScope.setVar(clause.binding, variant.type, { line: node.line });
        this._checkBlock(clause.bodyStatements, clauseScope);
      } else {
        this._checkBlock(clause.bodyStatements, scope);
      }
    }
  }

  _checkActionBody(node, scope) {
    // FFI external actions have no body — skip checking
    if (node.isExternal) return;
    const fnScope = scope.child();
    // Inject receiver (self) into scope for methods
    if (node.receiver) {
      fnScope.setVar(node.receiver.name, node.receiver.type, { line: node.line });
    }
    for (const p of (node.params || [])) {
      fnScope.setVar(p.name, p.type || T.UNKNOWN);
    }
    // Pass 1 on nested actions
    this._pass1(node.body || node.bodyStatements || [], fnScope);
    this._checkBlock(node.body || node.bodyStatements || [], fnScope);
  }

  _checkSpeciesBody(node, scope) {
    const classScope = scope.child();
    const spec = scope.getSpecies(node.name);
    if (spec) {
      for (const [k, t] of Object.entries(spec.fields || {})) {
        classScope.setVar(k, t);
      }
    }
    for (const m of (node.members || [])) {
      if (!m) continue;
      if (m.type === 'ActionDeclaration') this._checkActionBody(m, classScope);
    }
  }

  _checkBloom(node, scope) {
    // BloomStatement fields: speciesName + instanceIdent
    const className    = node.speciesName || node.className;
    const instanceName = node.instanceIdent || node.instanceName || node.resultIdent;
    const spec = className ? scope.getSpecies(className) : null;
    if (className && !spec) {
      this.warn('UNDEFINED_SPECIES',
        `BLOOM: species "${className}" is not defined in this scope`,
        node.line, node.column);
    }
    if (instanceName) {
      scope.setVar(instanceName, T.INSTANCE, { line: node.line });
    }
  }

  _checkWeather(node, scope) {
    // WEATHER body is intentionally "risky" — errors inside are caught by SHELTER.
    // Demote any errors found in the WEATHER body to info-level notes.
    const savedLen = this.diags.length;
    const weatherScope = scope.child();
    this._checkBlock(node.bodyStatements, weatherScope);
    // Downgrade errors added while checking the WEATHER body to 'info'
    for (let i = savedLen; i < this.diags.length; i++) {
      if (this.diags[i].severity === 'error') {
        this.diags[i] = new Diagnostic('info',
          this.diags[i].code,
          `(inside WEATHER — protected) ${this.diags[i].message}`,
          this.diags[i].line, this.diags[i].column);
      }
    }
    for (const shelter of (node.shelterClauses || [])) {
      const shelterScope = scope.child();
      if (shelter.errVar) shelterScope.setVar(shelter.errVar, T.TX);
      this._checkBlock(shelter.bodyStatements, shelterScope);
    }
    if (node.calmClause) {
      this._checkBlock(node.calmClause.bodyStatements || [], scope.child());
    }
  }

  _checkPut(node, scope) {
    const target = node.targetExpr;
    if (!target) return;
    const baseName = target.replace(/SELF:/, '').split(':')[0];
    const v = scope.getVar(baseName);
    if (!v && !/^SELF:/.test(target)) {
      this.error('UNDEFINED_VAR',
        `PUT: target "${baseName}" is not defined`,
        node.line, node.column);
    } else if (v && v.type !== T.LIST && v.type !== T.UNKNOWN) {
      this.error('TYPE_MISMATCH',
        `PUT: "${baseName}" is ${v.type} — PUT requires a LIST`,
        node.line, node.column);
    }
  }

  _checkTake(node, scope) {
    const listName = node.listExpr;
    const v = scope.getVar(listName);
    if (!v) {
      this.error('UNDEFINED_VAR',
        `TAKE: "${listName}" is not defined`,
        node.line, node.column);
    } else if (v.type !== T.LIST && v.type !== T.UNKNOWN) {
      this.error('TYPE_MISMATCH',
        `TAKE: "${listName}" is ${v.type} — TAKE requires a LIST`,
        node.line, node.column);
    }
  }

  _checkBraid(node, scope) {
    for (const listId of [node.list1, node.list2]) {
      const v = scope.getVar(listId);
      if (!v) {
        this.error('UNDEFINED_VAR',
          `BRAID: "${listId}" is not defined`,
          node.line, node.column);
      } else if (v.type !== T.LIST && v.type !== T.UNKNOWN) {
        this.error('TYPE_MISMATCH',
          `BRAID: "${listId}" is ${v.type} — BRAID requires two LISTs`,
          node.line, node.column);
      }
    }
    const resultType = node.asMap ? T.MAP : T.LIST;
    scope.setVar(node.resultIdent, resultType, { line: node.line });
  }

  _checkHarvest(node, scope) {
    // HARVEST result is always a MAP {ok, status, body, headers}
    if (node.resultIdent) {
      scope.setVar(node.resultIdent, T.MAP, { line: node.line });
    }
  }

  _checkVerify(node, scope) {
    // Verify that FROM action GIVES uses a defined action
    if (!node.assertion) return;
    const m = node.assertion.match(/^FROM\s+(\w+)/i);
    if (m) {
      const fn = scope.getFn(m[1]);
      if (!fn) {
        this.warn('UNDEFINED_ACTION',
          `VERIFY: action "${m[1]}" is not defined — test will fail at runtime`,
          node.line, node.column);
      }
    }
  }

  _checkSuite(node, scope) {
    const suiteScope = scope.child();
    this._pass1(node.bodyStatements, suiteScope);
    this._checkBlock(node.bodyStatements, suiteScope);
  }

  _checkLock(node, scope) {
    const v = scope.getVar(node.identifier);
    if (!v) {
      this.warn('UNDEFINED_VAR',
        `LOCK: "${node.identifier}" is not defined`,
        node.line, node.column);
    }
  }

  _checkEvaporate(node, scope) {
    const v = scope.getVar(node.identifier);
    if (!v) {
      this.warn('UNDEFINED_VAR',
        `EVAPORATE: "${node.identifier}" was never defined`,
        node.line, node.column);
    }
  }

  _checkStopIf(node, scope) {
    // No specific type errors — just walk
  }

  _checkMethodCall(node, scope) {
    const targetType = this._inferExprNode(node.target, scope);
    if (!targetType || targetType === T.UNKNOWN) {
      this.warn('UNKNOWN_TYPE',
        `Method call "${node.methodName}" on unknown type — cannot validate`,
        node.line, node.column);
      return;
    }

    // ── Intrinsic array methods: push and pop ──────────────────────────
    if (isArrayType(targetType)) {
      const innerType = arrayInnerType(targetType);
      if (node.methodName === 'push') {
        const got = (node.args || []).length;
        if (got !== 1) {
          this.error('ARITY_MISMATCH',
            `Array push expects 1 argument (item of type ${innerType}), got ${got}`,
            node.line, node.column);
          return;
        }
        const argType = this._inferExprNode(node.args[0], scope);
        if (argType !== T.UNKNOWN && argType !== T.ANY && !this._compatible(innerType, argType)) {
          this.error('TYPE_MISMATCH',
            `Array push: expected ${innerType}, got ${argType}`,
            node.args[0].line || node.line, node.args[0].column || node.column);
        }
        return;
      }
      if (node.methodName === 'pop') {
        const got = (node.args || []).length;
        if (got !== 0) {
          this.error('ARITY_MISMATCH',
            `Array pop expects 0 arguments, got ${got}`,
            node.line, node.column);
        }
        return;
      }
    }

    // ── Intrinsic MAP methods: put, get, has ───────────────────────────
    if (isMapType(targetType)) {
      const mt = mapInnerTypes(targetType);
      if (node.methodName === 'put') {
        const got = (node.args || []).length;
        if (got !== 2) {
          this.error('ARITY_MISMATCH',
            `Map put expects 2 arguments (key of type ${mt.keyType}, value of type ${mt.valueType}), got ${got}`,
            node.line, node.column);
          return;
        }
        const keyType = this._inferExprNode(node.args[0], scope);
        const valType = this._inferExprNode(node.args[1], scope);
        if (keyType !== T.UNKNOWN && keyType !== T.ANY && !this._compatible(mt.keyType, keyType)) {
          this.error('TYPE_MISMATCH',
            `Map put: expected key type ${mt.keyType}, got ${keyType}`,
            node.args[0].line || node.line, node.args[0].column || node.column);
        }
        if (valType !== T.UNKNOWN && valType !== T.ANY && !this._compatible(mt.valueType, valType)) {
          this.error('TYPE_MISMATCH',
            `Map put: expected value type ${mt.valueType}, got ${valType}`,
            node.args[1].line || node.line, node.args[1].column || node.column);
        }
        return;
      }
      if (node.methodName === 'get') {
        const got = (node.args || []).length;
        if (got !== 1) {
          this.error('ARITY_MISMATCH',
            `Map get expects 1 argument (key of type ${mt.keyType}), got ${got}`,
            node.line, node.column);
          return;
        }
        const keyType = this._inferExprNode(node.args[0], scope);
        if (keyType !== T.UNKNOWN && keyType !== T.ANY && !this._compatible(mt.keyType, keyType)) {
          this.error('TYPE_MISMATCH',
            `Map get: expected key type ${mt.keyType}, got ${keyType}`,
            node.args[0].line || node.line, node.args[0].column || node.column);
        }
        // get() returns Option<V> — return type is the choice type name
        return;
      }
      if (node.methodName === 'has') {
        const got = (node.args || []).length;
        if (got !== 1) {
          this.error('ARITY_MISMATCH',
            `Map has expects 1 argument (key of type ${mt.keyType}), got ${got}`,
            node.line, node.column);
          return;
        }
        const keyType = this._inferExprNode(node.args[0], scope);
        if (keyType !== T.UNKNOWN && keyType !== T.ANY && !this._compatible(mt.keyType, keyType)) {
          this.error('TYPE_MISMATCH',
            `Map has: expected key type ${mt.keyType}, got ${keyType}`,
            node.args[0].line || node.line, node.args[0].column || node.column);
        }
        return;
      }
      this.error('UNDEFINED_METHOD',
        `MAP[${mt.keyType},${mt.valueType}] has no method "${node.methodName}" (available: put, get, has)`,
        node.line, node.column);
      return;
    }

    // ── Choice/variant construction ────────────────────────────────
    const choiceDef = scope.getChoice(targetType);
    if (choiceDef) {
      const variant = choiceDef.find(v => v.name.toUpperCase() === node.methodName.toUpperCase());
      if (!variant) {
        this.error('UNDEFINED_VARIANT',
          `CHOICE "${targetType}" has no variant "${node.methodName}"`,
          node.line, node.column);
        return;
      }
      const got = (node.args || []).length;
      const expected = variant.type ? 1 : 0;
      if (got !== expected) {
        this.error('ARITY_MISMATCH',
          `Variant "${targetType}.${variant.name}" expects ${expected} argument(s) (payload of type "${variant.type || 'none'}"), got ${got}`,
          node.line, node.column);
        return;
      }
      if (variant.type && got === 1) {
        const argType = this._inferExprNode(node.args[0], scope);
        if (argType !== T.UNKNOWN && argType !== T.ANY && !this._compatible(variant.type, argType)) {
          this.error('TYPE_MISMATCH',
            `Variant "${targetType}.${variant.name}" expects payload type ${variant.type}, got ${argType}`,
            node.args[0].line || node.line, node.args[0].column || node.column);
        }
      }
      return;
    }

    // ── User-defined methods (structs) ─────────────────────────────
    const method = scope.getMethod(targetType, node.methodName);
    if (!method) {
      this.error('UNDEFINED_METHOD',
        `Type "${targetType}" has no method "${node.methodName}"`,
        node.line, node.column);
      return;
    }
    // Validate argument count
    const expected = (method.params || []).length;
    const got = (node.args || []).length;
    if (expected !== got) {
      this.error('ARITY_MISMATCH',
        `Method "${node.methodName}" on ${targetType} expects ${expected} argument(s), got ${got}`,
        node.line, node.column);
    }
    // Validate argument types
    for (let i = 0; i < Math.min(expected, got); i++) {
      const param = method.params[i];
      const argType = this._inferExprNode(node.args[i], scope);
      if (param.type && param.type !== T.UNKNOWN && argType !== T.UNKNOWN && argType !== T.ANY) {
        if (!this._compatible(param.type, argType)) {
          this.error('TYPE_MISMATCH',
            `Method "${node.methodName}" arg "${param.name}": expected ${param.type}, got ${argType}`,
            node.args[i].line || node.line, node.args[i].column || node.column);
        }
      }
    }
  }

  // ── Expression type inference ────────────────────────────────────────────────

  _inferExprNode(node, scope) {
    if (!node) return T.UNKNOWN;
    if (typeof node === 'string') return this._inferExprString(node, scope);
    if (node.type === 'Literal') {
      if (node.literalType === 'NUMBER')  return T.NUM;
      if (node.literalType === 'STRING')  return T.TX;
      if (node.literalType === 'BOOLEAN') return T.FACT;
      if (node.literalType === 'RAW_EXPR') return this._inferExprString(node.value, scope);
    }
    if (node.type === 'Identifier') {
      const v = scope.getVar(node.name || node.identifier || node.value);
      if (v) return v.type;
      // Check if the name is a CHOICE type (for variant construction like Option.Some(10))
      const choice = scope.getChoice(node.name || node.identifier || node.value);
      if (choice) return node.name || node.identifier || node.value;
      return T.UNKNOWN;
    }
    if (node.type === 'ArrayLiteral') {
      // Infer array type from elements if possible
      if (node.elements.length === 0) return T.LIST;
      const firstType = this._inferExprNode(node.elements[0], scope);
      if (firstType === T.UNKNOWN) return T.LIST;
      return `[${firstType}]`;
    }
    if (node.type === 'MapLiteral') {
      // Infer MAP type from entries if possible
      if (node.entries.length === 0) return T.MAP;
      const firstKeyType = this._inferExprNode(node.entries[0].key, scope);
      const firstValType = this._inferExprNode(node.entries[0].value, scope);
      if (firstKeyType === T.UNKNOWN || firstValType === T.UNKNOWN) return T.MAP;
      return `MAP[${firstKeyType},${firstValType}]`;
    }
    if (node.type === 'LenCall' || node.type === 'CapCall') {
      return T.NUM;
    }
    if (node.type === 'IndexAccess') {
      const targetType = this._inferExprNode(node.target, scope);
      // If target is an array type like [NUM], return the inner type
      if (isArrayType(targetType)) {
        return arrayInnerType(targetType);
      }
      // If target is a struct type, return UNKNOWN (field-level index not supported)
      if (targetType && scope.getStruct(targetType)) {
        return T.UNKNOWN;
      }
      return T.TX; // default: TX indexing (string characters)
    }
    if (node.type === 'MemberAccess') {
      const objType = this._inferExprNode(node.object, scope);
      if (typeof objType === 'string' && scope.getStruct(objType)) {
        const structDef = scope.getStruct(objType);
        const field = structDef.find(f => f.name === node.member);
        if (field) return field.varType;
      }
      // CHOICE variant access (e.g., Option.None, Option.Bad)
      if (typeof objType === 'string' && scope.getChoice(objType)) {
        const choiceDef = scope.getChoice(objType);
        const variant = choiceDef.find(v => v.name.toUpperCase() === node.member.toUpperCase());
        if (!variant) {
          this.error('UNDEFINED_VARIANT',
            `CHOICE "${objType}" has no variant "${node.member}"`,
            node.line, node.column);
          return T.UNKNOWN;
        }
        return objType;
      }
      return T.UNKNOWN;
    }
    if (node.type === 'StructInstantiation') {
      const st = scope.getStruct(node.structName);
      return st ? node.structName : T.UNKNOWN;
    }
    if (node.type === 'MethodCall') {
      // Validate via _checkMethodCall (emits diagnostics)
      this._checkMethodCall(node, scope);
      const targetType = this._inferExprNode(node.target, scope);
      // Intrinsic array methods
      if (targetType && isArrayType(targetType)) {
        if (node.methodName === 'push') return T.VOID;
        if (node.methodName === 'pop') return arrayInnerType(targetType);
      }
      // Intrinsic MAP methods
      if (targetType && isMapType(targetType)) {
        const mt = mapInnerTypes(targetType);
        if (node.methodName === 'put') return T.VOID;
        if (node.methodName === 'has') return T.FACT;
        if (node.methodName === 'get') return `Option<${mt.valueType}>`;
      }
      // Choice variant construction returns the choice type
      if (targetType && scope.getChoice(targetType)) {
        return targetType;
      }
      // User-defined methods
      if (targetType && targetType !== T.UNKNOWN) {
        const method = scope.getMethod(targetType, node.methodName);
        if (method) return method.returnType || T.UNKNOWN;
      }
      return T.UNKNOWN;
    }
    return T.UNKNOWN;
  }

  _inferExprString(expr, scope) {
    if (expr === null || expr === undefined) return T.UNKNOWN;
    if (typeof expr !== 'string') return this._inferExprNode(expr, scope);
    return inferExprString(expr, scope);
  }

  // ── Type compatibility ────────────────────────────────────────────────────────
  _compatible(declared, actual) {
    if (declared === T.ANY || actual === T.ANY) return true;
    if (declared === T.UNKNOWN || actual === T.UNKNOWN) return true;
    // NUM and SCL are inter-compatible
    if (NUMERIC.has(declared) && NUMERIC.has(actual)) return true;
    return declared === actual;
  }
}

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * typecheck(programNode, source?) → Diagnostic[]
 *
 * Runs the static type checker on a parsed PlantLang program.
 * Returns an array of Diagnostic objects (may be empty if no errors found).
 */
function typecheck(programNode, source) {
  const checker = new TypeChecker(source);
  return checker.check(programNode);
}

module.exports = { typecheck, Diagnostic, TypeChecker, T };
