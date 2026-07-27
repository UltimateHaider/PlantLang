'use strict';
const {storm,PlantStorm,inferType,coerce,Soil,VeinFS}=require('./runtime');
const {harvestSync,toPlantValue}=require('./harvest');
const {evalExpr,evalCond}=require('./evaluator');
const {INNATE}=require('./innate');
const {lex,subTokenColumn}=require('./lexer');
const {ListenBranchStatementNode,ResponseStatementNode}=require('./ast');
const fs=require('fs');
const path=require('path');
const { evaluateCycleInStatement } = require('../src/interpreter/cycle_evaluator');
const { evaluateSortStatement } = require('../src/interpreter/sort_evaluator');
const { evaluateBloomAsStatement } = require('../src/interpreter/bloom_evaluator');
const { formatShowValue } = require('../src/interpreter/show_formatter');
const { ArenaAllocator, ARCHeap } = require('../src/memory/allocator');
const { BreakSignalException, ContinueSignalException } = require('./ast');

class Interpreter {
  constructor(opts={}){
    this.mission=opts.mission||'SAFE';
    this.soil=new Soil();
    this.veinFS=new VeinFS();
    this.funcs=new Map();
    this.species=new Map();
    this.planted=new Map();
    this.structs=new Map(); // SHAPE definitions: name -> [{ name, varType }]
    this.choices=new Map(); // CHOICE definitions: name -> [{ name, type }]
    // v0.44.0: Built-in Option and Result algebraic types
    this.choices.set('Option', [{ name: 'Some', type: 'ANY' }, { name: 'None', type: null }]);
    this.choices.set('Result', [{ name: 'Ok', type: 'ANY' }, { name: 'Err', type: 'ANY' }]);
    this.typeMethods=new Map(); // type name -> Map(methodName -> fnInfo)
    this.watchers=new Map();
    this.rootDir=opts.rootDir||process.cwd();
    this.output=opts.output||[];
    this._cliArgs=opts.cliArgs||[]; // CLI arguments for get_cli_arg FFI
    this._externalFFI=new Map(); // name -> JS function for external FFI stubs in interpreted mode
    // Auto-register std/io bridge stubs
    this._registerStdStubs();
    this.emit=opts.emit||((line,type)=>{this.output.push({text:line,type:type||'info'});});
    this._symbolPassDone=false; // set true once symbolPass() runs; suppresses duplicate output
    // VERIFY tracking
    this.verifyStats={passed:0,failed:0,suite:null,results:[]};
    this.veinFS.write('demo.txt','line one\nline two\nline three');
  }

  _emit(msg) {
    if (this._emitFn) this._emitFn(msg);
  }

  run(source){
    const stmts=lex(source);
    this._firstPass(stmts);
    this._execBlock(stmts,0,stmts.length,this.soil);
  }

  // ═══════════════════════════════════════════════════════════
  //  AST evaluation bridge (compiler-frontend migration).
  //
  //  Additive entry point: accepts a parsed ProgramNode (from
  //  core/parser.js) instead of raw source text, and routes each
  //  statement node through a typed evaluator that operates on the
  //  same Soil scope-chain as the legacy regex pipeline above. This
  //  exists alongside run()/_execBlock() rather than replacing them
  //  — the 176-test regression matrix continues to exercise the
  //  proven regex pipeline, while runProgram()/evaluateNode() let
  //  AST-migrated statement kinds (SHOW, CREATE so far) be executed
  //  and verified independently as the migration proceeds.
  // ═══════════════════════════════════════════════════════════

  /** Run a fully-parsed ProgramNode AST against this interpreter's Soil. */
  runProgram(programNode){
    // symbolPass() must have already been called before runProgram() —
    // runSource() calls it automatically. If calling runProgram() directly,
    // call symbolPass(programNode) first to register declarations.
    for(const stmtNode of programNode.statements){
      this.evaluateNode(stmtNode,this.soil);
    }
  }

  runSource(source, sourcePath){
    const {parse}=require('./parser');
    const programNode=parse(source);
    if(sourcePath) this.rootDir=path.dirname(path.resolve(sourcePath));
    this.symbolPass(programNode);         // register declarations before execution
    this.runProgram(programNode);
    return programNode;
  }

  /** Resolve an AST expression node (Literal/Identifier/RAW_EXPR) to a value. */
  evaluateExpressionNode(node,soil){
    if(node===null||node===undefined)return null;
    if(node.type==='Literal'){
      if(node.literalType==='RAW_EXPR')return evalExpr(node.value,soil);
      return node.value;
    }
    if(node.type==='Identifier'){
      const e=soil.get(node.name);
      if(e)return e.value;
      // Check if it's a CHOICE type name — return a sentinel marker
      if(this.choices.has(node.name))return{__choiceType:node.name};
      storm('MISSING_STORM',`"${node.name}" not found`,node.line,node.column);
    }
    if(node.type==='ArrayLiteral'){
      return this.evaluateArrayLiteral(node, soil);
    }
    if(node.type==='MapLiteral'){
      const map = new Map();
      for (const entry of node.entries) {
        const key = this.evaluateExpressionNode(entry.key, soil);
        const val = this.evaluateExpressionNode(entry.value, soil);
        map.set(key, val);
      }
      return map;
    }
    if(node.type==='LenCall'){
      const arg=this.evaluateExpressionNode(node.arg,soil);
      if(Array.isArray(arg))return arg.length;
      if(typeof arg!=='string')return 0;
      return arg.length;
    }
    if(node.type==='CapCall'){
      const arg=this.evaluateExpressionNode(node.arg,soil);
      if(Array.isArray(arg))return arg.length;
      if(typeof arg!=='string')return 0;
      return arg.length+1;
    }
    if(node.type==='ListOp'){
      const arg=this.evaluateExpressionNode(node.arg,soil);
      if(!Array.isArray(arg))storm('TYPE_STORM',`${node.operation} requires an array argument`,node.line,node.column);
      if(node.operation==='COUNT')return arg.length;
      if(node.operation==='FIRST'){
        if(arg.length===0)storm('SEED_STORM','FIRST on empty array',node.line,node.column);
        return arg[0];
      }
      if(node.operation==='LAST'){
        if(arg.length===0)storm('SEED_STORM','LAST on empty array',node.line,node.column);
        return arg[arg.length-1];
      }
      if(node.operation==='SUM'){
        return arg.reduce((a,b)=>{
          const na=typeof a==='number'?a:0;
          const nb=typeof b==='number'?b:0;
          return na+nb;
        },0);
      }
      if(node.operation==='SORT'){
        arg.sort((a,b)=>a-b);
        return null;
      }
      storm('TYPE_STORM',`Unknown list operation "${node.operation}"`,node.line,node.column);
    }
    if(node.type==='StringOp'){
      if(node.operation==='SPLIT'){
        const src=this.evaluateExpressionNode(node.arg1,soil);
        const delim=this.evaluateExpressionNode(node.arg2,soil);
        if(typeof src!=='string')storm('TYPE_STORM','SPLIT requires TX first argument',node.line,node.column);
        if(typeof delim!=='string')storm('TYPE_STORM','SPLIT requires TX delimiter',node.line,node.column);
        return src.split(delim);
      }
      if(node.operation==='JOIN'){
        const arr=this.evaluateExpressionNode(node.arg1,soil);
        const delim=this.evaluateExpressionNode(node.arg2,soil);
        if(!Array.isArray(arr))storm('TYPE_STORM','JOIN requires [TX] array first argument',node.line,node.column);
        if(typeof delim!=='string')storm('TYPE_STORM','JOIN requires TX delimiter',node.line,node.column);
        return arr.join(delim);
      }
      storm('TYPE_STORM',`Unknown string operation "${node.operation}"`,node.line,node.column);
    }
    if(node.type==='IndexAccess'){
      const target=this.evaluateExpressionNode(node.target,soil);
      const idx=this.evaluateExpressionNode(node.index,soil);
      if(typeof idx!=='number'||!Number.isInteger(idx)||idx<0)storm('TYPE_STORM','Index must be a non-negative integer',node.line,node.column);
      if(Array.isArray(target)){
        if(idx>=target.length)storm('SEED_STORM',`Index ${idx} out of bounds for array of length ${target.length}`,node.line,node.column);
        return target[idx];
      }
      if(typeof target!=='string')storm('TYPE_STORM','Index access requires a TX or array value',node.line,node.column);
      if(idx>=target.length)storm('SEED_STORM',`Index ${idx} out of bounds for TX of length ${target.length}`,node.line,node.column);
      return target[idx];
    }
    if(node.type==='MethodCall'){
      const target=this.evaluateExpressionNode(node.target,soil);
      // ── Intrinsic array methods ──────────────────────────────
      if(Array.isArray(target)){
        if(node.methodName==='push'){
          const argVals=(node.args||[]).map(a=>this.evaluateExpressionNode(a,soil));
          if(argVals.length!==1)storm('ARITY_STORM','push expects exactly 1 argument',node.line,node.column);
          target.push(argVals[0]);
          return target;
        }
        if(node.methodName==='pop'){
          if(target.length===0)storm('SEED_STORM','pop on empty array',node.line,node.column);
          return target.pop();
        }
        storm('MISSING_STORM',`Array has no method "${node.methodName}"`,node.line,node.column);
      }
      // ── Intrinsic MAP methods ────────────────────────────────
      if (target instanceof Map) {
        if (node.methodName === 'put') {
          const argVals = (node.args||[]).map(a=>this.evaluateExpressionNode(a,soil));
          if (argVals.length !== 2) storm('ARITY_STORM','put expects exactly 2 arguments (key, value)',node.line,node.column);
          target.set(argVals[0], argVals[1]);
          return target;
        }
        if (node.methodName === 'get') {
          const argVals = (node.args||[]).map(a=>this.evaluateExpressionNode(a,soil));
          if (argVals.length !== 1) storm('ARITY_STORM','get expects exactly 1 argument (key)',node.line,node.column);
          const val = target.get(argVals[0]);
          if (val !== undefined) {
            return { __choiceType: 'Option', tag: 'Some', payload: val };
          } else {
            return { __choiceType: 'Option', tag: 'None', payload: null };
          }
        }
        if (node.methodName === 'has') {
          const argVals = (node.args||[]).map(a=>this.evaluateExpressionNode(a,soil));
          if (argVals.length !== 1) storm('ARITY_STORM','has expects exactly 1 argument (key)',node.line,node.column);
          return target.has(argVals[0]);
        }
        storm('MISSING_STORM',`MAP has no method "${node.methodName}" (available: put, get, has)`,node.line,node.column);
      }
      // ── CHOICE variant construction ──────────────────────────
      if(typeof target==='object'&&target!==null&&target.__choiceType){
        const choiceName=target.__choiceType;
        const choiceDef=this.choices.get(choiceName);
        if(!choiceDef)storm('MISSING_STORM',`CHOICE "${choiceName}" is not defined`,node.line,node.column);
        const variant=choiceDef.find(v=>v.name.toUpperCase()===node.methodName.toUpperCase());
        if(!variant)storm('MISSING_STORM',`CHOICE "${choiceName}" has no variant "${node.methodName}"`,node.line,node.column);
        const argVals=(node.args||[]).map(a=>this.evaluateExpressionNode(a,soil));
        if(variant.type&&argVals.length!==1)
          storm('ARITY_STORM',`Variant "${choiceName}.${variant.name}" expects 1 payload argument, got ${argVals.length}`,node.line,node.column);
        if(!variant.type&&argVals.length!==0)
          storm('ARITY_STORM',`Variant "${choiceName}.${variant.name}" expects 0 arguments, got ${argVals.length}`,node.line,node.column);
        return{__choiceType:choiceName,tag:variant.name,payload:variant.type?argVals[0]:null};
      }

      // ── Struct methods ───────────────────────────────────────
      if(typeof target!=='object'||target===null)
        storm('TYPE_STORM',`Cannot call method "${node.methodName}" on a non-struct value`,node.line,node.column);
      const typeName=target.__structType;
      if(!typeName)
        storm('TYPE_STORM',`Cannot determine type for method call "${node.methodName}" — target is not a tagged struct`,node.line,node.column);
      const typeMap=this.typeMethods.get(typeName);
      if(!typeMap||!typeMap.has(node.methodName))
        storm('MISSING_STORM',`Type "${typeName}" has no method "${node.methodName}"`,node.line,node.column);
      const method=typeMap.get(node.methodName);
      const argVals=(node.args||[]).map(a=>this.evaluateExpressionNode(a,soil));
      const result=this._callAction(method,argVals,target,soil);
      return result.value!==undefined?result.value:null;
    }

    if(node.type==='MemberAccess'){
      const obj=this.evaluateExpressionNode(node.object,soil);
      // CHOICE variant construction without args: Option.None
      if(typeof obj==='object'&&obj!==null&&obj.__choiceType){
        const choiceName=obj.__choiceType;
        const choiceDef=this.choices.get(choiceName);
        if(choiceDef){
          const variant=choiceDef.find(v=>v.name.toUpperCase()===node.member.toUpperCase());
          if(variant)return{__choiceType:choiceName,tag:variant.name,payload:variant.type?null:null};
        }
      }
      if(typeof obj!=='object'||obj===null||Array.isArray(obj))
        storm('TYPE_STORM',`Cannot access member "${node.member}" on a non-struct value`,node.line,node.column);
      if(!(node.member in obj))
        storm('MISSING_STORM',`Struct has no field "${node.member}"`,node.line,node.column);
      return obj[node.member];
    }
    if(node.type==='StructInstantiation'){
      const structDef=this.structs.get(node.structName);
      if(!structDef)
        storm('MISSING_STORM',`Struct "${node.structName}" is not defined`,node.line,node.column);
      const instance={};
      for(let i=0;i<structDef.length;i++){
        const field=structDef[i];
        const argNode=node.args[i];
        let val;
        if(argNode) val=this.evaluateExpressionNode(argNode,soil);
        if(val===undefined||val===null)
          val=field.varType==='NUM'?0:field.varType==='SCL'?0.0:field.varType==='FACT'?false:field.varType==='TX'?'':field.varType==='LIST'?[]:isMapTypeStr(field.varType)?new Map():{};
        instance[field.name]=val;
      }
      instance.__structType=node.structName;
      return instance;
    }
    if(node.type==='StructLiteral'){
      const structDef=node.structName?this.structs.get(node.structName):null;
      const instance={};
      for(const field of node.fields){
        let val=this.evaluateExpressionNode(field.value,soil);
        if(val===undefined||val===null&&structDef){
          const fd=structDef.find(f=>f.name===field.name);
          if(fd) val=fd.varType==='NUM'?0:fd.varType==='SCL'?0.0:fd.varType==='FACT'?false:fd.varType==='TX'?'':fd.varType==='LIST'?[]:isMapTypeStr(fd.varType)?new Map():{};
        }
        instance[field.name]=val;
      }
      if(node.structName)instance.__structType=node.structName;
      return instance;
    }
    if(node.type==='BloomExpression'){
      const spec=this.species.get(node.speciesName);
      if(!spec)storm('MISSING_STORM',`SPECIES "${node.speciesName}" not defined`,node.line,node.column);
      const inst={__species:node.speciesName};
      for(const[fname,fdef]of Object.entries(spec.fields)){
        inst[fname]=fdef.default!==null?fdef.default:(fdef.type==='NUM'?0:fdef.type==='FACT'?false:fdef.type==='LIST'?[]:fdef.type==='MAP'?{}:'');
      }
      inst.__actions=spec.actions;
      return inst;
    }
    if(node.type==='SelfExpression'){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF not available',node.line,node.column);
      return selfEntry.value;
    }
    // v0.44.0: Interpolated strings
    if(node.type==='InterpolatedString'){
      let result='';
      for(const seg of node.segments){
        if(seg.type==='text') result+=seg.value;
        else if(seg.node){
          const val=this.evaluateExpressionNode(seg.node,soil);
          result+=String(val);
        }
      }
      return result;
    }
    // v0.44.0: Option constructors
    if(node.type==='OptionConstruct'){
      const val=node.value?this.evaluateExpressionNode(node.value,soil):null;
      return{__choiceType:'Option',tag:node.variant,payload:val};
    }
    // v0.44.0: Result constructors
    if(node.type==='ResultConstruct'){
      const val=this.evaluateExpressionNode(node.value,soil);
      return{__choiceType:'Result',tag:node.variant,payload:val};
    }
    // v0.44.0: Range expressions
    if(node.type==='RangeExpression'){
      const start=this.evaluateExpressionNode(node.start,soil);
      const end=this.evaluateExpressionNode(node.end,soil);
      const arr=[];
      for(let i=start;i<end;i++) arr.push(i);
      return arr;
    }
    // v0.44.0: Slice expressions
    if(node.type==='SliceExpression'){
      const target=this.evaluateExpressionNode(node.target,soil);
      const start=node.start!==null?this.evaluateExpressionNode(node.start,soil):0;
      const end=node.end!==null?this.evaluateExpressionNode(node.end,soil):target.length;
      if(typeof target==='string') return target.slice(start,end);
      if(Array.isArray(target)) return target.slice(start,end);
      storm('TYPE_STORM','Slice requires string or array',node.line,node.column);
    }
    // v0.44.0: BinaryOp
    if(node.type==='BinaryOp'){
      const left=this.evaluateExpressionNode(node.left,soil);
      const right=this.evaluateExpressionNode(node.right,soil);
      switch(node.operator){
        case'+':return left+right;
        case'-':return left-right;
        case'*':return left*right;
        case'/':return(left/right)|0;
        case'%':return left%right;
        case'**':return Math.pow(left,right);
        case'IS':return left===right;
        case'IS_NOT':return left!==right;
        case'GREATER_THAN':return left>right;
        case'LESS_THAN':return left<right;
        case'GTE':return left>=right;
        case'LTE':return left<=right;
        case'AND':return left&&right;
        case'OR':return left||right;
        default:storm('TYPE_STORM',`Unknown operator: ${node.operator}`,node.line,node.column);
      }
    }
    // v0.44.0: UnaryOp
    if(node.type==='UnaryOp'){
      const operand=this.evaluateExpressionNode(node.operand,soil);
      switch(node.operator){
        case'NOT':return!operand;
        case'-':return-operand;
        default:storm('TYPE_STORM',`Unknown unary operator: ${node.operator}`,node.line,node.column);
      }
    }
    // v0.44.0: MatchExpr (MATCH as expression)
    if(node.type==='MatchExpr'){
      return this._evaluateMatchExpr(node,soil);
    }
    // Fallback: a raw string slipped through (shouldn't normally happen
    // once parseExpressionSpan always wraps in a Literal/Identifier).
    if(typeof node==='string')return evalExpr(node,soil);
    return null;
  }

