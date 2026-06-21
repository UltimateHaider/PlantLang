'use strict';
const {storm,PlantStorm,inferType,coerce,Soil,VeinFS}=require('./runtime');
const {harvestSync,toPlantValue}=require('./harvest');
const {evalExpr,evalCond}=require('./evaluator');
const {INNATE}=require('./innate');
const {lex,subTokenColumn}=require('./lexer');
const {ListenBranchStatementNode,ResponseStatementNode}=require('./ast');
const fs=require('fs');
const path=require('path');

class Interpreter {
  constructor(opts={}){
    this.mission=opts.mission||'SAFE';
    this.soil=new Soil();
    this.veinFS=new VeinFS();
    this.funcs=new Map();
    this.species=new Map();
    this.planted=new Map();
    this.watchers=new Map();
    this.rootDir=opts.rootDir||process.cwd();
    this.output=opts.output||[];
    this.emit=opts.emit||((line,type)=>{this.output.push({text:line,type:type||'info'});});
    // VERIFY tracking
    this.verifyStats={passed:0,failed:0,suite:null,results:[]};
    this.veinFS.write('demo.txt','سطر أول\nسطر ثاني\nسطر ثالث');
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
    for(const stmtNode of programNode.statements){
      this.evaluateNode(stmtNode,this.soil);
    }
  }

