'use strict';
const {storm,PlantStorm,inferType,coerce,Soil,VeinFS}=require('./runtime');
const {evalExpr,evalCond}=require('./evaluator');
const {INNATE}=require('./innate');
const {lex}=require('./lexer');
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
    this.veinFS.write('demo.txt','سطر أول\nسطر ثاني\nسطر ثالث');
  }

  run(source){
    const stmts=lex(source);
    this._firstPass(stmts);
    this._execBlock(stmts,0,stmts.length,this.soil);
  }

  runFile(filePath){
    const source=fs.readFileSync(filePath,'utf8');
    this.rootDir=path.dirname(filePath);
    this.run(source);
  }

  _firstPass(stmts){
    let i=0;
    while(i<stmts.length){
      const{text,line}=stmts[i];
      let m;
      if(m=text.match(/^ROOT\s+(\w+)\s+TO\s+(.+)$/i)){
        this.soil.set(m[1],evalExpr(m[2],this.soil),null,{locked:true});
        i++;continue;
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
    const{text,line}=stmts[i];
    if(!text||text.startsWith('#'))return{next:i+1};
    if(text.match(/^\/ACTION\.?$/i)||text.match(/^\/SPECIES\.?$/i)||
       text.match(/^(\d+)\\\.?$/)||text.match(/^\/\d+\.?$/))return{next:i+1};
    if(text.match(/^ACTION\s+\w+\(/i)){let j=i+1;while(j<stmts.length&&!stmts[j].text.match(/^\/ACTION\.?$/i))j++;return{next:j+1};}
    if(text.match(/^SPECIES\s+/i)){let j=i+1;while(j<stmts.length&&!stmts[j].text.match(/^\/SPECIES\.?$/i))j++;return{next:j+1};}
    if(text.match(/^ROOT\s+\w+\s+TO/i)||text.match(/^PLANT\s+/i)||text.match(/^MISSION\s*:/i))return{next:i+1};
    try{return this._exec(text,stmts,i,soil,line)||{next:i+1};}
    catch(e){
      if(e instanceof PlantStorm)throw e;
      this.emit(`⚡ ${e.message}`,'error');
      return{next:i+1};
    }
  }

  _exec(stmt,stmts,i,soil,line){
    const E=(expr)=>evalExpr(expr,soil);
    const C=(cond)=>evalCond(cond,soil);
    let m;

    if(m=stmt.match(/^GIVE\s+(.+)$/i))return{next:i+1,returned:true,value:E(m[1])};

    // SHOW
    if(m=stmt.match(/^SHOW\s+"([^"]*)"$/i)){this.emit(m[1]);return{next:i+1};}
    if(m=stmt.match(/^SHOW\s+NOW$/i)){this.emit(new Date().toLocaleString('ar-IQ'));return{next:i+1};}
    if(m=stmt.match(/^SHOW\s+(COUNT|SUM|MAX|MIN|FIRST|LAST)\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e)storm('MISSING_STORM',`"${m[2]}" غير موجود`,line);
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
    if(m=stmt.match(/^SHOW\s+(\w+):(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!e.value)storm('MISSING_STORM',`"${m[1]}" غير موجود`,line);
      this.emit(`${m[1]}:${m[2]} = ${e.value[m[2]]}`);return{next:i+1};
    }
    if(m=stmt.match(/^SHOW\s+(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e)storm('MISSING_STORM',`"${m[1]}" غير موجود`,line);
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
      if(!selfEntry)storm('SEED_STORM','SELF غير متاح',line);
      const newVal=E(m[2]);
      selfEntry.value[m[1]]=newVal;
      const scoped=soil.get('SELF:'+m[1]);
      if(scoped)scoped.value=newVal;else soil.set('SELF:'+m[1],newVal);
      return{next:i+1};
    }
    // SET inst:prop
    if(m=stmt.match(/^SET\s+(\w+):(\w+)\s+TO\s+(.+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!e.value||typeof e.value!=='object')storm('TYPE_STORM',`"${m[1]}" ليس كائناً`,line);
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
      if(!selfEntry)storm('SEED_STORM','SELF غير متاح',line);
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
      if(!e)storm('MISSING_STORM',`"${m[2]}" غير موجود`,line);
      if(Array.isArray(e.value)||typeof e.value==='object')storm('TYPE_STORM',`لا يمكن ${m[1]} على ${e.type}`,line);
      const n=+E(m[3]);
      e.value=m[1].toUpperCase()==='INCREASE'?(+e.value||0)+n:(+e.value||0)-n;
      return{next:i+1};
    }

    // LIST ops
    // PUT val INTO SELF:list
    if(m=stmt.match(/^PUT\s+(.+?)\s+INTO\s+SELF:(\w+)$/i)){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF غير متاح',line);
      if(!Array.isArray(selfEntry.value[m[2]]))storm('TYPE_STORM',`SELF:${m[2]} ليس LIST`,line);
      selfEntry.value[m[2]].push(E(m[1]));return{next:i+1};
    }
    if(m=stmt.match(/^PUT\s+(.+?)\s+INTO\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[2]}" ليس LIST`,line);
      e.value.push(E(m[1]));return{next:i+1};
    }
    if(m=stmt.match(/^TAKE\s+(.+?)\s+FROM\s+(\w+)$/i)){
      const e=soil.get(m[2]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[2]}" ليس LIST`,line);
      const idx=e.value.indexOf(E(m[1]));
      if(idx===-1)storm('LOST_STORM',`عنصر غير موجود في "${m[2]}"`,line);
      e.value.splice(idx,1);return{next:i+1};
    }
    if(m=stmt.match(/^SORT\s+(\w+)$/i)){
      const e=soil.get(m[1]);
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[1]}" ليس LIST`,line);
      e.value.sort((a,b)=>typeof a==='number'&&typeof b==='number'?a-b:String(a).localeCompare(String(b)));
      return{next:i+1};
    }
    if(m=stmt.match(/^SHAKE\s+(\w+)$/i)){
      const e=soil.get(m[1]);if(!e||!Array.isArray(e.value))storm('TYPE_STORM','',line);
      for(let j=e.value.length-1;j>0;j--){const k=Math.floor(Math.random()*(j+1));[e.value[j],e.value[k]]=[e.value[k],e.value[j]];}
      return{next:i+1};
    }
    if(m=stmt.match(/^EMPTY\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e)storm('MISSING_STORM','',line);e.value=Array.isArray(e.value)?[]:'';return{next:i+1};}
    if(m=stmt.match(/^EVAPORATE\s+(\w+)$/i)){soil.delete(m[1]);this.emit(`EVAPORATE "${m[1]}" ✓`,'muted');return{next:i+1};}
    if(m=stmt.match(/^LOCK\s+(\w+)$/i)){const e=soil.get(m[1]);if(e){e.locked=true;this.emit(`LOCK "${m[1]}" 🔒`,'warn');}return{next:i+1};}

    // MAP
    if(m=stmt.match(/^LINK\s+"([^"]+)"\s+WITH\s+(.+?)\s+IN\s+(\w+)$/i)){
      const e=soil.get(m[3]);
      if(!e||typeof e.value!=='object'||Array.isArray(e.value))storm('TYPE_STORM',`"${m[3]}" ليس MAP`,line);
      e.value[m[1]]=E(m[2]);this.emit(`LINK "${m[1]}" → ${m[3]}`,'ok');return{next:i+1};
    }

    // REAP SELF:method (call own method inside ACTION)
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+SELF:(\w+)(?:,\s*(.*))?$/i)){
      const selfEntry=soil.get('__self');
      if(!selfEntry)storm('SEED_STORM','SELF غير متاح',line);
      const spName=selfEntry.value.__species;
      const sp=spName&&this.species.get(spName);
      if(!sp)storm('MISSING_STORM','SPECIES غير موجود',line);
      const method=sp.actions[m[2]];
      if(!method)storm('MISSING_STORM',`الفعل "${m[2]}" غير موجود`,line);
      const rawArgs=m[3]||'';
      const argVals=rawArgs?this._splitArgs(rawArgs).map(a=>E(a.trim())):[];
      const result=this._callAction(method,argVals,selfEntry.value,soil);
      const val=result.value!==undefined?result.value:null;
      if(m[1]!=='_')soil.set(m[1],val,inferType(val));
      return{next:i+1};
    }
    // REAP lib:FUNC
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+(\w+):(\w+)(?:,\s*(.*))?$/i)){
      const resName=m[1],libOrObj=m[2],funcName=m[3],rawArgs=m[4]||'';
      if(this.planted.has(libOrObj)){
        const lib=this.planted.get(libOrObj);
        const fn=lib[funcName.toUpperCase()];
        if(!fn)storm('MISSING_STORM',`"${funcName}" غير موجود في "${libOrObj}"`,line);
        const argVals=rawArgs?this._splitArgs(rawArgs).map(a=>E(a.trim())):[];
        const result=fn(argVals);
        if(resName!=='_')soil.set(resName,result,inferType(result));
        return{next:i+1};
      }
      const instEntry=soil.get(libOrObj);
      if(!instEntry)storm('MISSING_STORM',`"${libOrObj}" غير موجود`,line);
      const spName=instEntry.value&&instEntry.value.__species;
      const sp=spName&&this.species.get(spName);
      if(!sp)storm('MISSING_STORM',`SPECIES غير موجود`,line);
      const method=sp.actions[funcName];
      if(!method)storm('MISSING_STORM',`الفعل "${funcName}" غير موجود`,line);
      const argVals=rawArgs?this._splitArgs(rawArgs).map(a=>E(a.trim())):[];
      const result=this._callAction(method,argVals,instEntry.value,soil);
      if(resName!=='_')soil.set(resName,result.value!==undefined?result.value:null,inferType(result.value));
      return{next:i+1};
    }

    // REAP ACTION
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+(\w+)(?:,\s*(.*))?$/i)){
      const resName=m[1],fnName=m[2],rawArgs=m[3]||'';
      if(!this.funcs.has(fnName))storm('MISSING_STORM',`الفعل "${fnName}" غير معرّف`,line);
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
      if(!sp)storm('MISSING_STORM',`SPECIES "${m[1]}" غير معرّفة`,line);
      const inst={__species:m[1],__parent:sp.parent};
      Object.entries(sp.fields).forEach(([fn,fd])=>{
        inst[fn]=fd.default!==null?fd.default:fd.type==='NUM'?0:fd.type==='LIST'?[]:fd.type==='FACT'?false:'';
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

    // NOW
    if(m=stmt.match(/^REAP\s+(\w+)\s+FROM\s+NOW(?:\s+FORMAT:(\w+))?$/i)){
      const fmt=(m[2]||'FULL').toUpperCase(),now=new Date();
      const val=fmt==='DATE'?now.toLocaleDateString('ar-IQ'):fmt==='TIME'?now.toLocaleTimeString('ar-IQ'):fmt==='YEAR'?now.getFullYear():fmt==='STAMP'?now.getTime():now.toLocaleString('ar-IQ');
      soil.set(m[1],val,fmt==='STAMP'?'NUM':'TX');return{next:i+1};
    }

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
      const e=soil.get(m[1]);if(!e||!e.value||!e.value.__vein)storm('TYPE_STORM',`"${m[1]}" ليس VEIN`,line);
      const lines=this.veinFS.read(e.value.file).split('\n');
      const pos=e.value.pos||0,lineContent=lines[pos]||'';
      e.value.pos=pos+1;soil.set(m[2],lineContent,'TX');
      this.emit(`ABSORB LINE → "${lineContent}"`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^ABSORB\s+(\w+)\s+AS\s+(\w+)$/i)){
      const e=soil.get(m[1]);if(!e||!e.value||!e.value.__vein)storm('TYPE_STORM',`"${m[1]}" ليس VEIN`,line);
      soil.set(m[2],this.veinFS.read(e.value.file),'TX');
      this.emit(`ABSORB "${e.value.file}" ✓`,'ok');return{next:i+1};
    }
    if(m=stmt.match(/^INFUSE\s+(\w+)\s+WITH\s+(.+)$/i)){
      const e=soil.get(m[1]);if(!e||!e.value||!e.value.__vein)storm('TYPE_STORM',`"${m[1]}" ليس VEIN`,line);
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
      try{this._execBlock(weatherBody,0,weatherBody.length,soil.child());}
      catch(e){
        if(!(e instanceof PlantStorm))throw e;
        const handler=shelters[e.stormType]||shelters['ANY_STORM'];
        if(handler){
          const hs=soil.child();
          if(handler.errVar)hs.set(handler.errVar,e.message,'TX');
          this._execBlock(handler.body,0,handler.body.length,hs);
        }else this.emit(`⚡ unhandled ${e.stormType}: ${e.message}`,'error');
      }
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
    if(m=stmt.match(/^STOP\s+IF\s+(.+?),\s*SHOW\s+"([^"]*)"$/i)){if(C(m[1])){this.emit(`STOP: ${m[2]}`,'warn');storm('STOP_STORM',m[2],line);}return{next:i+1};}
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
      if(!e||!Array.isArray(e.value))storm('TYPE_STORM',`"${m[2]}" ليس LIST`,line);
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
      const e=soil.get(m[1]);if(!e)storm('MISSING_STORM',`"${m[1]}"`,line);
      const t=m[2].toUpperCase();
      if(t==='NUM'){const cv=parseInt(e.value);if(isNaN(cv))storm('SEED_STORM','تعذر التحويل',line);e.value=cv;}
      else if(t==='SCL')e.value=parseFloat(e.value);
      else if(t==='TX')e.value=String(e.value);
      else e.value=!!e.value;
      e.type=t;return{next:i+1};
    }

    // WAIT
    if(m=stmt.match(/^WAIT\s+(.+)$/i)){this.emit(`WAIT ${E(m[1])}s`,'muted');return{next:i+1};}
    if(stmt.startsWith('NOTE ')||stmt.startsWith('#'))return{next:i+1};

    storm('SEED_STORM',`جملة غير معروفة: "${stmt}"`,line);
  }

  // Collect statements deeper than parentDepth
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
      if(!cs||cs==='\\'||cs.match(/^(\d+)\\\.?$/)||cs.match(/^\/\d+\.?$/))break;
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