  /** Central node router — delegator-executes each AST statement kind. */
  /**
   * Central node router — delegator-executes each AST statement kind.
   *
   * Mirrors the legacy engine's universal location-backfill safety net
   * (see _execOne's catch block, which stamps any location-less
   * PlantStorm with the current statement's line/column before it
   * propagates further). Storms thrown deep inside evalExpr/evalCond
   * (core/evaluator.js) or Soil mutations (core/runtime.js) — such as
   * ZERO_STORM from a bare division — carry no coordinates of their
   * own, since those layers have no access to source position. This
   * try/catch is the AST pipeline's single choke-point equivalent:
   * every statement node passes through here exactly once, so this is
   * where any coordinate-less PlantStorm gets backfilled with the
   * CURRENT NODE's own precise {line, column} (captured from its
   * origin token at parse time) before continuing to propagate — this
   * is what keeps the terminal caret (^) correctly aligned even for
   * storms raised arbitrarily deep inside a WEATHER/SHELTER body.
   */
  evaluateNode(node,soil){
    try{
      return this._evaluateNodeDispatch(node,soil);
    }catch(e){
      if(e instanceof PlantStorm&&(e.line===undefined||e.line===null)){
        e.line=node.line;e.column=node.column;
      }
      throw e;
    }
  }

  _evaluateNodeDispatch(node,soil){
    switch(node.type){
      case 'CreateStatement': return this.evaluateCreateStatement(node,soil);
      case 'ShowStatement':   return this.evaluateShowStatement(node,soil);
      case 'ListenBranchStatement': return this.evaluateListenBranch(node,soil);
      case 'ResponseStatement': return this.evaluateResponseStatement(node,soil);
      case 'WeatherStatement': return this.evaluateWeatherStatement(node,soil);
      case 'ImportStatement':   return this.evaluateImportStatement(node,soil);
      case 'ActionDeclaration': return this.evaluateActionDeclaration(node,soil);
      case 'SpeciesDeclaration': return this.evaluateSpeciesDeclaration(node,soil);
      case 'BloomStatement': return this.evaluateBloomStatement(node,soil);
      case 'TapStatement': return this.evaluateTapStatement(node,soil);
      case 'WheneverStatement': return this.evaluateWheneverStatement(node,soil);
      case 'ReapStatement':     return this.evaluateReapStatement(node,soil);
      case 'SetStatement':      return this.evaluateSetStatement(node,soil);
      case 'IncreaseStatement': return this.evaluateIncreaseStatement(node,soil);
      case 'DecreaseStatement': return this.evaluateDecreaseStatement(node,soil);
      // ── new nodes added in full-migration pass ──────────────
      case 'IfStatement':       return this.evaluateIfStatement(node,soil);
      case 'ForInStatement':    return this.evaluateForInStatement(node,soil);
      case 'CycleStatement':    return this.evaluateCycleStatement(node,soil);
      case 'SeasonStatement':   return this.evaluateSeasonStatement(node,soil);
      case 'MatchStatement':    return this.evaluateMatchStatement(node,soil);
      case 'GiveStatement':     return this.evaluateGiveStatement(node,soil);
      case 'StopIfStatement':   return this.evaluateStopIfStatement(node,soil);
      case 'PutStatement':      return this.evaluatePutStatement(node,soil);
      case 'TakeStatement':     return this.evaluateTakeStatement(node,soil);
      case 'LinkStatement':     return this.evaluateLinkStatement(node,soil);
      case 'SortStatement':     return this.evaluateSortStatement(node,soil);
      case 'ShakeStatement':    return this.evaluateShakeStatement(node,soil);
      case 'EvaporateStatement':return this.evaluateEvaporateStatement(node,soil);
      case 'LockStatement':     return this.evaluateLockStatement(node,soil);
      case 'BraidStatement':    return this.evaluateBraidStatement(node,soil);
      case 'MethodCallStatement': return this._evalMethodCallStatement(node,soil);
      case 'HarvestStatement':  return this.evaluateHarvestStatement(node,soil);
      case 'AnalyzeStatement':  return this.evaluateAnalyzeStatement(node,soil);
      case 'WaitStatement':     return this.evaluateWaitStatement(node,soil);
      case 'VerifyStatement':   return this.evaluateVerifyStatement(node,soil);
      case 'SuiteStatement':    return this.evaluateSuiteStatement(node,soil);
      case 'PlantStatement':    return this.evaluatePlantStatement(node,soil);
      case 'StructDeclaration': return this.evaluateStructDeclaration(node,soil);
      case 'VariantDeclaration': return this.evaluateVariantDeclaration(node,soil);
      case 'MissionStatement':  return this.evaluateMissionStatement(node,soil);
      case 'RootStatement':     return this.evaluateRootStatement(node,soil);
      case 'RootScopeStatement':return this.evaluateRootScopeStatement(node,soil);
      case 'ShowVerifySummary': return this.evaluateShowVerifySummary(node,soil);
      case 'CycleInStatement':    return evaluateCycleInStatement(node, this, soil);
      case 'SortStatementV2':     return evaluateSortStatement(node, this, soil);
      case 'BloomAsStatement':    return evaluateBloomAsStatement(node, this, soil);
      case 'BreakStatement':      throw new BreakSignalException();
      case 'ContinueStatement':   throw new ContinueSignalException();
      case 'EndBlock':            return null;  // silent no-op
      case 'BranchElse':          return null;  // silent no-op
      case 'BlockDelimiter':      return null;  // silent no-op
      case 'RawStatement':        return null;  // silent no-op
      case 'ConstDeclaration':
        return this.evaluateConstDeclaration(node, soil);
      case 'EnumDeclaration':
        return this.evaluateEnumDeclaration(node, soil);
      case 'TypeAliasDeclaration':
        return this.evaluateTypeAliasDeclaration(node, soil);
      case 'DestructDeclaration':
        return this.evaluateDestructDeclaration(node, soil);
      default:
        storm('SYNTAX_STORM',`No evaluator registered for AST node type "${node.type}"`,node.line,node.column);
    }
  }

  /** evaluateCreateStatement(node, soil) — CREATE ident(TYPE) TO expr. */
  evaluateCreateStatement(node,soil){
    // Check if this is a struct type
    const structDef = this.structs.get(node.varType);
    if (structDef) {
      return this.evaluateCreateStruct(node, soil, structDef);
    }

    let value;
    if(node.varType==='LIST'&&node.valueExpr!==null&&node.valueExpr.literalType==='RAW_EXPR'){
      // Replicate the legacy engine's special-case LIST parsing exactly:
      // "CREATE x(LIST) TO a, b, c." splits the raw comma-joined text and
      // coerces each item independently — a single compound expression
      // (e.g. "a + b") would otherwise be misinterpreted as one giant
      // string by the generic RAW_EXPR evaluator path below.
      value=node.valueExpr.value.split(',').map(v=>coerce(v.trim())).filter(v=>v!=='');
    }else if(node.varType && isArrayTypeStr(node.varType) && node.valueExpr && node.valueExpr.type==='ArrayLiteral'){
      value=this.evaluateArrayLiteral(node.valueExpr,soil);
    }else if(node.valueExpr!==null){
      value=this.evaluateExpressionNode(node.valueExpr,soil);
    }else{
      value=node.varType==='LIST'?[]:isMapTypeStr(node.varType)?new Map():node.varType==='MAP'?{}:node.varType==='FACT'?false:node.varType==='NUM'?0:'';
    }
    soil.set(node.identifier,value,node.varType||(value&&value.__species?'INSTANCE':null),{pulse:!!node.isPulse});
    const display = Array.isArray(value) ? '[' + value.join(', ') + ']' :
      value instanceof Map ? '{ ' + [...value.entries()].map(([k,v]) => (typeof k === 'string' ? '"' + k + '"' : String(k)) + ': ' + (typeof v === 'string' ? '"' + v + '"' : String(v))).join(', ') + ' }' :
      String(value);
    this.emit(`CREATE "${node.identifier}"(${node.varType})${node.isPulse?' PULSE':''} = ${display}`,'ok');
    return{next:1};
  }

  /** Evaluate an array literal into a JS array. */
  evaluateArrayLiteral(node, soil) {
    if (!node || node.type !== 'ArrayLiteral') return [];
    return node.elements.map(el => this.evaluateExpressionNode(el, soil));
  }

  /** evaluateCreateStruct(node, soil, structDef) — CREATE instance of a SHAPE. */
  evaluateCreateStruct(node, soil, structDef){
    const instance = { __structType: node.varType };
    const ve = node.valueExpr;
    if (ve && ve.type === 'StructInstantiation') {
      const args = ve.args || [];
      for (let i = 0; i < structDef.length; i++) {
        const field = structDef[i];
        const argNode = args[i];
        let val;
        if (argNode) {
          val = this.evaluateExpressionNode(argNode, soil);
        }
        if (val === undefined || val === null) {
          val = field.varType === 'NUM' ? 0 : field.varType === 'SCL' ? 0.0 : field.varType === 'FACT' ? false : field.varType === 'TX' ? '' : field.varType === 'LIST' ? [] : isMapTypeStr(field.varType) ? new Map() : null;
        }
        instance[field.name] = val;
      }
    } else if (ve && ve.type === 'StructLiteral') {
      for (const fv of ve.fields) {
        const fieldDef = structDef.find(f => f.name === fv.name);
        if (!fieldDef) {
          storm('TYPE_STORM', `CREATE "${node.identifier}": struct ${node.varType} has no field "${fv.name}"`, node.line, node.column);
        }
        instance[fv.name] = this.evaluateExpressionNode(fv.value, soil);
      }
      // Fill missing fields with defaults
      for (const field of structDef) {
        if (!(field.name in instance)) {
          instance[field.name] = field.varType === 'NUM' ? 0 : field.varType === 'SCL' ? 0.0 : field.varType === 'FACT' ? false : field.varType === 'TX' ? '' : field.varType === 'LIST' ? [] : isMapTypeStr(field.varType) ? new Map() : null;
        }
      }
    } else {
      for (const field of structDef) {
        instance[field.name] = field.varType === 'NUM' ? 0 : field.varType === 'SCL' ? 0.0 : field.varType === 'FACT' ? false : field.varType === 'TX' ? '' : field.varType === 'LIST' ? [] : isMapTypeStr(field.varType) ? new Map() : null;
      }
    }
    // v0.38.0: nested struct — check for child struct instances
    if (this._missionMode === 'PERSISTENT') {
      // For PERSISTENT, allocate in ARC heap
      // (this is a simplified integration; full ARC integration is per-session)
    }
    soil.set(node.identifier, instance, node.varType);
    this.emit(`CREATE "${node.identifier}"(${node.varType}) = ${JSON.stringify(instance)}`,'ok');
    return{next:1};
  }

  /** evaluateShowStatement(node, soil) — SHOW expr. */
  evaluateShowStatement(node,soil){
    let value=this.evaluateExpressionNode(node.expr,soil);
    let display;
    if (Array.isArray(value)) {
      display = '[' + value.map(v => typeof v === 'string' ? '"' + v + '"' : String(v)).join(', ') + ']';
    } else if (value instanceof Map) {
      const entries = [];
      for (const [k, v] of value) {
        entries.push((typeof k === 'string' ? '"' + k + '"' : String(k)) + ': ' + (typeof v === 'string' ? '"' + v + '"' : String(v)));
      }
      display = '{ ' + entries.join(', ') + ' }';
    } else if (value && typeof value === 'object' && !Array.isArray(value)) {
      // v0.38.0: nested struct formatting
      const formatted = formatShowValue(value);
      this.emit(formatted, 'SHOW');
      return{next:1};
    } else {
      display = value && typeof value === 'object' ? (Array.isArray(value) ? '[' + value.join(', ') + ']' : JSON.stringify(value)) : String(value);
    }
    this.emit(display,'inf');
    return{next:1};
  }

  /** evaluateListenBranch(node, soil) — bridges the AST node to the existing handler logic. */
  evaluateListenBranch(node,soil){
    const http=require('http');
    const E=expr=>evalExpr(expr,soil);

    // ── VERIFY / dry-run mode ─────────────────────────────────────
    // When running inside a VERIFY suite, execute body once synchronously
    // with an empty request so grammar/logic tests pass without a real server.
    if(this._verifyDryRun||this.verifyStats.suite!==null){
      const reqMap={method:'GET',path:'/',query:{},headers:{},body:{},raw:''};
      const reqSoil=soil.child();
      reqSoil.set(node.requestIdent,reqMap,'MAP');
      const port=E(node.portExpr)||3000;
      this.emit(`LISTEN BRANCH ON ${port} WITH ${node.configExpr} AS ${node.requestIdent} MAP — dry-run ✓`,'ok');
      return this._evalBody(node.bodyStatements,reqSoil);
    }

    // ── Real HTTP server ──────────────────────────────────────────
    const port=+E(node.portExpr)||3000;
    const cfgEntry=node.configExpr?soil.get(node.configExpr):null;
    const cfg=(cfgEntry&&typeof cfgEntry.value==='object')?cfgEntry.value:{};
    const requestTimeout=+(cfg.timeout||cfg.TIMEOUT||30)*1000;
    const hostname=cfg.host||cfg.HOST||'0.0.0.0';
    const self=this;

    this.emit(`LISTEN BRANCH ON ${port} — starting server…`,'ok');

    const server=http.createServer((req,res)=>{
      let rawBody='';
      req.setTimeout(requestTimeout,()=>req.destroy());
      req.on('data',chunk=>rawBody+=chunk);
      req.on('end',()=>{
        let parsedBody=rawBody;
        const ct=req.headers['content-type']||'';
        if(ct.includes('application/json')&&rawBody){
          try{parsedBody=JSON.parse(rawBody);}catch(_){parsedBody=rawBody;}
        }
        const url=new URL(req.url,'http://localhost');
        const queryMap={};
        url.searchParams.forEach((v,k)=>queryMap[k]=v);
        const reqMap={
          method:req.method,path:url.pathname,
          query:queryMap,headers:req.headers,
          body:typeof parsedBody==='object'?parsedBody:rawBody,raw:rawBody,
        };
        const reqSoil=soil.child();
        reqSoil.set(node.requestIdent,reqMap,'MAP');

        // Capture GIVE expr AS RESPONSE via override
        let responseValue=null,responseStatus=200;
        const responseHeaders={'Content-Type':'application/json'};
        const origResp=self.evaluateResponseStatement.bind(self);
        self.evaluateResponseStatement=function(rNode,rSoil){
          responseValue=evalExpr(rNode.responseExpr,rSoil);
          self.evaluateResponseStatement=origResp;
          return{next:1,returned:true,value:responseValue};
        };

        try{
          // Run body via evaluateNode so ResponseStatement override fires
          for(const bodyNode of (node.bodyStatements||[])){
            const r=self.evaluateNode(bodyNode,reqSoil);
            if(r&&r.returned){
              // If returned but responseValue still null, check r.value
              if(responseValue===null&&r.value!==undefined)responseValue=r.value;
              break;
            }
          }
        }catch(e){
          self.evaluateResponseStatement=origResp;
          if(e&&e.stormType==='STOP_STORM'){responseStatus=204;}
          else{
            self.emit(`⚡ Request handler error: ${e.message||e}`,'error');
            responseStatus=500;responseValue={error:String(e.message||e)};
          }
        }

        let body;
        if(responseValue===null||responseValue===undefined){
          body='';responseStatus=204;
        }else if(typeof responseValue==='string'){
          body=responseValue;responseHeaders['Content-Type']='text/plain; charset=utf-8';
        }else if(typeof responseValue==='object'){
          body=JSON.stringify(responseValue);
        }else{
          body=String(responseValue);responseHeaders['Content-Type']='text/plain; charset=utf-8';
        }
        res.writeHead(responseStatus,responseHeaders);
        res.end(body);
        self.emit(`  ${req.method} ${url.pathname} → ${responseStatus}`,'muted');
      });
      req.on('error',()=>{});
    });

    server.on('error',e=>this.emit(`⛈️  LISTEN BRANCH: server error — ${e.message}`,'error'));
    server.listen(port,hostname,()=>this.emit(`✓ LISTEN BRANCH listening on ${hostname}:${port}`,'ok'));
    this._activeServer=server;

    const shutdown=signal=>{
      this.emit(`\nLISTEN BRANCH — shutting down (${signal})…`,'muted');
      server.close(()=>process.exit(0));
      setTimeout(()=>process.exit(0),1000);
    };
    process.once('SIGINT',()=>shutdown('SIGINT'));
    process.once('SIGTERM',()=>shutdown('SIGTERM'));
    return null;
  }