  /** Parse source text via the new tokenizer/parser, then run it as an AST. */
  runSource(source){
    const {parse}=require('./parser');
    const programNode=parse(source);
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
      if(!e)storm('MISSING_STORM',`"${node.name}" غير موجود`,node.line,node.column);
      return e.value;
    }
    // Fallback: a raw string slipped through (shouldn't normally happen
    // once parseExpressionSpan always wraps in a Literal/Identifier).
    if(typeof node==='string')return evalExpr(node,soil);
    return null;
  }

  /** Central node router — delegator-executes each AST statement kind. */
  evaluateNode(node,soil){
    switch(node.type){
      case 'CreateStatement': return this.evaluateCreateStatement(node,soil);
      case 'ShowStatement':   return this.evaluateShowStatement(node,soil);
      case 'ListenBranchStatement': return this.evaluateListenBranch(node,soil);
      case 'ResponseStatement': return this.evaluateResponseStatement(node,soil);
      case 'RawStatement': {
        // Not yet migrated to a typed node — fall back to the proven
        // legacy single-statement executor so the AST path can still
        // run a complete real-world program during the migration.
        const fakeStmts=[{depth:node.depth,text:node.text,line:node.line,column:node.column}];
        return this._execOne(fakeStmts,0,1,soil);
      }
      default:
        storm('SYNTAX_STORM',`No evaluator registered for AST node type "${node.type}"`,node.line,node.column);
    }
  }

  /** evaluateCreateStatement(node, soil) — CREATE ident(TYPE) TO expr. */
  evaluateCreateStatement(node,soil){
    const value=node.valueExpr!==null
      ? this.evaluateExpressionNode(node.valueExpr,soil)
      : (node.varType==='LIST'?[]:node.varType==='MAP'?{}:node.varType==='FACT'?false:node.varType==='NUM'?0:'');
    soil.set(node.identifier,value,node.varType,{pulse:!!node.isPulse});
    this.emit(`CREATE "${node.identifier}"(${node.varType})${node.isPulse?' PULSE':''} = ${Array.isArray(value)?'['+value.join(', ')+']':value}`,'ok');
    return{next:1};
  }

  /** evaluateShowStatement(node, soil) — SHOW expr. */
  evaluateShowStatement(node,soil){
    const value=this.evaluateExpressionNode(node.expr,soil);
    const display=value&&typeof value==='object'?(Array.isArray(value)?'['+value.join(', ')+']':JSON.stringify(value)):String(value);
    this.emit(display,'inf');
    return{next:1};
  }

  /** evaluateListenBranch(node, soil) — bridges the AST node to the existing handler logic. */
  evaluateListenBranch(node,soil){
    this.emit(`LISTEN BRANCH ON ${node.portExpr} WITH ${node.configExpr} AS ${node.requestIdent} MAP — registered ✓`,'ok');
    const reqSoil=soil.child();
    reqSoil.set(node.requestIdent,{},'MAP');
    for(const bodyNode of node.bodyStatements){
      this.evaluateNode(bodyNode,reqSoil);
    }
    return{next:1};
  }

  /** evaluateResponseStatement(node, soil) — GIVE expr AS RESPONSE. */
  evaluateResponseStatement(node,soil){
    const value=evalExpr(node.responseExpr,soil);
    this.emit(`  → RESPONSE: ${value&&typeof value==='object'?JSON.stringify(value):value}`,'muted');
    return{next:1,returned:true,value};
  }

  runFile(filePath){
    const source=fs.readFileSync(filePath,'utf8');
    this.rootDir=path.dirname(filePath);
    this.run(source);
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
        this.emit(`✓ ROOT_SCOPE "${scopeName}" — ${Object.keys(map).length} مفاتيح`,'ok');
        i=j+1;continue;
      }
      if(m=text.match(/^MISSION\s*:\s*(\w+)$/i)){this.mission=m[1].toUpperCase();i++;continue;}
      if(m=text.match(/^PLANT\s+(\w+)(?:\s+AS\s+(\w+))?$/i)){
        const libName=m[1].toLowerCase(),alias=m[2]||m[1];
        if(INNATE[libName]){this.planted.set(alias,INNATE[libName]);this.emit(`✓ PLANT "${m[1]}"`, 'ok');}
        else this.emit(`⚠ PLANT "${m[1]}" — غير موجود`,'warn');
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
    if(m=stmt.match(/^SHOW\s+NOW$/)){this.emit(new Date().toLocaleString('ar-IQ'));return{next:i+1};}
    if(m=stmt.match(/^SHOW\s+(COUNT|SUM|MAX|MIN|FIRST|LAST)\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e)storm('MISSING_STORM',`"${m[2]}" غير موجود`,line,column);
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
      if(!e||!e.value)storm('MISSING_STORM',`"${m[1]}" غير موجود`,line,column);
      const _rv=e.value[m[2]];const _rd=(_rv&&typeof _rv==='object')?(Array.isArray(_rv)?'['+_rv.join(', ')+']':JSON.stringify(_rv)):String(_rv===undefined?'void':_rv);this.emit(`${m[1]}:"${m[2]}" = ${_rd}`,'inf');return{next:i+1};
    }
    if(m=stmt.match(/^SHOW\s+(\w+):(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!e.value)storm('MISSING_STORM',`"${m[1]}" غير موجود`,line,column);
      const _rv=e.value[m[2]];const _rd=(_rv&&typeof _rv==='object')?(Array.isArray(_rv)?'['+_rv.join(', ')+']':JSON.stringify(_rv)):String(_rv===undefined?'void':_rv);this.emit(`${m[1]}:${m[2]} = ${_rd}`,'inf');return{next:i+1};
    }
    if(m=stmt.match(/^SHOW\s+(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e)storm('MISSING_STORM',`"${m[1]}" غير موجود`,line,column);
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
      if(!selfEntry)storm('SEED_STORM','SELF غير متاح',line,column);
      const newVal=E(m[2]);
      selfEntry.value[m[1]]=newVal;
      const scoped=soil.get('SELF:'+m[1]);
      if(scoped)scoped.value=newVal;else soil.set('SELF:'+m[1],newVal);
      return{next:i+1};
    }
    // SET obj:"key" TO  (quoted MAP key)
    if(m=stmt.match(/^SET\s+(\w+):"([^"]*)"\s+TO\s+(.+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!e.value||typeof e.value!=='object')storm('TYPE_STORM',`"${m[1]}" ليس كائناً`,line,column);
      if(e.locked)storm('LOCK_STORM',`"${m[1]}" محمي — لا يمكن تعديله`,line,column);
      e.value[m[2]]=E(m[3]);
      this.emit(`SET ${m[1]}:"${m[2]}" → ${e.value[m[2]]}`,'ok');return{next:i+1};
    }
    // SET inst:prop
    if(m=stmt.match(/^SET\s+(\w+):(\w+)\s+TO\s+(.+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!e.value||typeof e.value!=='object')storm('TYPE_STORM',`"${m[1]}" ليس كائناً`,line,column);
      if(e.locked)storm('LOCK_STORM',`"${m[1]}" محمي — لا يمكن تعديله`,line,column);
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
      if(!selfEntry)storm('SEED_STORM','SELF غير متاح',line,column);
      const n=+E(m[3]),prop=m[2];
      const newVal=m[1].toUpperCase()==='INCREASE'?(+selfEntry.value[prop]||0)+n:(+selfEntry.value[prop]||0)-n;
      selfEntry.value[prop]=newVal;
      // Also update SELF:prop in scope so GIVE SELF:prop works
      const selfPropEntry=soil.get('SELF:'+prop);
      if(selfPropEntry)selfPropEntry.value=newVal;
      else soil.set('SELF:'+prop,newVal);
      return{next:i+1};
    }
    if(m=stmt.match(/^(INCREASE|DECREASE)\s+(\w+)\s+BY\s+(.+)$/i)){
      const e=soil.get(m[2]);
      if(!e)storm('MISSING_STORM',`"${m[2]}" غير موجود`,line,column);
      if(Array.isArray(e.value)||typeof e.value==='object')storm('TYPE_STORM',`لا يمكن ${m[1]} على ${e.type}`,line,column);
      const n=+E(m[3]);
      e.value=m[1].toUpperCase()==='INCREASE'?(+e.value||0)+n:(+e.value||0)-n;
      return{next:i+1};
    }

    // LIST ops
    // PUT val INTO SELF:list
    if(m=stmt.match(/^PUT\s+(.+?)\s+INTO\s+SELF:(\w+)$/i)){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF غير متاح',line,column);
      if(!Array.isArray(selfEntry.value[m[2]]))storm('TYPE_STORM',`SELF:${m[2]} ليس LIST`,line,column);
      selfEntry.value[m[2]].push(E(m[1]));return{next:i+1};
    }
    if(m=stmt.match(/^PUT\s+(.+?)\s+INTO\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[2]}" ليس LIST`,line,column);
      e.value.push(E(m[1]));return{next:i+1};
    }
    if(m=stmt.match(/^TAKE\s+(.+?)\s+FROM\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[2]}" ليس LIST`,line,column);
      const idx=e.value.indexOf(E(m[1]));
      if(idx===-1)storm('LOST_STORM',`عنصر غير موجود في "${m[2]}"`,line,column);
      e.value.splice(idx,1);return{next:i+1};
    }
    if(m=stmt.match(/^SORT\s+(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[1]}" ليس LIST`,line,column);
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
      if(!selfEntry)storm('SEED_STORM','SELF غير متاح',line,column);
      // m[1] can be a quoted string "key" or a variable name
      const mapKey=String(E(m[1]));
      const mapVal=E(m[2]);
      if(!selfEntry.value[m[3]]||typeof selfEntry.value[m[3]]!=='object')selfEntry.value[m[3]]={};
      selfEntry.value[m[3]][mapKey]=mapVal;
      return{next:i+1};
    }
    if(m=stmt.match(/^LINK\s+"([^"]+)"\s+WITH\s+(.+?)\s+IN\s+(\w+)$/i)){
      const e=soil.get(m[3]);
      if(!e||typeof e.value!=='object'||Array.isArray(e.value))storm('TYPE_STORM',`"${m[3]}" ليس MAP`,line,column);
      e.value[m[1]]=E(m[2]);this.emit(`LINK "${m[1]}" → ${m[3]}`,'ok');return{next:i+1};
    }

    // REAP SELF:method (call own method inside ACTION)
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+SELF:(\w+)(?:,\s*(.*))?$/i)){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF غير متاح',line,column);
      const spName=selfEntry.value.__species;
      const sp=spName&&this.species.get(spName);
      if(!sp)storm('MISSING_STORM','SPECIES غير موجود',line,column);
      const method=sp.actions[m[2]];
      if(!method)storm('MISSING_STORM',`الفعل "${m[2]}" غير موجود`,line,column);
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
          else this.emit(`FLOW: "${_st}" غير معرّف`,'warn');
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
        if(!fn)storm('MISSING_STORM',`"${funcName}" غير موجود في "${libOrObj}"`,line,column);
        const argVals=rawArgs?this._splitArgs(rawArgs).map(a=>E(a.trim())):[];
        const result=fn(argVals);
        if(resName!=='_')soil.set(resName,result,inferType(result));
        return{next:i+1};
      }
      const instEntry=soil.get(libOrObj);
      if(!instEntry)storm('MISSING_STORM',`"${libOrObj}" غير موجود`,line,column);
      const spName=instEntry.value&&instEntry.value.__species;
      const sp=spName&&this.species.get(spName);
      if(!sp)storm('MISSING_STORM',`SPECIES غير موجود`,line,column);
      const method=sp.actions[funcName];
      if(!method)storm('MISSING_STORM',`الفعل "${funcName}" غير موجود`,line,column);
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
            storm('MISSING_STORM',`"${testExpr}" غير موجود`,line,column);
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
      if(!e)storm('MISSING_STORM',`"${m[1]}" غير موجود`,line,column);
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
        this.emit(`ANALYZE "${m[1]}" → MAP{${keys.length} مفاتيح}`,'inf');
        for(const k of keys)this.emit(`  "${k}": ${v[k]} (${inferType(v[k])})`,'muted');
      }else if(t==='TX'||typeof v==='string'){
        this.emit(`ANALYZE "${m[1]}" → TX[${[...String(v)].length} حرف]`,'inf');
        this.emit(`  "${v}"`,'muted');
      }else if(v&&v.__species){
        const fields=Object.keys(v).filter(k=>!k.startsWith('__'));
        this.emit(`ANALYZE "${m[1]}" → INSTANCE(${v.__species}){${fields.length} خصائص}`,'inf');
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
      const val=fmt==='DATE'?now.toLocaleDateString('ar-IQ'):fmt==='TIME'?now.toLocaleTimeString('ar-IQ'):fmt==='YEAR'?now.getFullYear():fmt==='STAMP'?now.getTime():now.toLocaleString('ar-IQ');
      if(m[1]!=='_')soil.set(m[1],val,fmt==='STAMP'?'NUM':'TX');return{next:i+1};
    }

    // REAP ACTION
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+(\w+)(?:,\s*(.*))?$/i)){
      const resName=m[1],fnName=m[2],rawArgs=m[3]||'';
      if(!this.funcs.has(fnName))storm('MISSING_STORM',`الفعل "${fnName}" غير معرّف`,line,column);
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
      if(!sp)storm('MISSING_STORM',`SPECIES "${m[1]}" غير معرّفة`,line,column);
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

      if(!result.success){
        storm('NETWORK_STORM',`HARVEST failed: ${result.error}`,line,column);
      }

      const plantVal=toPlantValue(result.data);
      const valType=Array.isArray(plantVal)?'LIST':(plantVal&&typeof plantVal==='object')?'MAP':'TX';
      if(resName!=='_')soil.set(resName,plantVal,valType);

      this.emit(`HARVEST ${method} ${url} → ${result.status} (${valType})`,result.ok?'ok':'warn');
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
      const e=soil.get(m[1]);if(!e||!e.value||!e.value.__vein)storm('TYPE_STORM',`"${m[1]}" ليس VEIN`,line,column);
      const lines=this.veinFS.read(e.value.file).split('\n');
      const pos=e.value.pos||0,lineContent=lines[pos]||'';
      e.value.pos=pos+1;soil.set(m[2],lineContent,'TX');
      this.emit(`ABSORB LINE → "${lineContent}"`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^ABSORB\s+(\w+)\s+AS\s+(\w+)$/i)){
      const e=soil.get(m[1]);if(!e||!e.value||!e.value.__vein)storm('TYPE_STORM',`"${m[1]}" ليس VEIN`,line,column);
      soil.set(m[2],this.veinFS.read(e.value.file),'TX');
      this.emit(`ABSORB "${e.value.file}" ✓`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^INFUSE\s+(\w+)\s+WITH\s+(.+)$/i)){
      const e=soil.get(m[1]);if(!e||!e.value||!e.value.__vein)storm('TYPE_STORM',`"${m[1]}" ليس VEIN`,line,column);
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
      this.emit(`WHENEVER "${watchName}" — مراقب ✓`,'muted');
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
    // Block IF — body is next deeper-depth statements
    if(m=stmt.match(/^IF\s+(.+)$/i)){
      const cond=m[1].trim();
      const myDepth=stmts[i]?stmts[i].depth:1;
      const body=this._collectDepthBlock(stmts,i+1,myDepth);
      if(body.length>0){
        // Block form - run in same scope so mutations propagate
        if(C(cond)){
          const r=this._execBlock(body,0,body.length,soil);
          if(r&&r.returned)return{next:i+1+body.length,...r};
        }
        return{next:i+1+body.length};
      }
      return{next:i+1};
    }
    if(stmt.match(/^(ELSE|ORIF)\b/i))return{next:i+1};

    // CYCLE x IN list
    if(m=stmt.match(/^CYCLE\s+(\w+)\s+IN\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[2]}" ليس LIST`,line,column);
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
      if(t==='NUM'){const cv=parseInt(e.value);if(isNaN(cv))storm('SEED_STORM','تعذر التحويل',line,column);e.value=cv;}
      else if(t==='SCL')e.value=parseFloat(e.value);
      else if(t==='TX')e.value=String(e.value);
      else e.value=!!e.value;
      e.type=t;return{next:i+1};
    }

    // WAIT
    if(m=stmt.match(/^WAIT\s+(.+)$/i)){this.emit(`WAIT ${E(m[1])}s`,'muted');return{next:i+1};}
    if(stmt.startsWith('NOTE ')||stmt.startsWith('#'))return{next:i+1};

    storm('SEED_STORM',`جملة غير معروفة: "${stmt}"`,line,column);
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

  _callAction(fn,argVals,instance,parentSoil){
    const scope=parentSoil.child();
    fn.params.forEach((p,idx)=>{if(argVals[idx]!==undefined)scope.set(p.name,argVals[idx],p.type);});
    if(instance){
      scope.set('__self',instance,'INSTANCE');
      Object.keys(instance).filter(k=>!k.startsWith('__')).forEach(k=>{
        scope.set('SELF:'+k,instance[k],inferType(instance[k]));
      });
    }
    const result=this._execBlock(fn.body,0,fn.body.length,scope);
    if(instance){
      const self=scope.get('__self');
      if(self&&self.value)Object.assign(instance,self.value);
      Object.keys(instance).filter(k=>!k.startsWith('__')).forEach(k=>{
        const e=scope.get('SELF:'+k);if(e)instance[k]=e.value;
      });
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
      else if(ch==='('||ch==='['){{depth++;cur+=ch;}}
      else if(ch===')'||ch===']'){{depth--;cur+=ch;}}
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

module.exports={Interpreter};