  _evalBody(nodes,soil){
    for(const n of (nodes||[])){
      const r=this.evaluateNode(n,soil);
      if(r&&r.returned)return r;
    }
    return null;
  }



  /** evaluateResponseStatement(node, soil) — GIVE expr AS RESPONSE. */
  evaluateResponseStatement(node,soil){
    const value=evalExpr(node.responseExpr,soil);
    this.emit(`  → RESPONSE: ${value&&typeof value==='object'?JSON.stringify(value):value}`,'muted');
    return{next:1,returned:true,value};
  }

  /**
   * evaluateWeatherStatement(node, soil) — WEATHER [IF cond] / SHELTER / CALM.
   *
   * Execution model (matches the legacy regex-engine's proven semantics
   * for the unconditional case, see core/interpreter.js's earlier
   * `stmt.match(/^WEATHER$/i)` handler which this AST path runs
   * alongside, not in place of; the conditional "WEATHER IF" form is new
   * in this milestone and fully backward-compatible — every existing
   * bare "WEATHER," statement still has `conditionExpr === null` and
   * behaves byte-for-byte identically to before):
   *
   *   0. If node.conditionExpr is non-null, it is evaluated against the
   *      OUTER soil (not the sandboxed weatherSoil — the condition is a
   *      guard checked before the protected block's own scope even
   *      exists, so it can only see variables already in scope at the
   *      WEATHER statement's own position, matching how IF/STOP IF
   *      conditions are evaluated elsewhere in the language). If the
   *      condition evaluates falsy, the protected body and all SHELTER
   *      clauses are skipped entirely — no storm was ever thrown, so
   *      there is nothing to catch, and entering SHELTER resolution
   *      would be meaningless. CALM still runs unconditionally (see
   *      step 5) since it is the block's structural terminator, not
   *      part of the conditional protected/recovery logic.
   *   1. The protected body runs inside its OWN child Soil scope, so any
   *      CREATE/SET done there does not leak into the enclosing scope —
   *      this is the "execution state sandboxing" required by the task.
   *   2. If the body throws a PlantStorm, it is matched against the
   *      WEATHER's shelterClauses by `stormType` (case-insensitive on
   *      the stored uppercase type), falling back to a clause whose
   *      stormType is "ANY_STORM" if no exact match exists.
   *   3. The matching SHELTER clause's body also runs in its own child
   *      Soil (sandboxed from both the WEATHER body and the outer
   *      scope), with `errVar` (if specified) bound to the storm's
   *      message text — mirroring the legacy engine's `hs.set(errVar,
   *      e.message,'TX')` behavior exactly.
   *   4. Any PlantStorm with no matching SHELTER clause propagates
   *      upward uncaught, exactly like the legacy engine, so it still
   *      reaches the centralized diagnostic handler in _exec/_execOne
   *      and gets the visual caret pointer instead of being silently
   *      swallowed.
   *   5. evaluateCalmStatement is invoked unconditionally once the
   *      WEATHER/SHELTER resolution above completes (success, recovered
   *      failure, or the condition-skip path) — it is currently a no-op
   *      terminator per the existing grammar, called for forward-
   *      compatibility and to keep CalmStatementNode's evaluation
   *      symmetrical with its sibling node types.
   */
  evaluateWeatherStatement(node,soil){
    if(node.conditionExpr!==null){
      const conditionValue=evalCond(node.conditionExpr,soil);
      if(!conditionValue){
        if(node.calmClause)this.evaluateCalmStatement(node.calmClause,soil);
        return{next:1};
      }
    }
    const weatherSoil=soil.child(); // sandbox: body-local CREATE/SET never leak upward
    let result=null;
    try{
      for(const bodyNode of node.bodyStatements){
        const r=this.evaluateNode(bodyNode,weatherSoil);
        if(r&&r.returned){result=r;break;}
      }
    }catch(e){
      if(!(e instanceof PlantStorm))throw e;
      const clause=node.shelterClauses.find(c=>c.stormType===e.stormType)
                 ||node.shelterClauses.find(c=>c.stormType==='ANY_STORM');
      if(clause){
        result=this.evaluateShelterStatement(clause,soil,e);
      }else{
        // No matching SHELTER — propagate, exactly like the legacy engine,
        // so the central diagnostic handler still renders the caret.
        throw e;
      }
    }
    if(node.calmClause)this.evaluateCalmStatement(node.calmClause,soil);
    return result&&result.returned?result:{next:1};
  }

  /**
   * evaluateShelterStatement(clauseNode, soil, caughtStorm) — runs a
   * single SHELTER clause's recovery body in its own sandboxed child
   * Soil, binding `errVar` to the caught storm's message text if the
   * clause declared one (e.g. "SHELTER ZERO_STORM AS err,"). Called
   * internally by evaluateWeatherStatement once a matching clause has
   * been selected — not reached directly via evaluateNode()'s router,
   * since a bare SHELTER outside its owning WEATHER has no standalone
   * meaning (matching the legacy engine, which also treats a
   * stray SHELTER/CALM line as a structural no-op — see _execOne's
   * `if(text.match(/^SHELTER\b/i)...)return{next:i+1}` skip rule).
   */
  evaluateShelterStatement(clauseNode,soil,caughtStorm){
    const shelterSoil=soil.child(); // sandbox: recovery-body locals never leak upward
    if(clauseNode.errVar)shelterSoil.set(clauseNode.errVar,caughtStorm.message,'TX');
    let result=null;
    for(const bodyNode of clauseNode.bodyStatements){
      const r=this.evaluateNode(bodyNode,shelterSoil);
      if(r&&r.returned){result=r;break;}
    }
    return result;
  }

  /**
   * evaluateCalmStatement(node, soil) — terminates a WEATHER/SHELTER
   * chain. Per the current grammar, CALM carries no body of its own
   * (CalmStatementNode.bodyStatements is always empty today — see
   * core/ast.js's documentation on the reserved future extension), so
   * this is intentionally a no-op pass-through. It still exists as a
   * named evaluator (rather than being skipped silently) so the AST
   * execution pipeline remains symmetrical and self-documenting, and
   * so a future "finally"-style CALM body can be wired in here without
   * touching evaluateWeatherStatement's call site.
   */
  evaluateCalmStatement(node,soil){
    for(const bodyNode of node.bodyStatements){
      this.evaluateNode(bodyNode,soil);
    }
    return{next:1};
  }

  /**
   * evaluateStructDeclaration(node, soil) — registers a SHAPE definition.
   */
  evaluateStructDeclaration(node,soil){
    this.structs.set(node.name, node.fields);
    this.emit(`SHAPE "${node.name}" { ${node.fields.map(f=>f.name+'('+f.varType+')').join(', ')} }`,'ok');
    return{next:1};
  }

  evaluateVariantDeclaration(node,soil){
    this.choices.set(node.name, node.variants);
    this.emit(`CHOICE "${node.name}" { ${node.variants.map(v=>v.name+(v.type?'('+v.type+')':'')).join(', ')} }`,'ok');
    return{next:1};
  }

  /**
   * evaluateImportStatement(node, soil) — resolves and executes an
   * imported file.  Merges the imported file's declarations (actions,
   * species) into this interpreter's symbol tables.
   */
  evaluateImportStatement(node, soil) {
    const importRelPath = node.path;
    let resolvedPath;
    // Standard library resolution
    if (importRelPath.startsWith('std/') || importRelPath.startsWith('std\\')) {
      const relativePart = importRelPath.replace(/^std[/\\]/, '');
      const stdDir = path.join(__dirname, '..', 'std');
      resolvedPath = path.resolve(stdDir, relativePart);
    } else if (path.isAbsolute(importRelPath)) {
      resolvedPath = path.normalize(importRelPath);
    } else {
      resolvedPath = path.resolve(this.rootDir || process.cwd(), importRelPath);
    }
    // Add .plnt extension if missing
    if (!resolvedPath.endsWith('.plnt')) resolvedPath += '.plnt';
    resolvedPath = path.normalize(resolvedPath);
    if (!fs.existsSync(resolvedPath)) {
      const alt = resolvedPath.replace(/\.plnt$/, '');
      if (fs.existsSync(alt)) resolvedPath = alt;
      else {
        storm('MISSING_STORM',
          `Import file not found: "${importRelPath}" (looked at ${resolvedPath})`,
          node.line, node.column);
      }
    }
    // Avoid re-importing the same file
    if (this._importedFiles && this._importedFiles.has(resolvedPath)) return null;
    if (!this._importedFiles) this._importedFiles = new Set();
    this._importedFiles.add(resolvedPath);

    // Parse and resolve the imported file
    const source = fs.readFileSync(resolvedPath, 'utf-8');
    const { parseFile } = require('./parser');
    const importedProgram = parseFile(resolvedPath, { noPrelude: true });
    // Register imported declarations, then execute
    this.symbolPass(importedProgram);
    this.runProgram(importedProgram);
    return null;
  }

  /**
   * evaluateActionDeclaration(node, soil) — registers the ACTION into
   * this.funcs using EXACTLY the same {params, body, line} shape the
   * legacy _firstPass() uses, with one key addition: body is an array
   * of AST nodes (rather than legacy text records). _callAction() is
   * already polymorphic (see its implementation) so all existing call
   * sites (REAP, FLOW, recursion) work unchanged without modification.
   */
  evaluateActionDeclaration(node,soil){
    // FFI external actions: register with external flag; body is empty
    if(node.isExternal){
      if(node.receiver){
        // External receiver action (unusual but handle gracefully)
        this._registerTypeMethod(node);
      } else {
        this.funcs.set(node.name,{
          name:node.name,
          params:node.params,
          body:[],
          line:node.line,
          isExternal:true
        });
        if(!this._symbolPassDone)
          this.emit(`✓ ACTION "${node.name}"(${node.params.map(p=>p.name+'('+p.type+')').join(', ')}) -> external`,'ok');
      }
      return{next:1};
    }
    // Receiver-bound method: register in typeMethods
    if(node.receiver){
      this._registerTypeMethod(node);
      return{next:1};
    }
    this.funcs.set(node.name,{
      params:node.params,
      body:node.bodyStatements,
      line:node.line
    });
    if(!this._symbolPassDone)
      this.emit(`✓ ACTION "${node.name}"(${node.params.map(p=>p.name+'('+p.type+')').join(', ')})`,'ok');
    return{next:1};
  }

  _registerTypeMethod(node){
    const typeName = node.receiver.type;
    if(!this.typeMethods.has(typeName)) this.typeMethods.set(typeName, new Map());
    this.typeMethods.get(typeName).set(node.name, {
      params: node.params,
      body: node.bodyStatements || [],
      line: node.line,
      isExternal: !!node.isExternal,
      receiverName: node.receiver.name,
    });
    if(!this._symbolPassDone)
      this.emit(`✓ METHOD "${typeName}.${node.name}"(${node.params.map(p=>p.name+'('+p.type+')').join(', ')})`,'ok');
  }

  /**
   * evaluateSpeciesDeclaration(node, soil) — registers the SPECIES into
   * this.species, replicating the exact shape the legacy _firstPass()
   * produces: {fields:{name:{type,default}}, actions:{name:{params,body}},
   * parent}. PARENT inheritance deep-clones the parent's fields/actions
   * first, then the child's own fields/actions overlay — identical to
   * the legacy engine's JSON.parse(JSON.stringify(...)) deep-clone.
   */
  evaluateSpeciesDeclaration(node,soil){
    const fields={},actions={};
    if(node.parentName&&this.species.has(node.parentName)){
      const p=this.species.get(node.parentName);
      Object.assign(fields,JSON.parse(JSON.stringify(p.fields)));
      Object.assign(actions,JSON.parse(JSON.stringify(p.actions)));
    }
    for(const f of node.fields){
      const dflt=f.defaultExpr!==null
        ? this.evaluateExpressionNode(f.defaultExpr,soil)
        : (f.varType==='NUM'?0:f.varType==='FACT'?false:f.varType==='LIST'?[]:f.varType==='MAP'?{}:'');
      fields[f.name]={type:f.varType,default:dflt};
    }
    for(const a of node.actions){
      actions[a.name]={params:a.params,body:a.bodyStatements};
    }
    this.species.set(node.name,{fields,actions,parent:node.parentName});
    if(!this._symbolPassDone)
      this.emit(`✓ SPECIES "${node.name}"${node.parentName?' PARENT '+node.parentName:''}`,'ok');
    return{next:1};
  }

  /**
   * evaluateBloomStatement(node, soil) — instantiates a SPECIES,
   * creating a concrete instance object {__species, field1, field2, ...}
   * and binding it to `node.instanceIdent` in the current scope. Mirrors
   * the legacy BLOOM handler's exact instantiation logic (see line ~873
   * in _exec's BLOOM handler) so all existing SELF:/method invocation
   * code in _callAction keeps working with instances created via this
   * new AST path.
   */
  evaluateBloomStatement(node,soil){
    const spec=this.species.get(node.speciesName);
    if(!spec)storm('MISSING_STORM',`SPECIES "${node.speciesName}" not defined`,node.line,node.column);
    const inst={__species:node.speciesName};
    const ownSpec={fields:spec.fields,actions:spec.actions,parent:spec.parent};
    for(const[fname,fdef]of Object.entries(ownSpec.fields)){
      inst[fname]=fdef.default!==null?fdef.default:(fdef.type==='NUM'?0:fdef.type==='FACT'?false:fdef.type==='LIST'?[]:fdef.type==='MAP'?{}:'');
    }
    inst.__actions=ownSpec.actions;
    soil.set(node.instanceIdent,inst,'INSTANCE');
    this.emit(`BLOOM "${node.speciesName}" AS "${node.instanceIdent}" ✓`,'ok');
    return{next:1};
  }

  /**
   * evaluateTapStatement(node, soil) — opens a file handle, delegating
   * to the existing VeinFS / TAP legacy handler via a synthetic
   * RawStatement (since TAP already has a complete, tested implementation
   * in _exec and VeinFS handles all file state; re-implementing would be
   * duplication without additional correctness gain).
   */
  evaluateTapStatement(node,soil){
    const synth={depth:node.depth||1,
      text:`TAP "${node.filename}" MODE:${node.mode} AS ${node.handleIdent}`,
      line:node.line,column:node.column};
    return this._execOne([synth],0,1,soil);
  }

  evaluateWheneverStatement(node,soil){
    // Build a synthetic body as legacy flat-statement records so the
    // existing watcher/WHENEVER infrastructure (_exec's WHENEVER handler
    // which populates this.watchers[]) keeps working unchanged.
    const bodyAsLegacy=node.bodyStatements.map(s=>({
      depth:s.depth||2,
      text:s.type==='RawStatement'?s.text:(s.text||''),
      line:s.line,column:s.column
    }));
    const synth={depth:node.depth||1,
      text:`WHENEVER ${node.watchIdent} CHANGES`,
      line:node.line,column:node.column};
    // Reconstruct the flat statement list that the legacy WHENEVER handler
    // expects: header + body records + closer (N\.)
    const closer={depth:node.depth||1,text:'\\\\',line:node.line,column:1};
    const allStmts=[synth,...bodyAsLegacy,closer];
    return this._execOne(allStmts,0,allStmts.length,soil);
  }

  // ── REAP evaluator ─────────────────────────────────────────────
  evaluateReapStatement(node,soil){
    const E=(expr)=>{
      try{return evalExpr(expr,soil);}
      catch(e){
        if(e instanceof PlantStorm&&e.line===undefined){e.line=node.line;e.column=node.column;}
        throw e;
      }
    };
    const splitArgs=(rawArr)=>rawArr.map(a=>E(a));
    const store=(val)=>{
      if(node.variable!=='_')soil.set(node.variable,val,inferType(val));
    };

    const {kind}=node.source;

    if(kind==='NOW'){
      const fmt=node.source.format||'FULL',now=new Date();
      const val=fmt==='DATE'?now.toLocaleDateString('en-US')
        :fmt==='TIME'?now.toLocaleTimeString('en-US')
        :fmt==='YEAR'?now.getFullYear()
        :fmt==='STAMP'?now.getTime()
        :now.toLocaleString('en-US');
      store(val); return{next:1};
    }

    if(kind==='TYPEOF'){
      const e=soil.get(node.source.target);
      store(e?e.type:'VOID'); return{next:1};
    }

    if(kind==='SELF'){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF not available',node.line,node.column);
      const sp=this.species.get(selfEntry.value.__species);
      if(!sp)storm('MISSING_STORM','SPECIES not found',node.line,node.column);
      const method=sp.actions[node.source.method];
      if(!method)storm('MISSING_STORM',`action "${node.source.method}" not found`,node.line,node.column);
      const result=this._callAction(method,splitArgs(node.args),selfEntry.value,soil);
      store(result.value!==undefined?result.value:null); return{next:1};
    }

    if(kind==='INSTANCE_OR_LIBRARY'){
      const {name,fn}=node.source;
      // Is it a planted library?
      if(this.planted.has(name)){
        const lib=this.planted.get(name);
        const libFn=lib[fn.toUpperCase()];
        if(!libFn)storm('MISSING_STORM',`"${fn}" not found in "${name}"`,node.line,node.column);
        store(libFn(splitArgs(node.args))); return{next:1};
      }
      // Is it an instance variable?
      const instEntry=soil.get(name);
      if(!instEntry)storm('MISSING_STORM',`"${name}" not found`,node.line,node.column);
      const spName=instEntry.value&&instEntry.value.__species;
      const sp=spName&&this.species.get(spName);
      if(!sp)storm('MISSING_STORM',`SPECIES not found`,node.line,node.column);
      const method=sp.actions[fn];
      if(!method)storm('MISSING_STORM',`action "${fn}" not found`,node.line,node.column);
      const result=this._callAction(method,splitArgs(node.args),instEntry.value,soil);
      store(result.value!==undefined?result.value:null); return{next:1};
    }

    if(kind==='ACTION'){
      const {name}=node.source;
      if(!this.funcs.has(name))storm('MISSING_STORM',`action "${name}" not defined`,node.line,node.column);
      const fn=this.funcs.get(name);
      const result=this._callAction(fn,splitArgs(node.args),null,soil);
      store(result.value!==undefined?result.value:null); return{next:1};
    }

    if(kind==='EXPR'){
      const val=this.evaluateExpressionNode(node.source.expr,soil);
      store(val); return{next:1};
    }

    storm('SEED_STORM',`Unknown REAP source kind: ${kind}`,node.line,node.column);
  }

  _evalMethodCallStatement(node,soil){
    // obj:method(args) — standalone dispatch
    const targetName=node.target&&node.target.name;
    if(!targetName)storm('SEED_STORM','Invalid method call target',node.line,node.column);
    const instEntry=soil.get(targetName);
    if(!instEntry)storm('MISSING_STORM',`"${targetName}" not found`,node.line,node.column);
    const inst=instEntry.value;
    const spName=inst&&inst.__species;
    const sp=spName&&this.species.get(spName);
    if(!sp)storm('MISSING_STORM','SPECIES not found',node.line,node.column);
    const methodDef=sp.actions[node.methodName];
    if(!methodDef)storm('MISSING_STORM',`action "${node.methodName}" not found on ${targetName}`,node.line,node.column);
    const splitArgs=(args)=>args.map(a=>evalExpr(a instanceof Object&&a.type?this.evaluateExpressionNode(a,soil):a,soil));
    const argVals=(node.args||[]).map(a=>{
      if(a&&a.type)return this.evaluateExpressionNode(a,soil);
      return evalExpr(a,soil);
    });
    const result=this._callAction(methodDef,argVals,inst,soil);
    return{next:1};
  }

  // ── SET / INCREASE / DECREASE evaluators ──────────────────────
  evaluateSetStatement(node,soil){
    // Struct member access: SET obj.field TO val
    if (node.isMemberAccess) {
      const objEntry = soil.get(node.memberObject);
      if (!objEntry) storm('MISSING_STORM',`SET: "${node.memberObject}" is not defined`,node.line,node.column);
      if (typeof objEntry.value !== 'object' || objEntry.value === null || Array.isArray(objEntry.value))
        storm('TYPE_STORM',`SET: "${node.memberObject}" is not a struct`,node.line,node.column);
      if (!(node.memberField in objEntry.value))
        storm('MISSING_STORM',`SET: struct "${node.memberObject}" has no field "${node.memberField}"`,node.line,node.column);
      const newVal = evalExpr(node.valueExpr, soil);
      objEntry.value[node.memberField] = newVal;
      // Fire PULSE watchers if any
      if (this.watchers.has(node.memberObject)) {
        for (const w of this.watchers.get(node.memberObject)) {
          this.evaluateNode(w, soil);
        }
      }
      return {next:1};
    }

    const E=(expr)=>evalExpr(expr,soil);
    const newVal=E(node.valueExpr);
    // Delegate to the proven legacy handler via a synthetic statement so
    // all edge cases (PULSE watchers, SELF:prop, obj:prop, locked vars)
    // are covered without duplication.
    const synth={depth:node.depth||1,
      text:`SET ${node.identifier} TO ${node.valueExpr}`,
      line:node.line,column:node.column};
    return this._execOne([synth],0,1,soil);
  }

  evaluateIncreaseStatement(node,soil){
    const synth={depth:node.depth||1,
      text:`INCREASE ${node.identifier} BY ${node.amountExpr}`,
      line:node.line,column:node.column};
    return this._execOne([synth],0,1,soil);
  }

  evaluateDecreaseStatement(node,soil){
    const synth={depth:node.depth||1,
      text:`DECREASE ${node.identifier} BY ${node.amountExpr}`,
      line:node.line,column:node.column};
    return this._execOne([synth],0,1,soil);
  }

  // ── New evaluators — bridge each AST node to proven legacy _exec ─────────

  _evalRaw(text,node,soil){
    const s=[{depth:node.depth||1,text,line:node.line,column:node.column}];
    return this._execOne(s,0,1,soil);
  }

  _evalBody(nodes,soil){
    for(const n of (nodes||[])){
      try {
        const r=this.evaluateNode(n,soil);
        if(r&&r.returned)return r;
      } catch (err) {
        if (err instanceof BreakSignalException || err instanceof ContinueSignalException) {
          throw err; // propagate to cycle_evaluator
        }
        throw err;
      }
    }
    return null;
  }

  evaluateIfStatement(node,soil){
    const C=cond=>evalCond(cond,soil);
    for(const branch of (node.branches||[])){
      if(branch.cond===null||C(branch.cond)){
        return this._evalBody(branch.bodyStatements,soil);
      }
    }
    return null;
  }

  evaluateCycleStatement(node,soil){
    const E=expr=>evalExpr(expr,soil);
    if(node.sourceExpr!==null){
      const listE=soil.get(node.sourceExpr);
      const list=listE&&Array.isArray(listE.value)?listE.value:
                 listE?[listE.value]:[E(node.sourceExpr)].flat();
      for(const item of list){
        const cs=soil.child();
        cs.set(node.iterVar,item,inferType(item));
        try {
          const r=this._evalBody(node.bodyStatements,cs);
          if(r&&r.returned)return r;
        } catch (err) {
          if (err instanceof ContinueSignalException) continue;
          if (err instanceof BreakSignalException) break;
          throw err;
        }
      }
    }else{
      const lo=+E(node.fromExpr)||0,hi=+E(node.toExpr)||0;
      const step=node.stepExpr?(+E(node.stepExpr)||1):(lo<=hi?1:-1);
      for(let i=lo;(step>0?i<=hi:i>=hi);i+=step){
        const cs=soil.child();
        cs.set(node.iterVar,i,'NUM');
        try {
          const r=this._evalBody(node.bodyStatements,cs);
          if(r&&r.returned)return r;
        } catch (err) {
          if (err instanceof ContinueSignalException) continue;
          if (err instanceof BreakSignalException) break;
          throw err;
        }
      }
    }
    return null;
  }

  evaluateForInStatement(node, soil) {
    const raw = soil.get(node.sourceExpr);
    if (!raw) storm('MISSING_STORM', `"${node.sourceExpr}" is not defined`, node.line, node.column);
    const collection = raw.value;
    const collType = raw.type;
    if (Array.isArray(collection)) {
      for (const item of collection) {
        const cs = soil.child();
        cs.set(node.iterVar, item, inferType(item));
        try {
          const r = this._evalBody(node.bodyStatements, cs);
          if (r && r.returned) return r;
        } catch (e) {
          if (e.name === 'STOP_STORM') return null;
          throw e;
        }
      }
      return null;
    }
    if (collection instanceof Map) {
      const mt = collType && isMapTypeStr(collType) ? mapInnerTypes(collType) : null;
      const valType = mt ? mt.valueType : null;
      for (const [key, val] of collection) {
        const cs = soil.child();
        cs.set(node.iterVar, val, valType || inferType(val));
        try {
          const r = this._evalBody(node.bodyStatements, cs);
          if (r && r.returned) return r;
        } catch (e) {
          if (e.name === 'STOP_STORM') return null;
          throw e;
        }
      }
      return null;
    }
    storm('TYPE_STORM', `FOR IN: "${node.sourceExpr}" is not iterable (not an array or MAP)`, node.line, node.column);
  }

  evaluateSeasonStatement(node,soil){
    const C=cond=>evalCond(cond,soil);
    while(C(node.condExpr)){
      try {
        const r=this._evalBody(node.bodyStatements,soil);
        if(r&&r.returned)return r;
      } catch (err) {
        if (err instanceof ContinueSignalException) continue;
        if (err instanceof BreakSignalException) break;
        throw err;
      }
    }
    return null;
  }

  evaluateMatchStatement(node,soil){
    // Legacy MATCH format (clauseText-based)
    if(node.clauses.length>0&&node.clauses[0].clauseText!==undefined){
      const clauses=(node.clauses||[]).map(c=>c.clauseText).join('\n');
      const fullSrc=`MATCH ${node.subjectExpr},\n${clauses}\n\\.`;
      const stmts=lex(fullSrc).map(s=>({...s,line:node.line,column:node.column}));
      return this._execBlock(stmts,0,stmts.length,soil);
    }
    // New pattern-matching MATCH (variantName-based)
    const subject=this.evaluateExpressionNode(node.subjectExpr,soil);
    if(typeof subject!=='object'||subject===null||!subject.__choiceType)
      storm('TYPE_STORM','MATCH requires a CHOICE value',node.line,node.column);

    for(const clause of node.clauses){
      if(clause.variantName===subject.tag){
        let clauseSoil=soil;
        if(clause.binding&&subject.payload!==undefined){
          clauseSoil=soil.child();
          clauseSoil.set(clause.binding,subject.payload);
        }
        for(const stmt of clause.bodyStatements){
          const r=this.evaluateNode(stmt,clauseSoil);
          if(r&&r.returned)return r;
        }
        return null;
      }
    }
    storm('SEED_STORM',`MATCH: no clause handles variant "${subject.tag}"`,node.line,node.column);
  }

  evaluateDestructDeclaration(node,soil){
    const sourceVal=this.evaluateExpressionNode(node.sourceExpr,soil);
    if(node.patternType==='object'){
      for(const field of node.pattern){
        const val=sourceVal[field];
        if(val===undefined)storm('MISSING_STORM',`Field "${field}" not found in source`,node.line,node.column);
        soil.set(field,val);
      }
    }else if(node.patternType==='array'){
      for(let i=0;i<node.pattern.length;i++){
        const field=node.pattern[i];
        const val=sourceVal[i];
        if(val===undefined)storm('MISSING_STORM',`Index ${i} not found in source array`,node.line,node.column);
        soil.set(field,val);
      }
    }
  }

  _evaluateMatchExpr(node,soil){
    const subject=this.evaluateExpressionNode(node.subjectExpr,soil);
    if(typeof subject!=='object'||subject===null||!subject.__choiceType)
      storm('TYPE_STORM','MATCH expression requires a CHOICE/Option/Result value',node.line,node.column);
    for(const clause of node.clauses){
      if(clause.variantName===subject.tag){
        let clauseSoil=soil;
        if(clause.binding&&subject.payload!==undefined){
          clauseSoil=soil.child();
          clauseSoil.set(clause.binding,subject.payload);
        }
        let result=null;
        for(const stmt of clause.bodyStatements){
          const r=this.evaluateNode(stmt,clauseSoil);
          if(r&&r.returned)return r.value;
          if(stmt.type==='GiveStatement')return this.evaluateExpressionNode(stmt.valueExpr,clauseSoil);
        }
        return result;
      }
    }
    storm('SEED_STORM',`MATCH: no clause handles variant "${subject.tag}"`,node.line,node.column);
  }

  evaluateGiveStatement(node,soil){
    const val=evalExpr(node.valueExpr,soil);
    return{returned:true,value:val};
  }

  evaluateStopIfStatement(node,soil){
    const C=cond=>evalCond(cond,soil);
    if(C(node.condExpr)){
      if(node.actionExpr)this.emit(String(evalExpr(node.actionExpr,soil)));
      throw new PlantStorm('STOP_STORM','STOP IF triggered',node.line,node.column);
    }
    return null;
  }

  evaluatePutStatement(node,soil){
    return this._evalRaw(`PUT ${node.valueExpr} INTO ${node.targetExpr}`,node,soil);
  }

  evaluateTakeStatement(node,soil){
    return this._evalRaw(`TAKE ${node.valueExpr} FROM ${node.listExpr}`,node,soil);
  }

  evaluateLinkStatement(node,soil){
    // Direct AST evaluation (bypass _evalRaw regex) for MAP[NUM,...] types
    const keyVal = this.evaluateExpressionNode(node.keyExpr, soil);
    const valExpr = this.evaluateExpressionNode(node.valueExpr, soil);
    const mapEntry = soil.get(node.mapIdent);
    if (mapEntry && mapEntry.value instanceof Map) {
      mapEntry.value.set(keyVal, valExpr);
      this.emit(`LINK ${keyVal} → ${node.mapIdent}`,'ok');
      return {next:1};
    }
    // Fallback to legacy regex pipeline for plain objects
    return this._evalRaw(`LINK ${node.keyExpr} WITH ${node.valueExpr} IN ${node.mapIdent}`,node,soil);
  }

  evaluateSortStatement(node, soil) {
    // v0.38.0: dispatch to sort_evaluator for v2 nodes
    return evaluateSortStatement(node, this, soil);
  }

  evaluateShakeStatement(node,soil){
    return this._evalRaw(`SHAKE ${node.listIdent}`,node,soil);
  }

  evaluateEvaporateStatement(node,soil){
    return this._evalRaw(`EVAPORATE ${node.identifier}`,node,soil);
  }

  evaluateLockStatement(node,soil){
    return this._evalRaw(`LOCK ${node.identifier}`,node,soil);
  }

  evaluateBraidStatement(node,soil){
    return this._evalRaw(`BRAID ${node.list1} WITH ${node.list2} AS ${node.resultIdent}${node.asMap?' MAP':''}`,node,soil);
  }

  evaluateHarvestStatement(node,soil){
    const url=evalExpr(node.urlExpr,soil);
    let raw=`HARVEST "${url}"`;
    if(node.method&&node.method!=='GET')raw+=` METHOD:${node.method}`;
    if(node.bodyExpr)raw+=` BODY:${node.bodyExpr}`;
    if(node.headersIdent)raw+=` HEADERS:${node.headersIdent}`;
    if(node.timeoutExpr)raw+=` TIMEOUT:${node.timeoutExpr}`;
    raw+=` AS ${node.resultIdent}`;
    return this._evalRaw(raw,node,soil);
  }

  evaluateAnalyzeStatement(node,soil){
    return this._evalRaw(`ANALYZE ${node.identifier}`,node,soil);
  }

  evaluateWaitStatement(node,soil){
    return this._evalRaw(`WAIT ${node.secsExpr}`,node,soil);
  }

  evaluateVerifyStatement(node,soil){
    return this._evalRaw(`VERIFY "${node.label}", ${node.assertion}`,node,soil);
  }

  evaluateSuiteStatement(node,soil){
    this.verifyStats.suite=node.name;
    this.emit(`\nSUITE "${node.name}"`, 'suite');
    // Don't propagate 'returned' from GIVE AS RESPONSE out of SUITE scope
    for(const n of (node.bodyStatements||[])){
      this.evaluateNode(n,soil);
    }
    this.verifyStats.suite=null;
    return null;
  }

  evaluatePlantStatement(node,soil){
    const libName=node.libName.trim().toLowerCase();
    if(INNATE[libName]){
      this.planted.set(libName,INNATE[libName]);
      if(!this._symbolPassDone)this.emit(`✓ PLANT "${node.libName.trim()}"`, 'ok');
    }else{
      this.emit(`⚠ PLANT "${node.libName.trim()}" — not found`,'warn');
    }
    return null;
  }

  evaluateMissionStatement(node,soil){
    this.mission=node.mode;
    return null;
  }

  // ── v0.43.0: CONST declaration ──
  evaluateConstDeclaration(node, soil) {
    let value;
    if (node.valueExpr && typeof node.valueExpr === 'object' && node.valueExpr.type === 'Literal') {
      value = node.valueExpr.value;
    } else {
      value = evalExpr(node.valueExpr, soil);
    }
    const type = node.varType || inferType(value);
    soil.set(node.identifier, value, type, { locked: true });
    return null;
  }

  // ── v0.43.0: ENUM declaration ──
  evaluateEnumDeclaration(node, soil) {
    const enumMap = {};
    for (const m of node.members) {
      enumMap[m.name] = m.value;
      soil.set(node.name + '.' + m.name, m.value, 'NUM');
    }
    soil.set(node.name, enumMap, 'MAP');
    return null;
  }

  // ── v0.43.0: TYPE alias declaration ──
  evaluateTypeAliasDeclaration(node, soil) {
    // Register alias in type map for later CREATE/TYPE checks
    if (!this._typeAliases) this._typeAliases = new Map();
    this._typeAliases.set(node.alias, node.targetType);
    return null;
  }

  evaluateRootStatement(node,soil){
    const val=evalExpr(node.valueExpr,soil);
    soil.set(node.identifier,val,inferType(val),{locked:true});
    if(!this._symbolPassDone)this.emit(`✓ ROOT "${node.identifier}" = ${val}`,'ok');
    return null;
  }

  evaluateRootScopeStatement(node,soil){
    const map={};
    for(const link of (node.links||[])){
      const keyStr=link.key.replace(/^"|"$/g,'');
      map[keyStr]=evalExpr(link.valueExpr,soil);
    }
    soil.set(node.identifier,map,'MAP',{locked:true});
    if(!this._symbolPassDone)this.emit(`✓ ROOT_SCOPE "${node.identifier}" — ${Object.keys(map).length} keys`,'ok');
    return null;
  }

  evaluateShowVerifySummary(node,soil){
    return this._evalRaw('SHOW_VERIFY_SUMMARY',node,soil);
  }

  /**
   * symbolPass(programNode) — AST-based symbol table pre-registration.
   *
   * Replaces _firstPass() as the pre-execution declaration registration
   * step for the new AST pipeline (runSource/runProgram). Walks the
   * ProgramNode's top-level statements once before main execution and
   * registers every ActionDeclarationNode, SpeciesDeclarationNode,
   * ROOT RawStatement, ROOT_SCOPE RawStatement, MISSION RawStatement,
   * and PLANT RawStatement into the interpreter's symbol tables.
   *
   * This is structurally identical to _firstPass()'s contract — forward
   * references work because declarations are visible before the first
   * non-declaration statement runs — but it operates on typed AST nodes
   * rather than flat text records, and it delegates the actual
   * registration logic to the same evaluateActionDeclaration /
   * evaluateSpeciesDeclaration methods that the main execution pass
   * uses, so there's no duplication of registration logic.
   *
   * RawStatements that contain legacy pre-pass declarations (ROOT,
   * ROOT_SCOPE, MISSION, PLANT) are still forwarded to the legacy
   * _firstPass()-compatible handler via a one-shot flat-statement list
   * so those constructs keep working during the remaining migration.
   */
  symbolPass(programNode){
    const legacyDecls=[];
    for(const node of programNode.statements){
      if(node.type==='ActionDeclaration'){
        this.evaluateActionDeclaration(node,this.soil);
      }else if(node.type==='SpeciesDeclaration'){
        this.evaluateSpeciesDeclaration(node,this.soil);
      }else if(node.type==='PlantStatement'){
        this.evaluatePlantStatement(node,this.soil);
      }else if(node.type==='StructDeclaration'){
        this.evaluateStructDeclaration(node,this.soil);
      }else if(node.type==='MissionStatement'){
        this.evaluateMissionStatement(node,this.soil);
      }else if(node.type==='RootStatement'){
        this.evaluateRootStatement(node,this.soil);
      }else if(node.type==='RootScopeStatement'){
        this.evaluateRootScopeStatement(node,this.soil);
      }else if(node.type==='RawStatement'){
        const t=node.text;
        if(t&&(
          /^ROOT\s+\w+\s+TO\s+/i.test(t)||
          /^ROOT_SCOPE\s+\w+$/i.test(t)||
          /^MISSION\s*:/i.test(t)||
          /^PLANT\s+/i.test(t)
        )){
          legacyDecls.push({text:t,depth:node.depth||0,line:node.line,column:node.column});
        }
      }
    }
    if(legacyDecls.length>0)this._firstPass(legacyDecls);
    this._symbolPassDone=true;
  }


  runFile(filePath){
    const {parseFile}=require('./parser');
    const absPath=path.resolve(filePath);
    this.rootDir=path.dirname(absPath);
    const programNode=parseFile(absPath);
    this.symbolPass(programNode);         // register declarations before execution
    this.runProgram(programNode);
    return programNode;
  }

  _firstPass(stmts){
    let i=0;
    while(i<stmts.length){
      const{text,line,column}=stmts[i];
      let m;
      if(m=text.match(/^ROOT\s+(\w+)\s+TO\s+(.+)$/i)){
        this.soil.set(m[1],evalExpr(m[2],this.soil),null,{locked:true});
        i++;continue;
      }
      // ROOT_SCOPE NAME, ... LINK "key" WITH val IN NAME ... ROOT_SCOPE/
      if(m=text.match(/^ROOT_SCOPE\s+(\w+)$/i)){
        const scopeName=m[1],map={};
        let j=i+1;
        while(j<stmts.length&&!stmts[j].text.match(/^ROOT_SCOPE\/\.?$/i)){
          const lm=stmts[j].text.match(/^LINK\s+"([^"]*)"\s+WITH\s+(.+?)\s+IN\s+(\w+)$/i);
          if(lm&&lm[3]===scopeName)map[lm[1]]=evalExpr(lm[2],this.soil);
          j++;
        }
        this.soil.set(scopeName,map,'MAP',{locked:true});
          this.emit(`✓ ROOT_SCOPE "${scopeName}" — ${Object.keys(map).length} keys`,'ok');
        i=j+1;continue;
      }
      if(m=text.match(/^MISSION\s*:\s*(\w+)$/i)){this.mission=m[1].toUpperCase();i++;continue;}
      if(m=text.match(/^PLANT\s+(\w+)(?:\s+AS\s+(\w+))?$/i)){
        const libName=m[1].toLowerCase(),alias=m[2]||m[1];
        if(INNATE[libName]){this.planted.set(alias,INNATE[libName]);this.emit(`✓ PLANT "${m[1]}"`, 'ok');}
        else this.emit(`⚠ PLANT "${m[1]}" — not found`,'warn');
        i++;continue;
      }
      if(m=text.match(/^ACTION\s+(\w+)\((.*)\)$/i)){
        const name=m[1],params=this._parseParams(m[2]),body=[];
        let j=i+1;
        while(j<stmts.length&&!stmts[j].text.match(/^\/ACTION\.?$/i))body.push(stmts[j++]);
        this.funcs.set(name,{params,body,line});
        i=j+1;continue;
      }
      if(m=text.match(/^SPECIES\s+(\w+)(?:\s+PARENT\s+(\w+))?$/i)){
        const sname=m[1],parentName=m[2]||null;
        const fields={},actions={};
        if(parentName&&this.species.has(parentName)){
          const p=this.species.get(parentName);
          Object.assign(fields,JSON.parse(JSON.stringify(p.fields)));
          Object.assign(actions,JSON.parse(JSON.stringify(p.actions)));
        }
        let j=i+1;
        while(j<stmts.length&&!stmts[j].text.match(/^\/SPECIES\.?$/i)){
          const cs=stmts[j].text;let fm;
          if(fm=cs.match(/^VAR\s+(\w+)\((\w+)\)(?:\s+TO\s+(.+))?$/i)){
            fields[fm[1]]={type:fm[2].toUpperCase(),default:fm[3]?coerce(fm[3]):null};j++;continue;
          }
          if(fm=cs.match(/^ACTION\s+(\w+)\((.*)\)$/i)){
            const aname=fm[1],aparams=this._parseParams(fm[2]),abody=[];
            let k=j+1;
            while(k<stmts.length&&!stmts[k].text.match(/^\/ACTION\.?$/i))abody.push(stmts[k++]);
            actions[aname]={params:aparams,body:abody};j=k+1;continue;
          }
          j++;
        }
        this.species.set(sname,{fields,actions,parent:parentName});
        i=j+1;continue;
      }
      i++;
    }
  }

  _execBlock(stmts,from,to,soil){
    let i=from;
    while(i<to){
      const result=this._execOne(stmts,i,to,soil);
      if(result&&result.returned)return result;
      i=result?result.next:i+1;
    }
    return null;
  }

  _execOne(stmts,i,maxIdx,soil){
    const{text,line,column}=stmts[i];
    if(!text||/^\\+$/.test(text)||text.startsWith('#'))return{next:i+1};
    if(text.match(/^\/ACTION\.?$/i)||text.match(/^\/SPECIES\.?$/i)||
       text.match(/^(\d+)\\\.?$/)||text.match(/^\/\d+\.?$/))return{next:i+1};
    if(text.match(/^LISTEN\/\.?$/i))return{next:i+1};
    if(text.match(/^ACTION\s+\w+\(/i)){let j=i+1;while(j<stmts.length&&!stmts[j].text.match(/^\/ACTION\.?$/i))j++;return{next:j+1};}
    if(text.match(/^SPECIES\s+/i)){let j=i+1;while(j<stmts.length&&!stmts[j].text.match(/^\/SPECIES\.?$/i))j++;return{next:j+1};}
    if(text.match(/^ROOT\s+\w+\s+TO/i)||text.match(/^PLANT\s+/i)||text.match(/^MISSION\s*:/i))return{next:i+1};
    if(text.match(/^ROOT_SCOPE\s+\w+$/i)){let j=i+1;while(j<stmts.length&&!stmts[j].text.match(/^ROOT_SCOPE\/\.?$/i))j++;return{next:j+1};}
    if(text.match(/^SUITE\/\.?$/i))return{next:i+1};
    try{return this._exec(text,stmts,i,soil,line,column)||{next:i+1};}
    catch(e){
      if(e instanceof PlantStorm){
        if(e.line===undefined||e.line===null){e.line=line;e.column=column;}
        throw e;
      }
      this.emit(`⚡ ${e.message}`,'error');
      return{next:i+1};
    }
  }

  _exec(stmt,stmts,i,soil,line,column){
    const E=(expr)=>{
      try{return evalExpr(expr,soil);}
      catch(e){
        if(e instanceof PlantStorm&&e.line===undefined){e.line=line;e.column=column;}
        throw e;
      }
    };
    const C=(cond)=>evalCond(cond,soil);
    let m;

    if(m=stmt.match(/^GIVE\s+(.+)$/i))return{next:i+1,returned:true,value:E(m[1])};

    // SHOW
    if(m=stmt.match(/^SHOW\s+"([^"]*)"$/i)){this.emit(m[1]);return{next:i+1};}
    if(m=stmt.match(/^SHOW\s+NOW$/)){this.emit(new Date().toLocaleString('en-US'));return{next:i+1};}
    if(m=stmt.match(/^SHOW\s+(COUNT|SUM|MAX|MIN|FIRST|LAST)\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e)storm('MISSING_STORM',`"${m[2]}" not found`,line,column);
      const v=e.value,op=m[1].toUpperCase();
      let res;
      if(op==='COUNT')res=Array.isArray(v)?v.length:1;
      else if(op==='FIRST')res=Array.isArray(v)?v[0]:v;
      else if(op==='LAST')res=Array.isArray(v)?v[v.length-1]:v;
      else{const nums=(Array.isArray(v)?v:[v]).map(Number).filter(n=>!isNaN(n));
        if(op==='SUM')res=nums.reduce((a,b)=>a+b,0);
        else if(op==='MAX')res=Math.max(...nums);
        else if(op==='MIN')res=Math.min(...nums);}
      this.emit(`${m[2]}: ${res}`);return{next:i+1};
    }
    if(m=stmt.match(/^SHOW\s+TYPE\s+(\w+)$/i)){const e=soil.get(m[1]);this.emit(`TYPE "${m[1]}": ${e?e.type:'VOID'}`);return{next:i+1};}
    if(m=stmt.match(/^SHOW\s+(\w+):"([^"]*)"$/i)){
      const e=soil.get(m[1]);
      if(!e||!e.value)storm('MISSING_STORM',`"${m[1]}" not found`,line,column);
      const _rv=e.value[m[2]];const _rd=(_rv&&typeof _rv==='object')?(Array.isArray(_rv)?'['+_rv.join(', ')+']':JSON.stringify(_rv)):String(_rv===undefined?'void':_rv);this.emit(`${m[1]}:"${m[2]}" = ${_rd}`,'inf');return{next:i+1};
    }
    if(m=stmt.match(/^SHOW\s+(\w+):(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!e.value)storm('MISSING_STORM',`"${m[1]}" not found`,line,column);
      const _rv=e.value[m[2]];const _rd=(_rv&&typeof _rv==='object')?(Array.isArray(_rv)?'['+_rv.join(', ')+']':JSON.stringify(_rv)):String(_rv===undefined?'void':_rv);this.emit(`${m[1]}:${m[2]} = ${_rd}`,'inf');return{next:i+1};
    }
    if(m=stmt.match(/^SHOW\s+(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e)storm('MISSING_STORM',`"${m[1]}" not found`,line,column);
      const display=Array.isArray(e.value)?`[${e.value.join(', ')}]`:
        (e.value&&typeof e.value==='object'&&!Array.isArray(e.value)?
          `{${Object.entries(e.value).filter(([k])=>!k.startsWith('__')).map(([k,v])=>`${k}:${v}`).join(', ')}}`:
          String(e.value));
      this.emit(`${m[1]}: ${display}`);return{next:i+1};
    }
    if(m=stmt.match(/^SHOW\s+(.+)$/i)){this.emit(String(E(m[1])));return{next:i+1};}

    // CREATE
    if(m=stmt.match(/^CREATE\s+(\w+)\((\w+)\)\s+PULSE(?:\s+TO\s+(.+))?$/i)){
      const val=m[3]?E(m[3]):(m[2].toUpperCase()==='NUM'?0:'');
      soil.set(m[1],val,m[2].toUpperCase(),{pulse:true});
      this.emit(`CREATE "${m[1]}"(${m[2].toUpperCase()}) PULSE = ${val}`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^CREATE\s+(\w+)\(MAP\)$/i)){soil.set(m[1],{},'MAP');this.emit(`CREATE "${m[1]}"(MAP)`,'ok');return{next:i+1};}
    // CREATE list(LIST) TO  (empty — no items)
    if(m=stmt.match(/^CREATE\s+(\w+)\(LIST\)\s+TO$/i)){
      soil.set(m[1],[],'LIST');
      this.emit(`CREATE "${m[1]}"(LIST) = []`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^CREATE\s+(\w+)\((\w+)\)(?:\s+TO\s+(.+))?$/i)){
      const name=m[1],type=m[2].toUpperCase(),rawVal=m[3];
      let val;
      if(type==='LIST')val=rawVal?rawVal.split(',').map(v=>coerce(v.trim())).filter(v=>v!==''):[];
      else val=rawVal?E(rawVal):(type==='FACT'?false:type==='NUM'?0:'');
      soil.set(name,val,type);
      this.emit(`CREATE "${name}"(${type}) = ${Array.isArray(val)?'['+val.join(', ')+']':val}`,'ok');return{next:i+1};
    }

    // SET SELF:prop (inside ACTION)
    if(m=stmt.match(/^SET\s+SELF:(\w+)\s+TO\s+(.+)$/i)){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF not available',line,column);
      const newVal=E(m[2]);
      selfEntry.value[m[1]]=newVal;
      const scoped=soil.get('SELF:'+m[1]);
      if(scoped)scoped.value=newVal;else soil.set('SELF:'+m[1],newVal);
      return{next:i+1};
    }
    // SET obj:"key" TO  (quoted MAP key)
    if(m=stmt.match(/^SET\s+(\w+):"([^"]*)"\s+TO\s+(.+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!e.value||typeof e.value!=='object')storm('TYPE_STORM',`"${m[1]}" is not an object`,line,column);
      if(e.locked)storm('LOCK_STORM',`"${m[1]}" protected — cannot be modified`,line,column);
      e.value[m[2]]=E(m[3]);
      this.emit(`SET ${m[1]}:"${m[2]}" → ${e.value[m[2]]}`,'ok');return{next:i+1};
    }
    // SET inst:prop
    if(m=stmt.match(/^SET\s+(\w+):(\w+)\s+TO\s+(.+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!e.value||typeof e.value!=='object')storm('TYPE_STORM',`"${m[1]}" is not an object`,line,column);
      if(e.locked)storm('LOCK_STORM',`"${m[1]}" protected — cannot be modified`,line,column);
      e.value[m[2]]=E(m[3]);
      this.emit(`SET ${m[1]}:${m[2]} → ${e.value[m[2]]}`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^SET\s+(\w+)\s+TO\s+(.+)$/i)){
      const newVal=E(m[2]);
      const e=soil.update(m[1],newVal);
      if(e.pulse&&this.watchers.has(m[1])){
        const childSoil=soil.child();
        childSoil.set(m[1],newVal,e.type,{pulse:true});
        for(const watchBody of this.watchers.get(m[1])){
          this._execBlock(watchBody,0,watchBody.length,childSoil);
        }
      }
      this.emit(`SET "${m[1]}" → ${newVal}`,'ok');return{next:i+1};
    }

    // INCREASE / DECREASE
    if(m=stmt.match(/^(INCREASE|DECREASE)\s+SELF:(\w+)\s+BY\s+(.+)$/i)){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF not available',line,column);
      const n=+E(m[3]),prop=m[2];
      const newVal=m[1].toUpperCase()==='INCREASE'?(+selfEntry.value[prop]||0)+n:(+selfEntry.value[prop]||0)-n;
      selfEntry.value[prop]=newVal;
      // Also update SELF:prop in scope so GIVE SELF:prop works
      const selfPropEntry=soil.get('SELF:'+prop);
      if(selfPropEntry)selfPropEntry.value=newVal;
      else soil.set('SELF:'+prop,newVal);
      return{next:i+1};
    }
    // INCREASE/DECREASE obj.field BY amount (struct member access)
    if(m=stmt.match(/^(INCREASE|DECREASE)\s+(\w+)\s*\.\s*(\w+)\s+BY\s+(.+)$/i)){
      const objEntry=soil.get(m[2]);
      if(!objEntry)storm('MISSING_STORM',`"${m[2]}" not found`,line,column);
      if(typeof objEntry.value!=='object'||objEntry.value===null)
        storm('TYPE_STORM',`cannot ${m[1]} on ${objEntry.type}`,line,column);
      if(!(m[3] in objEntry.value))
        storm('MISSING_STORM',`"${m[2]}" does not have a property "${m[3]}"`,line,column);
      const n=+E(m[4]);
      objEntry.value[m[3]]=m[1].toUpperCase()==='INCREASE'?(+objEntry.value[m[3]]||0)+n:(+objEntry.value[m[3]]||0)-n;
      return{next:i+1};
    }
    if(m=stmt.match(/^(INCREASE|DECREASE)\s+(\w+)\s+BY\s+(.+)$/i)){
      const e=soil.get(m[2]);
      if(!e)storm('MISSING_STORM',`"${m[2]}" not found`,line,column);
      if(Array.isArray(e.value)||typeof e.value==='object')storm('TYPE_STORM',`cannot ${m[1]} on ${e.type}`,line,column);
      const n=+E(m[3]);
      e.value=m[1].toUpperCase()==='INCREASE'?(+e.value||0)+n:(+e.value||0)-n;
      return{next:i+1};
    }

    // LIST ops
    // PUT val INTO SELF:list
    if(m=stmt.match(/^PUT\s+(.+?)\s+INTO\s+SELF:(\w+)$/i)){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF not available',line,column);
      if(!Array.isArray(selfEntry.value[m[2]]))storm('TYPE_STORM',`SELF:${m[2]} is not a LIST`,line,column);
      selfEntry.value[m[2]].push(E(m[1]));return{next:i+1};
    }
    if(m=stmt.match(/^PUT\s+(.+?)\s+INTO\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[2]}" is not a LIST`,line,column);
      e.value.push(E(m[1]));return{next:i+1};
    }
    if(m=stmt.match(/^TAKE\s+(.+?)\s+FROM\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[2]}" is not a LIST`,line,column);
      const idx=e.value.indexOf(E(m[1]));
      if(idx===-1)storm('LOST_STORM',`element not found in "${m[2]}"`,line,column);
      e.value.splice(idx,1);return{next:i+1};
    }
    if(m=stmt.match(/^SORT\s+(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[1]}" is not a LIST`,line,column);
      e.value.sort((a,b)=>typeof a==='number'&&typeof b==='number'?a-b:String(a).localeCompare(String(b)));
      return{next:i+1};
    }
    if(m=stmt.match(/^SHAKE\s+(\w+)$/i)){
      const e=soil.get(m[1]);if(!e||!Array.isArray(e.value))storm('TYPE_STORM','',line,column);
      for(let j=e.value.length-1;j>0;j--){const k=Math.floor(Math.random()*(j+1));[e.value[j],e.value[k]]=[e.value[k],e.value[j]];}
      return{next:i+1};
    }
    if(m=stmt.match(/^EMPTY\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e)storm('MISSING_STORM','',line,column);e.value=Array.isArray(e.value)?[]:'';return{next:i+1};}
    if(m=stmt.match(/^EVAPORATE\s+(\w+)$/i)){soil.delete(m[1]);this.emit(`EVAPORATE "${m[1]}" ✓`,'muted');return{next:i+1};}
    if(m=stmt.match(/^LOCK\s+(\w+)$/i)){const e=soil.get(m[1]);if(e){e.locked=true;this.emit(`LOCK "${m[1]}" 🔒`,'warn');}return{next:i+1};}

    // BRAID ─────────────────────────────────────────────────
    // BRAID list1 WITH list2 AS result          → [[a1,b1],[a2,b2],...]
    // BRAID list1 WITH list2 AS result MAP      → {a1:b1, a2:b2, ...}
    if(m=stmt.match(/^BRAID\s+(\w+)\s+WITH\s+(\w+)\s+AS\s+(\w+)(\s+MAP)?$/i)){
      const e1=soil.get(m[1]),e2=soil.get(m[2]);
      if(!e1||!Array.isArray(e1.value))storm('TYPE_STORM',`"${m[1]}" is not a LIST`,line,column);
      if(!e2||!Array.isArray(e2.value))storm('TYPE_STORM',`"${m[2]}" is not a LIST`,line,column);
      const a=e1.value,b=e2.value;
      const len=Math.min(a.length,b.length);
      const asMap=!!m[4];
      let result;
      if(asMap){
        result={};
        for(let j=0;j<len;j++)result[String(a[j])]=b[j];
        soil.set(m[3],result,'MAP');
        this.emit(`BRAID "${m[1]}" ⟷ "${m[2]}" → MAP "${m[3]}" {${len} pairs}`,'ok');
      }else{
        result=[];
        for(let j=0;j<len;j++)result.push([a[j],b[j]]);
        soil.set(m[3],result,'LIST');
        this.emit(`BRAID "${m[1]}" ⟷ "${m[2]}" → LIST "${m[3]}" [${len} pairs]`,'ok');
      }
      return{next:i+1};
    }
    // ─────────────────────────────────────────────────────────

    // MAP
    // LINK "key" WITH val IN SELF:map
    if(m=stmt.match(/^LINK\s+(\S+)\s+WITH\s+(.+?)\s+IN\s+SELF:(\w+)$/i)){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF not available',line,column);
      // m[1] can be a quoted string "key" or a variable name
      const mapKey=String(E(m[1]));
      const mapVal=E(m[2]);
      if(!selfEntry.value[m[3]]||typeof selfEntry.value[m[3]]!=='object')selfEntry.value[m[3]]={};
      selfEntry.value[m[3]][mapKey]=mapVal;
      return{next:i+1};
    }
    if(m=stmt.match(/^LINK\s+"([^"]+)"\s+WITH\s+(.+?)\s+IN\s+(\w+)$/i)){
      const e=soil.get(m[3]);
      if(!e||typeof e.value!=='object'||Array.isArray(e.value))storm('TYPE_STORM',`"${m[3]}" is not a MAP`,line,column);
      const val=E(m[2]);
      if(e.value instanceof Map){e.value.set(m[1],val);}else{e.value[m[1]]=val;}
      this.emit(`LINK "${m[1]}" → ${m[3]}`,'ok');return{next:i+1};
    }
    // LINK unquoted-key WITH val IN map (e.g. LINK 1 WITH "hello" IN m)
    if(m=stmt.match(/^LINK\s+(\S+)\s+WITH\s+(.+?)\s+IN\s+(\w+)$/i)){
      const e=soil.get(m[3]);
      if(!e||typeof e.value!=='object'||Array.isArray(e.value))storm('TYPE_STORM',`"${m[3]}" is not a MAP`,line,column);
      const key=E(m[1]);
      const val=E(m[2]);
      if(e.value instanceof Map){e.value.set(key,val);}else{e.value[key]=val;}
      this.emit(`LINK ${key} → ${m[3]}`,'ok');return{next:i+1};
    }

    // REAP SELF:method (call own method inside ACTION)
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+SELF:(\w+)(?:,\s*(.*))?$/i)){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF not available',line,column);
      const spName=selfEntry.value.__species;
      const sp=spName&&this.species.get(spName);
      if(!sp)storm('MISSING_STORM','SPECIES not found',line,column);
      const method=sp.actions[m[2]];
      if(!method)storm('MISSING_STORM',`action "${m[2]}" not found`,line,column);
      const rawArgs=m[3]||'';
      const argVals=rawArgs?this._splitArgs(rawArgs).map(a=>E(a.trim())):[];
      const result=this._callAction(method,argVals,selfEntry.value,soil);
      const val=result.value!==undefined?result.value:null;
      if(m[1]!=='_')soil.set(m[1],val,inferType(val));
      return{next:i+1};
    }
    // FLOW pipeline: REAP res FROM source FLOW fn1 FLOW fn2...
    if(stmt.includes(' FLOW ')||stmt.includes(' FLOW\n')){
      const flowMatch=stmt.match(/^REAP\s+(\w+)\s+FROM\s+((?:"[^"]*"|[^\s])+(?:\s+(?!FLOW\s)(?:"[^"]*"|[^\s])+)*)(?=\s+FLOW\s)/i)||stmt.match(/^REAP\s+(\w+)\s+FROM\s+("[^"]*")(?=\s+FLOW\s)/i);
      if(flowMatch){
        const resName=flowMatch[1];
        let current=E(flowMatch[2].replace(/,\s*$/,''));
        const afterFrom=stmt.replace(/^REAP\s+\w+\s+FROM\s+.+?(?=\s+FLOW\s)/i,'').trim();const pipeline=afterFrom.replace(/^FLOW\s+/i,'').split(/\s+FLOW\s+/i).filter(Boolean);
        for(const step of pipeline){
          const _st=step.trim().replace(/,\s*$/,'');
          if(_st==='SORT'){if(Array.isArray(current))current=[...current].sort((a,b)=>typeof a==='number'&&typeof b==='number'?a-b:String(a).localeCompare(String(b)));continue;}
          if(_st==='REVERSE'){if(Array.isArray(current))current=[...current].reverse();continue;}
          if(_st==='UNIQUE'){if(Array.isArray(current))current=[...new Set(current)];continue;}
          if(_st==='FLATTEN'){if(Array.isArray(current))current=current.flat();continue;}
          const fn=this.funcs.get(_st);
          if(fn){const r=this._callAction(fn,[current],null,soil);current=r.value!==undefined?r.value:current;}
          else this.emit(`FLOW: "${_st}" not defined`,'warn');
        }
        if(resName!=='_')soil.set(resName,current,inferType(current));
        return{next:i+1};
      }
    }
    // REAP lib:FUNC
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+(\w+):(\w+)(?:,\s*(.*))?$/i)){
      const resName=m[1],libOrObj=m[2],funcName=m[3],rawArgs=m[4]||'';
      if(this.planted.has(libOrObj)){
        const lib=this.planted.get(libOrObj);
        const fn=lib[funcName.toUpperCase()];
        if(!fn)storm('MISSING_STORM',`"${funcName}" not found in "${libOrObj}"`,line,column);
        const argVals=rawArgs?this._splitArgs(rawArgs).map(a=>E(a.trim())):[];
        const result=fn(argVals);
        if(resName!=='_')soil.set(resName,result,inferType(result));
        return{next:i+1};
      }
      const instEntry=soil.get(libOrObj);
      if(!instEntry)storm('MISSING_STORM',`"${libOrObj}" not found`,line,column);
      const spName=instEntry.value&&instEntry.value.__species;
      const sp=spName&&this.species.get(spName);
      if(!sp)storm('MISSING_STORM',`SPECIES not found`,line,column);
      const method=sp.actions[funcName];
      if(!method)storm('MISSING_STORM',`action "${funcName}" not found`,line,column);
      const argVals=rawArgs?this._splitArgs(rawArgs).map(a=>E(a.trim())):[];
      const result=this._callAction(method,argVals,instEntry.value,soil);
      if(resName!=='_')soil.set(resName,result.value!==undefined?result.value:null,inferType(result.value));
      return{next:i+1};
    }

    // ── VERIFY framework ──────────────────────────────────────
    // SUITE "name", ...VERIFY/stmts... SUITE/.
    if(m=stmt.match(/^SUITE\s+"([^"]+)"$/i)){
      this.verifyStats.suite=m[1];
      // Collect until SUITE/
      let j=i+1;
      while(j<stmts.length&&!stmts[j].text.match(/^SUITE\/\.?$/i))j++;
      const suiteBody=stmts.slice(i+1,j);
      this.emit(`\nSUITE "${m[1]}"`, 'suite');
      this._execBlock(suiteBody,0,suiteBody.length,soil);
      this.verifyStats.suite=null;
      return{next:j+1};
    }

    // VERIFY "label", assertion (inline — joined by lexer comma continuation)
    if(m=stmt.match(/^VERIFY\s+"([^"]+)",?\s+(.+)$/i)){
      const label=m[1];
      const assertion=m[2].trim();
      let pass=false, detail='', am;

      // FROM action, args GIVES expected
      if(am=assertion.match(/^FROM\s+(\w+)(?:,\s*(.+?))?\s+GIVES\s+(.+)$/i)){
        const fnName=am[1],rawArgs=am[2]||'',expected=E(am[3]);
        let actual=null;
        if(this.funcs.has(fnName)){
          const argVals=rawArgs?this._splitArgs(rawArgs).map(a=>E(a.trim())):[];
          try{const r=this._callAction(this.funcs.get(fnName),argVals,null,soil);actual=r.value!==undefined?r.value:null;}
          catch(e){detail=`threw ${e.stormType||e.message}`;}
        }else{detail=`action "${fnName}" not found`;}
        if(!detail){pass=String(actual)===String(expected);if(!pass)detail=`got ${JSON.stringify(actual)}, expected ${JSON.stringify(expected)}`;}
      }

      // STORMS STORM_TYPE FROM expr|stmt
      else if(am=assertion.match(/^STORMS\s+(\w+)\s+FROM\s+(.+)$/i)){
        const expectedStorm=am[1].toUpperCase();
        const testExpr=am[2].trim();
        const _tryStorm=()=>{
          // Bare undefined variable check: single word not in soil
          if(/^[a-zA-Z_][a-zA-Z0-9_]*$/.test(testExpr)&&!soil.get(testExpr)){
            storm('MISSING_STORM',`"${testExpr}" not found`,line,column);
          }
          // Try as statement (SET, INCREASE etc)
          const stmtM=testExpr.match(/^(SET|INCREASE|DECREASE|LOCK|EVAPORATE|HARVEST|PUT|TAKE|REAP|LISTEN)\s/i);
          if(stmtM){const r=this._exec(testExpr,stmts,i,soil,line,column);return;}
          // Try as expression
          E(testExpr);
        };
        try{_tryStorm();pass=false;detail='no storm thrown';}
        catch(e){
          if(e instanceof PlantStorm){pass=e.stormType===expectedStorm;if(!pass)detail=`got ${e.stormType}, expected ${expectedStorm}`;}
          else{pass=false;detail=e.message||String(e);}
        }
      }

      // TYPE var IS TYPE_NAME  (also supports TYPE obj:"key" IS ...)
      else if(am=assertion.match(/^TYPE\s+(\w+):"([^"]*)"\s+IS\s+(\w+)$/i)){
        const e=soil.get(am[1]);
        const val=e&&e.value&&typeof e.value==='object'?e.value[am[2]]:undefined;
        const actualType=val===undefined?'VOID':inferType(val);
        pass=actualType.toUpperCase()===am[3].toUpperCase();
        if(!pass)detail=`type is ${actualType}`;
      }
      else if(am=assertion.match(/^TYPE\s+(\w+)\s+IS\s+(\w+)$/i)){
        const e=soil.get(am[1]);const actualType=e?e.type:'VOID';
        pass=actualType.toUpperCase()===am[2].toUpperCase();
        if(!pass)detail=`type is ${actualType}`;
      }

      // COUNT list IS n
      else if(am=assertion.match(/^COUNT\s+(\w+)\s+IS\s+(.+)$/i)){
        const e=soil.get(am[1]);
        const cnt=e&&Array.isArray(e.value)?e.value.length:(e&&e.value&&typeof e.value==='object'?Object.keys(e.value).length:0);
        const expected=+E(am[2]);pass=cnt===expected;
        if(!pass)detail=`count is ${cnt}, expected ${expected}`;
      }

      // TYPE var|expr IS TYPE_NAME — must be checked before general IS condition
      else if(am=assertion.match(/^TYPE\s+(.+?)\s+IS\s+(\w+)$/i)){
        let actualType='VOID';
        const simpleE=soil.get(am[1]);
        if(simpleE){actualType=simpleE.type;}
        else{try{const v=E(am[1]);actualType=inferType(v);}catch(_){}}
        pass=actualType.toUpperCase()===am[2].toUpperCase();
        if(!pass)detail=`type is ${actualType}`;
      }

      // General condition: expr IS val / BETWEEN / GREATER THAN etc.
      else{
        try{pass=evalCond(assertion,soil);}
        catch(e){pass=false;detail=e.message||String(e);}
        if(!pass){
          if(am=assertion.match(/^(.+?)\s+IS\s+(.+)$/i)){
            try{const actual=E(am[1]);detail=`got ${JSON.stringify(actual)}, expected ${JSON.stringify(E(am[2]))}`;}
            catch(_){detail=`condition false`;}
          }else{detail=`condition false: ${assertion}`;}
        }
      }

      // Record
      const prefix=this.verifyStats.suite?'  ':' ';
      if(pass){
        this.verifyStats.passed++;
        this.verifyStats.results.push({pass,label,suite:this.verifyStats.suite});
        this.emit(`${prefix}✓ ${label}`,'verify_pass');
      }else{
        this.verifyStats.failed++;
        this.verifyStats.results.push({pass,label,suite:this.verifyStats.suite,detail});
        this.emit(`${prefix}✗ ${label}`,'verify_fail');
        if(detail)this.emit(`${prefix}  → ${detail}`,'verify_detail');
      }
      return{next:i+1};
    }

    // SHOW_VERIFY_SUMMARY — totals banner
    if(stmt.match(/^SHOW_VERIFY_SUMMARY$/i)){
      const{passed,failed}=this.verifyStats;
      const total=passed+failed;
      this.emit(`\n─────────────────────────────`,'verify_hr');
      this.emit(`VERIFY: ${total} tests — ${passed} passed, ${failed} failed`,'verify_hr');
      if(failed===0)this.emit(`🌿 All tests passed!`,'verify_win');
      else this.emit(`⚡ ${failed} test(s) failed`,'verify_fail');
      this.emit(`─────────────────────────────`,'verify_hr');
      return{next:i+1};
    }
    // ─────────────────────────────────────────────────────────

    // WAIT n  — synchronous sleep (seconds), capped for safety
    if(m=stmt.match(/^WAIT\s+(.+)$/i)){
      const secs=Math.max(0,Math.min(+E(m[1])||0,10));
      if(secs>0){
        const sab=new SharedArrayBuffer(4);
        Atomics.wait(new Int32Array(sab),0,0,secs*1000);
      }
      this.emit(`WAIT ${secs}s ⏳`,'muted');return{next:i+1};
    }

    // ANALYZE x — type-aware data inspection report
    if(m=stmt.match(/^ANALYZE\s+(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e)storm('MISSING_STORM',`"${m[1]}" not found`,line,column);
      const v=e.value,t=e.type;
      if(Array.isArray(v)){
        this.emit(`ANALYZE "${m[1]}" → LIST[${v.length}]`,'inf');
        const nums=v.filter(x=>typeof x==='number');
        if(nums.length===v.length&&nums.length>0){
          const sum=nums.reduce((a,b)=>a+b,0);
          const sorted=[...nums].sort((a,b)=>a-b);
          const mid=Math.floor(sorted.length/2);
          const median=sorted.length%2?sorted[mid]:(sorted[mid-1]+sorted[mid])/2;
          this.emit(`  sum=${sum}  avg=${(sum/nums.length).toFixed(2)}  min=${Math.min(...nums)}  max=${Math.max(...nums)}  median=${median}`,'muted');
        }else{
          this.emit(`  [${v.join(', ')}]`,'muted');
        }
      }else if(t==='MAP'||(v&&typeof v==='object'&&!v.__species&&!v.__vein)){
        const keys=Object.keys(v);
        this.emit(`ANALYZE "${m[1]}" → MAP{${keys.length} keys}`,'inf');
        for(const k of keys)this.emit(`  "${k}": ${v[k]} (${inferType(v[k])})`,'muted');
      }else if(t==='TX'||typeof v==='string'){
        this.emit(`ANALYZE "${m[1]}" → TX[${[...String(v)].length} characters]`,'inf');
        this.emit(`  "${v}"`,'muted');
      }else if(v&&v.__species){
        const fields=Object.keys(v).filter(k=>!k.startsWith('__'));
        this.emit(`ANALYZE "${m[1]}" → INSTANCE(${v.__species}){${fields.length} properties}`,'inf');
        for(const k of fields)this.emit(`  ${k}: ${v[k]} (${inferType(v[k])})`,'muted');
      }else{
        this.emit(`ANALYZE "${m[1]}" → ${t}`,'inf');
        this.emit(`  ${v}`,'muted');
      }
      return{next:i+1};
    }

    // REAP t FROM TYPEOF x
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+TYPEOF\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      const t=e?e.type:'VOID';
      if(m[1]!=='_')soil.set(m[1],t,'TX');return{next:i+1};
    }


    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+NOW(?:\s+FORMAT:(\w+))?$/i)){
      const fmt=(m[2]||'FULL').toUpperCase(),now=new Date();
      const val=fmt==='DATE'?now.toLocaleDateString('en-US'):fmt==='TIME'?now.toLocaleTimeString('en-US'):fmt==='YEAR'?now.getFullYear():fmt==='STAMP'?now.getTime():now.toLocaleString('en-US');
      if(m[1]!=='_')soil.set(m[1],val,fmt==='STAMP'?'NUM':'TX');return{next:i+1};
    }

    // REAP ACTION
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+(\w+)(?:,\s*(.*))?$/i)){
      const resName=m[1],fnName=m[2],rawArgs=m[3]||'';
      if(!this.funcs.has(fnName))storm('MISSING_STORM',`action "${fnName}" not defined`,line,column);
      const fn=this.funcs.get(fnName);
      const argVals=rawArgs?this._splitArgs(rawArgs).map(a=>E(a.trim())):[];
      const result=this._callAction(fn,argVals,null,soil);
      const val=result.value!==undefined?result.value:null;
      if(resName!=='_')soil.set(resName,val,inferType(val));
      return{next:i+1};
    }

    // BLOOM
    if(m=stmt.match(/^BLOOM\s+(\w+)\s+AS\s+(\w+)$/i)){
      const sp=this.species.get(m[1]);
      if(!sp)storm('MISSING_STORM',`SPECIES "${m[1]}" not found`,line,column);
      const inst={__species:m[1],__parent:sp.parent};
      Object.entries(sp.fields).forEach(([fn,fd])=>{
        inst[fn]=fd.default!==null?fd.default:fd.type==='NUM'?0:fd.type==='LIST'?[]:fd.type==='FACT'?false:fd.type==='MAP'?{}:'';
      });
      soil.set(m[2],inst,'INSTANCE');
      this.emit(`BLOOM "${m[1]}" → "${m[2]}"`,'ok');return{next:i+1};
    }

    // ANALYZE
    if(m=stmt.match(/^ANALYZE\s+(\w+)$/i)){
      const e=soil.get(m[1]);if(!e){this.emit(`ANALYZE "${m[1]}" — VOID`,'muted');return{next:i+1};}
      const v=e.value;
      if(Array.isArray(v)){
        const nums=v.map(Number).filter(n=>!isNaN(n));
        this.emit(`ANALYZE "${m[1]}" → LIST[${v.length}]`,'info');
        if(nums.length){
          const sum=nums.reduce((a,b)=>a+b,0),avg=sum/nums.length;
          const sorted=[...nums].sort((a,b)=>a-b);
          const mid=Math.floor(sorted.length/2);
          const median=sorted.length%2?sorted[mid]:(sorted[mid-1]+sorted[mid])/2;
          this.emit(`  sum=${sum}  avg=${avg.toFixed(2)}  min=${sorted[0]}  max=${sorted[sorted.length-1]}  median=${median}`,'muted');
        }
      }else{this.emit(`ANALYZE "${m[1]}" → ${e.type}: ${v}`,'info');}
      return{next:i+1};
    }

    // ── LISTEN BRANCH / RESPONSE: web server grammar (Phase 1) ──
    // Grammar:
    //   LISTEN BRANCH ON [portExpr] WITH [configExpr] AS [requestIdent] MAP,
    //     ...body (may use GIVE [expr] AS RESPONSE)...
    //   LISTEN/.
    //
    // NOTE: this phase implements lexing/grammar/diagnostics only.
    // Actual socket binding is out of scope here — the body is parsed,
    // validated, and (for now) executed once synchronously with the
    // requestIdent bound to an empty request MAP, so handler logic and
    // RESPONSE extraction can be authored and tested ahead of the real
    // listener runtime landing in a later phase.
    if(stmt.match(/^LISTEN\s+BRANCH\b/i)){
      const node=this.parseListenBranch(stmt,stmts,i);
      this.emit(`LISTEN BRANCH ON ${node.portExpr} WITH ${node.configExpr} AS ${node.requestIdent} MAP — registered ✓`,'ok');

      const reqSoil=soil.child();
      reqSoil.set(node.requestIdent,{},'MAP');

      // Execute the handler body in natural order. GIVE [expr] AS RESPONSE
      // is intercepted statement-by-statement (same depth-aware pattern as
      // _execBlock/_execOne) so any CREATE/SET earlier in the body has
      // already run and is visible to the response expression — unlike a
      // pre-scan, which would evaluate RESPONSE before its dependencies exist.
      let response=null,j=0;
      while(j<node.bodyStatements.length){
        const bs=node.bodyStatements[j];
        const respNode=this.parseResponseStatement(bs.text,bs);
        if(respNode){
          response=evalExpr(respNode.responseExpr,reqSoil);
          this.emit(`  → RESPONSE: ${response&&typeof response==='object'?JSON.stringify(response):response}`,'muted');
          j++;continue;
        }
        const r=this._execOne(node.bodyStatements,j,node.bodyStatements.length,reqSoil);
        j=r?r.next:j+1;
      }

      return{next:node._closerIndex+1};
    }

    // ── HARVEST: synchronous-style HTTP/HTTPS client ───────────
    // HARVEST "url" AS result.
    // HARVEST "url" METHOD:POST BODY:payload AS result.
    // HARVEST "url" HEADERS:hdrMap AS result.
    // HARVEST "url" METHOD:POST BODY:payload HEADERS:hdrMap AS result.
    if(m=stmt.match(/^HARVEST\s+(.+?)\s+AS\s+(\w+)$/i)){
      const rawHead=m[1],resName=m[2];

      // First token is the URL (string literal or variable)
      const urlMatch=rawHead.match(/^("(?:[^"]*)"|\S+)/);
      if(!urlMatch)storm('SEED_STORM','HARVEST requires a URL',line,column);
      const urlExpr=urlMatch[1];
      const url=E(urlExpr);
      if(typeof url!=='string'||!url)storm('SEED_STORM',`HARVEST: invalid URL "${url}"`,line,column);

      let rest=rawHead.slice(urlMatch[0].length).trim();
      let method='GET',bodyVal=null,headersVal=null,timeoutMs=10000;

      // METHOD:WORD
      let mm=rest.match(/METHOD:(\w+)/i);
      if(mm)method=mm[1].toUpperCase();

      // TIMEOUT:n  (seconds)
      let tm=rest.match(/TIMEOUT:([\d.]+)/i);
      if(tm)timeoutMs=Math.round(parseFloat(tm[1])*1000);

      // BODY:expr  — expr is either a quoted string or a variable name
      let bm=rest.match(/BODY:("(?:[^"]*)"|\w+)/i);
      if(bm)bodyVal=E(bm[1]);

      // HEADERS:varname — must reference a MAP variable
      let hm=rest.match(/HEADERS:(\w+)/i);
      if(hm){
        const he=soil.get(hm[1]);
        if(!he||typeof he.value!=='object'||Array.isArray(he.value))
          storm('TYPE_STORM',`HARVEST HEADERS: "${hm[1]}" is not a MAP`,line,column);
        headersVal=he.value;
      }

      this.emit(`HARVEST ${method} ${url} …`,'muted');
      const result=harvestSync(url,{method,headers:headersVal,body:bodyVal,timeoutMs});

      if(!result.ok && result.error){
        storm('NETWORK_STORM',`HARVEST failed: ${result.error}`,line,column);
      }

      // Build a response MAP: {ok, status, body}
      const responseBodyVal = toPlantValue(result.body !== undefined ? result.body : result);
      const bodyType = Array.isArray(responseBodyVal) ? 'LIST' : (responseBodyVal && typeof responseBodyVal === 'object') ? 'MAP' : 'TX';

      // Store either just the body (simple), or a full response map
      // Full response: result has status field
      let plantVal, valType;
      if (result.status !== undefined) {
        // Full response object: {ok:FACT, status:NUM, body:..., headers:MAP}
        plantVal = {
          ok: result.ok,
          status: result.status,
          body: responseBodyVal,
          headers: toPlantValue(result.headers || {}),
        };
        valType = 'MAP';
      } else {
        plantVal = responseBodyVal;
        valType = bodyType;
      }

      if(resName!=='_')soil.set(resName, plantVal, valType);
      this.emit(`HARVEST ${method} ${url} → ${result.status||'?'} (${valType})`, result.ok?'ok':'warn');
      return{next:i+1};
    }
    // ─────────────────────────────────────────────────────────

    // TAP/ABSORB/INFUSE/SEAL
    if(m=stmt.match(/^TAP\s+"([^"]+)"\s+MODE:(\w+)\s+AS\s+(\w+)$/i)){
      const fname=m[1],mode=m[2].toUpperCase();
      if(!this.veinFS.exists(fname)){
        const realPath=path.join(this.rootDir,fname);
        if(fs.existsSync(realPath))this.veinFS.write(fname,fs.readFileSync(realPath,'utf8'));
        else this.veinFS.write(fname,'');
      }
      soil.set(m[3],{__vein:true,file:fname,mode,pos:0},'VEIN');
      this.emit(`TAP "${fname}" [${mode}] → "${m[3]}"`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^ABSORB\s+LINE\s+(\w+)\s+AS\s+(\w+)$/i)){
      const e=soil.get(m[1]);if(!e||!e.value||!e.value.__vein)storm('TYPE_STORM',`"${m[1]}" is not a VEIN`,line,column);
      const lines=this.veinFS.read(e.value.file).split('\n');
      const pos=e.value.pos||0,lineContent=lines[pos]||'';
      e.value.pos=pos+1;soil.set(m[2],lineContent,'TX');
      this.emit(`ABSORB LINE → "${lineContent}"`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^ABSORB\s+(\w+)\s+AS\s+(\w+)$/i)){
      const e=soil.get(m[1]);if(!e||!e.value||!e.value.__vein)storm('TYPE_STORM',`"${m[1]}" is not a VEIN`,line,column);
      soil.set(m[2],this.veinFS.read(e.value.file),'TX');
      this.emit(`ABSORB "${e.value.file}" ✓`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^INFUSE\s+(\w+)\s+WITH\s+(.+)$/i)){
      const e=soil.get(m[1]);if(!e||!e.value||!e.value.__vein)storm('TYPE_STORM',`"${m[1]}" is not a VEIN`,line,column);
      const val=String(E(m[2]));
      this.veinFS.append(e.value.file,val);
      if(e.value.mode==='MARK'){
        try{fs.appendFileSync(path.join(this.rootDir,e.value.file),val+'\n','utf8');}catch(err){}
      }
      this.emit(`INFUSE → "${val}"`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^SEAL\s+(\w+)$/i)){
      const e=soil.get(m[1]);
      if(e&&e.value&&e.value.__vein)this.emit(`SEAL "${e.value.file}" 🔒`,'muted');
      soil.delete(m[1]);return{next:i+1};
    }

    // WHENEVER
    if(m=stmt.match(/^WHENEVER\s+(\w+)\s+CHANGES$/i)){
      const watchName=m[1],body=this._collectBlock(stmts,i+1);
      if(!this.watchers.has(watchName))this.watchers.set(watchName,[]);
      this.watchers.get(watchName).push(body);
      this.emit(`WHENEVER "${watchName}" — watcher ✓`,'muted');
      return{next:i+1+body.length+1};
    }

    // WEATHER/SHELTER/CALM
    if(stmt.match(/^WEATHER$/i)){
      const weatherBody=this._collectBlock(stmts,i+1,true);
      let j=i+1+weatherBody.length;  // point to first stmt after body (SHELTER or CALM)
      const shelters={};
      while(j<stmts.length){
        const cs=stmts[j].text;let sm;
        if(sm=cs.match(/^SHELTER\s+(\w+)(?:\s+AS\s+(\w+))?$/i)){
          const sbody=this._collectBlock(stmts,j+1,true);  // stop at next SHELTER/CALM
          shelters[sm[1].toUpperCase()]={body:sbody,errVar:sm[2]};j+=sbody.length+2;continue;
        }
        if(cs.match(/^CALM$/i)){j++;break;}
        break;
      }
      let weatherResult=null;
    try{weatherResult=this._execBlock(weatherBody,0,weatherBody.length,soil.child());}
      catch(e){
        if(!(e instanceof PlantStorm))throw e;
        const handler=shelters[e.stormType]||shelters['ANY_STORM'];
        if(handler){
          const hs=soil.child();
          if(handler.errVar)hs.set(handler.errVar,e.message,'TX');
          const shelterResult=this._execBlock(handler.body,0,handler.body.length,hs);
          if(shelterResult&&shelterResult.returned){weatherResult=shelterResult;}
        }else this.emit(`⚡ unhandled ${e.stormType}: ${e.message}`,'error');
      }
      if(weatherResult&&weatherResult.returned)return{next:j,...weatherResult};
      return{next:j};
    }
    if(stmt.match(/^SHELTER\b/i)||stmt.match(/^CALM$/i)||stmt.match(/^STEADY$/i))return{next:i+1};

    // MATCH
    if(m=stmt.match(/^MATCH\s+(.+)$/i)){
      const matchVal=E(m[1]);
      let j=i+1,matched=false;
      while(j<stmts.length){
        const cs=stmts[j].text;
        if(cs.match(/^(\d+)\\\.?$/)||cs==='')break;
        let cm;
        if(!matched&&(cm=cs.match(/^IS\s+BETWEEN\s+(\S+)\s+(\S+)\s+YIELD\s+(.+)$/i))){
          if(+matchVal>=+E(cm[1])&&+matchVal<=+E(cm[2])){matched=true;const r=this._execYield(cm[3],stmts,j,soil);if(r&&r.returned)return{next:j+1,returned:true,value:r.value};}
        }else if(!matched&&(cm=cs.match(/^IS\s+"([^"]*)"\s+YIELD\s+(.+)$/i))){
          if(String(matchVal)===cm[1]){matched=true;const r=this._execYield(cm[2],stmts,j,soil);if(r&&r.returned)return{next:j+1,returned:true,value:r.value};}
        }else if(!matched&&(cm=cs.match(/^IS\s+(\S+)\s+YIELD\s+(.+)$/i))){
          if(E(cm[1])==matchVal){matched=true;this._execYield(cm[2],stmts,j,soil);}
        }else if(!matched&&(cm=cs.match(/^ELSE\s+YIELD\s+(.+)$/i))){
          matched=true;const r=this._execYield(cm[1],stmts,j,soil);if(r&&r.returned)return{next:j+1,returned:true,value:r.value};
        }
        j++;
      }
      return{next:j+1};
    }

    // IF / ORIF / ELSE — block and single-line forms
    if(m=stmt.match(/^STOP\s+IF\s+(.+?),\s*SHOW\s+"([^"]*)"$/i)){if(C(m[1])){this.emit(`STOP: ${m[2]}`,'warn');storm('STOP_STORM',m[2],line,column);}return{next:i+1};}
    // Single-line IF with GIVE
    if(m=stmt.match(/^IF\s+(.+?),\s*GIVE\s+(.+)$/i)){if(C(m[1]))return{next:i+1,returned:true,value:E(m[2])};return{next:i+1};}
    // Single-line IF with SHOW
    if(m=stmt.match(/^IF\s+(.+?),?\s+SHOW\s+"([^"]*)"$/i)){if(C(m[1]))this.emit(m[2]);return{next:i+1};}
    if(m=stmt.match(/^IF\s+(.+?),?\s+SHOW\s+(.+)$/i)){if(C(m[1]))this.emit(String(E(m[2])));return{next:i+1};}
    // Block IF — collect full IF/ORIF/ELSE chain, run first matching branch
    if(m=stmt.match(/^IF\s+(.+)$/i)){
      const myDepth=stmts[i]?stmts[i].depth:1;
      // Build chain: [{cond, body}]   cond===null means ELSE
      const chain=[];
      let j=i;
      while(j<stmts.length){
        const s=stmts[j];
        let cm,isIfLine=(j===i);
        if(isIfLine){
          cm=s.text.match(/^IF\s+(.+)$/i);
        }else{
          if(s.depth!==myDepth)break;
          cm=s.text.match(/^ORIF\s+(.+)$/i);
          if(!cm&&!s.text.match(/^ELSE$/i))break;
        }
        const branchCond=cm?cm[1].trim():null;
        const body=this._collectDepthBlock(stmts,j+1,myDepth);
        chain.push({cond:branchCond,body,start:j});
        j+=1+body.length;
      }
      // Run first matching branch
      let ran=false;
      for(const branch of chain){
        if(branch.cond===null||C(branch.cond)){
          if(branch.body.length){
            const r=this._execBlock(branch.body,0,branch.body.length,soil);
            if(r&&r.returned)return{next:j,...r};
          }
          ran=true;break;
        }
      }
      return{next:j};
    }
    if(stmt.match(/^(ELSE|ORIF)\b/i))return{next:i+1};

    // CYCLE x IN list
    if(m=stmt.match(/^CYCLE\s+(\w+)\s+IN\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[2]}" is not a LIST`,line,column);
      const body=this._collectBlock(stmts,i+1);
      for(const el of e.value){
        const cs=soil.child();cs.set(m[1],el,inferType(el));
        const r=this._execBlock(body,0,body.length,cs);
        if(r&&r.returned)return{next:i+1+body.length+1,...r};
      }
      return{next:i+1+body.length+1};
    }

    // CYCLE i FROM n TO m
    if(m=stmt.match(/^CYCLE\s+(\w+)\s+FROM\s+(.+?)\s+TO\s+(.+)$/i)){
      const from=+E(m[2]),to=+E(m[3]);
      const body=this._collectBlock(stmts,i+1);
      for(let n=from;n<=to;n++){
        const cs=soil.child();cs.set(m[1],n,'NUM');
        const r=this._execBlock(body,0,body.length,cs);
        if(r&&r.returned)return{next:i+1+body.length+1,...r};
      }
      return{next:i+1+body.length+1};
    }

    // SEASON
    if(m=stmt.match(/^SEASON\s+(.+)$/i)){
      const condE=m[1],body=this._collectBlock(stmts,i+1);
      let guard=0;
      while(C(condE)&&guard<10000){
        const cs=soil.child();
        const r=this._execBlock(body,0,body.length,cs);
        for(const[k,v]of cs._vars){if(soil.has(k))try{soil.update(k,v.value);}catch(e){}}
        if(r&&r.returned)return{next:i+1+body.length+1,...r};
        guard++;if(guard>=5000){this.emit('PRUNING: SEASON','warn');break;}
      }
      return{next:i+1+body.length+1};
    }

    // CONVERT
    if(m=stmt.match(/^CONVERT\s+(\w+)\s+TO\s+(NUM|SCL|TX|FACT)$/i)){
      const e=soil.get(m[1]);if(!e)storm('MISSING_STORM',`"${m[1]}"`,line,column);
      const t=m[2].toUpperCase();
      if(t==='NUM'){const cv=parseInt(e.value);if(isNaN(cv))storm('SEED_STORM','conversion failed',line,column);e.value=cv;}
      else if(t==='SCL')e.value=parseFloat(e.value);
      else if(t==='TX')e.value=String(e.value);
      else e.value=!!e.value;
      e.type=t;return{next:i+1};
    }

    // WAIT
    if(m=stmt.match(/^WAIT\s+(.+)$/i)){this.emit(`WAIT ${E(m[1])}s`,'muted');return{next:i+1};}
    if(stmt.startsWith('NOTE ')||stmt.startsWith('#'))return{next:i+1};

    storm('SEED_STORM',`unknown statement: "${stmt}"`,line,column);
  }

  // Collect statements deeper than parentDepth
  // ═══════════════════════════════════════════════════════════
  //  parseListenBranch — strict grammar parser for the web server
  //  signature:
  //
  //    LISTEN BRANCH ON [portExpr] WITH [configExpr] AS [requestIdent] MAP,
  //      ...bodyStatements...
  //    LISTEN/.
  //
  //  Raises SYNTAX_STORM, with a caret pointed at the exact missing
  //  or misspelled connective keyword, if the pipeline ON/WITH/AS/MAP
  //  is broken in any way. Returns a ListenBranchStatementNode.
  // ═══════════════════════════════════════════════════════════
  parseListenBranch(stmt,stmts,i){
    const stmtRec=stmts[i];
    const{line,column}=stmtRec;
    const text=stmt;

    // Header must start with LISTEN BRANCH — caller already verified this,
    // but we re-anchor here so offsets below are relative to a known prefix.
    const headerMatch=text.match(/^LISTEN\s+BRANCH\b/i);
    if(!headerMatch){
      storm('SYNTAX_STORM','LISTEN BRANCH: malformed header',line,column);
    }
    let cursor=headerMatch[0].length; // offset into `text` just past "LISTEN BRANCH"
    const rest=text.slice(cursor);

    // Stage 1 — ON [portExpr]
    let m=rest.match(/^\s+ON\s+/i);
    if(!m){
      // Find where ON should be (right after BRANCH) to aim the caret precisely
      const expectedOffset=cursor+rest.match(/^\s*/)[0].length;
      storm('SYNTAX_STORM',
        `LISTEN BRANCH: expected "ON" after BRANCH, found "${rest.trim().split(/\s+/)[0]||'(end of line)'}"`,
        line, subTokenColumn(stmtRec,expectedOffset));
    }
    let afterOn=rest.slice(m[0].length);
    let onOffsetEnd=cursor+m[0].length;

    // Stage 2 — portExpr WITH [configExpr]
    m=afterOn.match(/^(.+?)\s+WITH\s+/i);
    if(!m){
      // Distinguish: is WITH missing entirely, or did portExpr swallow into EOL?
      const wOffset=onOffsetEnd+afterOn.length;
      storm('SYNTAX_STORM',
        `LISTEN BRANCH: expected "WITH" after the port expression, found "${afterOn.trim().split(/\s+/).pop()||'(end of line)'}"`,
        line, subTokenColumn(stmtRec,wOffset));
    }
    const portExpr=m[1].trim();
    let afterWith=afterOn.slice(m[0].length);
    let withOffsetEnd=onOffsetEnd+m[0].length;

    // Stage 3 — configExpr AS [requestIdent]
    m=afterWith.match(/^(.+?)\s+AS\s+/i);
    if(!m){
      const aOffset=withOffsetEnd+afterWith.length;
      storm('SYNTAX_STORM',
        `LISTEN BRANCH: expected "AS" after the config expression, found "${afterWith.trim().split(/\s+/).pop()||'(end of line)'}"`,
        line, subTokenColumn(stmtRec,aOffset));
    }
    const configExpr=m[1].trim();
    let afterAs=afterWith.slice(m[0].length);
    let asOffsetEnd=withOffsetEnd+m[0].length;

    // Stage 4 — requestIdent MAP  (explicit type declaration required)
    m=afterAs.match(/^(\w+)\s+MAP\s*$/i);
    if(!m){
      const identMatch=afterAs.match(/^(\w+)/);
      const mapOffset=asOffsetEnd+(identMatch?identMatch[0].length:0);
      const found=afterAs.trim().split(/\s+/)[1]||'(missing)';
      storm('SYNTAX_STORM',
        `LISTEN BRANCH: expected request identifier followed by "MAP" (e.g. "req MAP"), found "${found}"`,
        line, subTokenColumn(stmtRec,mapOffset));
    }
    const requestIdent=m[1];

    // Header is grammatically valid — collect the nested body up to the
    // matching depth, sealed by "LISTEN/." (mirrors ACTION/SPECIES closers).
    let j=i+1;
    while(j<stmts.length&&!stmts[j].text.match(/^LISTEN\/\.?$/i))j++;
    const bodyStatements=stmts.slice(i+1,j);

    const node=new ListenBranchStatementNode({
      portExpr, configExpr, requestIdent, bodyStatements, line, column
    });
    node._closerIndex=j; // internal bookkeeping for the caller's {next:...}
    return node;
  }

  // GIVE [expr] AS RESPONSE.  →  ResponseStatementNode
  // Recognized as a distinct grammar from a normal GIVE so it can be
  // intercepted by handler bodies running inside a LISTEN BRANCH block
  // without being treated as an ordinary ACTION return value.
  parseResponseStatement(stmt,stmtRec){
    const m=stmt.match(/^GIVE\s+(.+?)\s+AS\s+RESPONSE$/i);
    if(!m)return null;
    return new ResponseStatementNode({
      responseExpr: m[1].trim(),
      line: stmtRec.line,
      column: stmtRec.column
    });
  }


  _collectDepthBlock(stmts,start,parentDepth){
    const body=[];let j=start;
    while(j<stmts.length){
      const s=stmts[j];
      if(s.depth<=parentDepth)break;
      body.push(s);j++;
    }
    return body;
  }

  _collectBlock(stmts,start,stopAtShelter=false){
    const body=[];let j=start;
    while(j<stmts.length){
      const cs=stmts[j].text;
      // Empty text = closing marker (e.g. "1\." stripped to "")
      if(!cs||/^[\\]+$/.test(cs)||cs.match(/^(\d+)\\.?$/)||cs.match(/^\/\d+.?$/))break;
      if(stopAtShelter&&(cs.match(/^SHELTER\b/i)||cs.match(/^CALM$/i)))break;
      body.push(stmts[j]);j++;
    }
    return body;
  }

  /**
   * _callAction(fn, argVals, instance, parentSoil)
   *
   * Invokes a registered ACTION. `fn.body` may be EITHER:
   *   - a legacy flat-statement array ({depth,text,line,column} records,
   *     produced by the original _firstPass()/core/lexer.js path), or
   *   - an AST-node array (objects with a `.type` field, produced by
   *     the new parseActionDeclaration() in core/parser.js).
   * This polymorphism is what lets every existing call site (FLOW,
   * REAP, SELF:method invocation, recursive self-calls) keep working
   * completely unchanged regardless of which pipeline registered the
   * function — the dispatch decision is made once, here, based on
   * whether the first body entry looks like an AST node or a legacy
   * statement record.
   */
  /**
   * Register the standard library FFI bridge functions for interpreted mode.
   * Each stub mirrors the C bridge in core/runtime_bridge.c.
   * Keys must match the lowercase action names declared in std/io.plnt.
   */
  _registerStdStubs(){
    // plant_printf(s(TX)) -> external — prints TX without newline
    this._externalFFI.set('plant_printf',(args)=>{
      const out=String(args[0]!==undefined?args[0]:'');
      process.stdout.write(out);
      this.emit(out,'inf');
      return out.length;
    });
    // plant_puts(s(TX)) -> external — prints TX with newline
    this._externalFFI.set('plant_puts',(args)=>{
      const out=String(args[0]!==undefined?args[0]:'')+'\n';
      process.stdout.write(out);
      this.emit(out,'inf');
      return out.length;
    });
    // plant_flush() -> external — flushes stdout
    this._externalFFI.set('plant_flush',()=>{
      process.stdout.write('');
      return 0;
    });
    // get_cli_arg(idx(NUM)) -> external — reads CLI args passed to the script
    // Falls back to 'test.plant' when the requested index is out of range.
    this._externalFFI.set('get_cli_arg',(args)=>{
      const idx=Number(args[0])||0;
      return this._cliArgs[idx]!==undefined?this._cliArgs[idx]:'test.plant';
    });
    // _map_get(m, key(TX)) -> external — map/object field access (handles JS Map and plain objects)
    this._externalFFI.set('_map_get',(args)=>{
      const obj=args[0];
      const key=args[1];
      if(obj instanceof Map)return obj.get(key);
      return obj&&obj[key];
    });
  }

  _callAction(fn,argVals,instance,parentSoil){
    // FFI external actions: look up JS stub in _externalFFI
    if(fn.isExternal){
      const stub=this._externalFFI.get(fn.name);
      if(stub){
        const result=stub(argVals,fn,parentSoil);
        return result!==undefined?{value:result}:{value:null};
      }
      storm('MISSING_STORM',
        `External FFI ACTION "${fn.name}" has no JS fallback in interpreted mode. `+
        `Use interpreter._externalFFI.set("${fn.name}", fn) or compile with LLVM.`,
        fn.line,0);
    }
    const scope=parentSoil.child();
    fn.params.forEach((p,idx)=>{if(argVals[idx]!==undefined)scope.set(p.name,argVals[idx],p.type);});
    if(instance){
      scope.set('__self',instance,'INSTANCE');
      // Bind the receiver variable name (e.g., "self") for receiver-based methods
      if(fn.receiverName){
        scope.set(fn.receiverName, instance, inferType(instance));
      }else{
        // Species methods use SELF:field convention
        Object.keys(instance).filter(k=>!k.startsWith('__')).forEach(k=>{
          scope.set('SELF:'+k,instance[k],inferType(instance[k]));
        });
      }
    }
    const isAstBody=fn.body.length>0&&typeof fn.body[0].type==='string'&&fn.body[0].text===undefined;
    let result;
    if(isAstBody){
      result=null;
      for(const bodyNode of fn.body){
        const r=this.evaluateNode(bodyNode,scope);
        if(r&&r.returned){result=r;break;}
      }
    }else{
      result=this._execBlock(fn.body,0,fn.body.length,scope);
    }
    if(instance){
      const self=scope.get('__self');
      if(self&&self.value)Object.assign(instance,self.value);
      // For receiver-based methods (struct methods), skip SELF: writeback
      // since member access modifies the struct directly.
      if(!fn.receiverName){
        Object.keys(instance).filter(k=>!k.startsWith('__')).forEach(k=>{
          const e=scope.get('SELF:'+k);if(e)instance[k]=e.value;
        });
      }
    }
    return result||{value:null};
  }

  _execYield(action,stmts,j,soil){
    const fake=[{depth:1,text:action,line:stmts[j].line}];
    try{return this._execOne(fake,0,1,soil);}catch(e){return null;}
  }

  // Split comma-separated args, respecting quoted strings
  _splitArgs(s){
    const args=[];let cur='',depth=0,inStr=false,strChar='';
    for(const ch of s){
      if(inStr){cur+=ch;if(ch===strChar)inStr=false;}
      else if(ch==='"'||ch==="'"){inStr=true;strChar=ch;cur+=ch;}
      else if(ch==='('||ch==='['||ch==='{'){{depth++;cur+=ch;}}
      else if(ch===')'||ch===']'||ch==='}'){{depth--;cur+=ch;}}
      else if(ch===','&&depth===0){args.push(cur.trim());cur='';}
      else cur+=ch;
    }
    if(cur.trim())args.push(cur.trim());
    return args;
  }

  _parseParams(s){
    return s.split(',').map(p=>p.trim()).filter(Boolean).map(p=>{
      const m=p.match(/(\w+)\((\w+)\)/);return m?{name:m[1],type:m[2].toUpperCase()}:{name:p,type:'ANY'};
    });
  }
}

// ── Array helpers (shared between interpreter and evaluator) ─────────────────
function isArrayTypeStr(s) {
  return typeof s === 'string' && s.startsWith('[') && s.endsWith(']') && s.length >= 3;
}
function arrayInnerType(s) {
  return isArrayTypeStr(s) ? s.slice(1, -1) : null;
}

// ── MAP type helpers ──────────────────────────────────────────────────────────
function isMapTypeStr(s) {
  return typeof s === 'string' && s.startsWith('MAP[') && s.endsWith(']');
}
function mapInnerTypes(s) {
  if (!isMapTypeStr(s)) return null;
  const inner = s.slice(4, -1);
  const parts = inner.split(',');
  if (parts.length !== 2) return null;
  return { keyType: parts[0].trim(), valueType: parts[1].trim() };
}

module.exports={Interpreter, isArrayTypeStr, arrayInnerType, isMapTypeStr, mapInnerTypes};
