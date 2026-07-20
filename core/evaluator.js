'use strict';
const {storm,inferType,coerce}=require('./runtime');

function evalExpr(expr,soil){
  if(expr===undefined||expr===null)return null;
  expr=String(expr).trim();
  if(/^"[^"]*"$/.test(expr)||/^'[^']*'$/.test(expr))return expr.slice(1,-1);
  if(expr==='TRUE'||expr==='true')return true;
  if(expr==='FALSE'||expr==='false')return false;
  if(/^FACT:TRUE$/i.test(expr))return true;
  if(/^FACT:FALSE$/i.test(expr))return false;
  if(expr==='NULL'||expr==='VOID')return null;
  if(!isNaN(Number(expr))&&expr!=='')return Number(expr);
  let m;
  if(m=expr.match(/^PICK\s+\((.+?)\)\s+"([^"]*)"\s+"([^"]*)"$/i))return evalCond(m[1],soil)?m[2]:m[3];
  if(m=expr.match(/^PICK\s+\((.+?)\)\s+(\S+)\s+(\S+)$/i))return evalCond(m[1],soil)?coerce(m[2]):coerce(m[3]);
  if(m=expr.match(/^COUNT\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e)return 0;return Array.isArray(e.value)?e.value.length:(e.value&&typeof e.value==='object'?Object.keys(e.value).length:1);}
  if(m=expr.match(/^COUNT\s+SELF:(\w+)$/i)){const self=soil.get('__self');if(!self||!self.value)return 0;const v=self.value[m[1]];return Array.isArray(v)?v.length:(v&&typeof v==='object'?Object.keys(v).length:1);}
  if(m=expr.match(/^FIRST\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e)return null;return Array.isArray(e.value)?e.value[0]:e.value;}
  if(m=expr.match(/^LAST\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e)return null;const v=e.value;return Array.isArray(v)?v[v.length-1]:v;}
  if(m=expr.match(/^LENGTH\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e)return 0;return Array.isArray(e.value)?e.value.length:String(e.value||'').length;}
  if(m=expr.match(/^SUM\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e||!Array.isArray(e.value))return 0;return e.value.reduce((a,b)=>a+(+b||0),0);}
  if(m=expr.match(/^MAX\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e||!Array.isArray(e.value))return null;return Math.max(...e.value.map(Number));}
  if(m=expr.match(/^MIN\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e||!Array.isArray(e.value))return null;return Math.min(...e.value.map(Number));}
  // SELF:prop — must check before generic inst:prop
  if(m=expr.match(/^SELF:(\w+)$/i)){
    const self=soil.get('__self');
    if(self&&self.value&&self.value[m[1]]!==undefined)return self.value[m[1]];
    // fallback to scoped SELF:prop entry
    const scoped=soil.get('SELF:'+m[1]);
    return scoped?scoped.value:null;
  }
  // Chained MAP access: obj:"k1":"k2":"k3" (any depth)
  if(/^\w+:"/.test(expr)){
    const parts=[];
    let s=expr;
    const firstId=s.match(/^(\w+)/);
    if(firstId){
      let cur=soil.get(firstId[1]);
      if(cur!==null){
        let val=cur.value;
        s=s.slice(firstId[0].length);
        while(s.startsWith(':"')){
          const km=s.match(/^:"([^"]*)"/);
          if(!km)break;
          if(val&&typeof val==='object'&&!Array.isArray(val))val=val[km[1]];
          else{val=null;break;}
          s=s.slice(km[0].length);
        }
        if(!s)return val!==undefined?val:null;
      }
    }
  }
  if(m=expr.match(/^(\w+):"([^"]*)"$/)){const e=soil.get(m[1]);if(e&&e.value&&typeof e.value==='object'&&!Array.isArray(e.value))return e.value[m[2]]!==undefined?e.value[m[2]]:null;return null;}
  if(m=expr.match(/^(\w+):(\w+)$/)){const e=soil.get(m[1]);if(e&&e.value&&typeof e.value==='object'&&!Array.isArray(e.value))return e.value[m[2]]!==undefined?e.value[m[2]]:null;return null;}
  if(/^[a-zA-Z_\u0600-\u06FF][a-zA-Z0-9_\u0600-\u06FF]*$/.test(expr)){const e=soil.get(expr);return e!==null?e.value:expr;}
  return evalCompound(expr,soil);
}

function evalCompound(expr,soil){
  // Pre-resolve chained obj:"k1":"k2":"k3" quoted MAP access in compound expressions
  expr=expr.replace(/(\w+)(?::"[^"]*")+/g,(full)=>{
    // parse out variable name and chain of keys
    const idM=full.match(/^(\w+)/);
    if(!idM)return full;
    const e=soil.get(idM[1]);
    if(!e)return full;
    let val=e.value;
    let rest=full.slice(idM[0].length);
    while(rest.startsWith(':"')){
      const km=rest.match(/^:"([^"]*)"/);
      if(!km)break;
      if(val&&typeof val==='object'&&!Array.isArray(val))val=val[km[1]];
      else{val=null;break;}
      rest=rest.slice(km[0].length);
    }
    if(val===undefined||val===null)return 'null';
    if(typeof val==='string')return JSON.stringify(val);
    if(typeof val==='object')return '[MAP]';
    return String(val);
  });
  // Pre-resolve FIRST/LAST/COUNT/SUM/MAX/MIN/REVERSE on variables so they work in concat
  expr=expr.replace(/\b(FIRST|LAST|COUNT|SUM|MAX|MIN|REVERSE)\s+(\w+)\b/gi,(full,fn,varName)=>{
    const e=soil.get(varName);
    if(!e||!Array.isArray(e.value))return full;
    const v=e.value;
    const fu=fn.toUpperCase();
    let res;
    if(fu==='FIRST')res=v[0];
    else if(fu==='LAST')res=v[v.length-1];
    else if(fu==='COUNT')res=v.length;
    else if(fu==='SUM')res=v.reduce((a,b)=>a+(+b||0),0);
    else if(fu==='MAX')res=Math.max(...v.map(Number));
    else if(fu==='MIN')res=Math.min(...v.map(Number));
    else if(fu==='REVERSE')res=[...v].reverse();
    else return full;
    if(res===undefined||res===null)return 'null';
    if(typeof res==='string')return JSON.stringify(res);
    if(Array.isArray(res))return JSON.stringify(res);
    return String(res);
  });
  let resolved=expr;
  resolved=resolved.replace(/\bSELF:(\w+)\b/g,(full,prop)=>{
    const self=soil.get('__self');
    if(self&&self.value&&self.value[prop]!==undefined){const v=self.value[prop];return typeof v==='string'?JSON.stringify(v):String(v);}
    return'null';
  });
  resolved=resolved.replace(/(\w+):(\w+)/g,(full,obj,prop)=>{
    const e=soil.get(obj);
    if(e&&e.value&&typeof e.value==='object'&&!Array.isArray(e.value)){const v=e.value[prop];if(v===undefined)return'null';return typeof v==='string'?JSON.stringify(v):String(v);}
    return full;
  });
  // Substitute bare identifiers with their variable values — but ONLY in the
  // segments of the expression that are OUTSIDE double-quoted string literals.
  // Without this split, a literal like "pi=" would have its "pi" substring
  // matched by the word-boundary regex below (word boundaries don't know
  // about quotes) and incorrectly replaced with the value of a variable
  // named `pi`, corrupting the literal text itself.
  resolved=resolved.replace(/("(?:[^"\\]|\\.)*")|\b([a-zA-Z_\u0600-\u06FF][a-zA-Z0-9_\u0600-\u06FF]*)\b/g,(full,stringLit,name)=>{
    if(stringLit!==undefined)return stringLit; // leave string literals untouched
    const SKIPS=['true','false','null','undefined','Infinity','NaN','Math','parseInt','parseFloat','String','Number','Array','Object','JSON','TRUE','FALSE','NULL','VOID'];
    if(SKIPS.includes(name))return name==='TRUE'?'true':name==='FALSE'?'false':name;
    const e=soil.get(name);
    if(e===null)return full;
    const v=e.value;
    if(typeof v==='string')return JSON.stringify(v);
    if(Array.isArray(v))return JSON.stringify(v);
    if(v&&typeof v==='object')return JSON.stringify(v);
    if(v===null)return'null';
    return String(v);
  });
  resolved=resolved.replace(/(\S+)\s*\/\/\s*(\S+)/g,'Math.floor($1/$2)');
  resolved=resolved.replace(/(\S+)\s*\*\*\s*(\S+)/g,'Math.pow($1,$2)');
  try{
    const result=Function('"use strict";return('+resolved+')')();
    if(!isFinite(result)&&typeof result==='number')storm('ZERO_STORM','division by zero');
    return result;
  }catch(e){if(e&&e.stormType)throw e;return coerce(expr);}
}

function evalCond(expr,soil){
  expr=String(expr).trim();
  let m;
  if(m=expr.match(/^(.+?)\s+NOR\s+(.+)$/i))return !evalCond(m[1],soil)&&!evalCond(m[2],soil);
  if((m=expr.match(/^(.+?)\s+OR\s+(.+)$/i))&&!/^(EQUAL|GREATER|LESS)\b/i.test(m[2]))return evalCond(m[1],soil)||evalCond(m[2],soil);
  if(m=expr.match(/^(.+?)\s+AND\s+(.+)$/i))return evalCond(m[1],soil)&&evalCond(m[2],soil);
  if(m=expr.match(/^NOT\s+(.+)$/i))return !evalCond(m[1],soil);
  if(m=expr.match(/^ANY\s+(\w+)\s+(GREATER THAN OR EQUAL|LESS THAN OR EQUAL|GREATER THAN|LESS THAN|IS NOT|IS|>=|<=|>|<)\s+(.+)$/i)){const e=soil.get(m[1]);if(!e||!Array.isArray(e.value))return false;const rv=evalExpr(m[3],soil);return e.value.some(el=>cmpOp(el,m[2].toUpperCase(),rv));}
  if(m=expr.match(/^ALL\s+(\w+)\s+(GREATER THAN OR EQUAL|LESS THAN OR EQUAL|GREATER THAN|LESS THAN|IS NOT|IS|>=|<=|>|<)\s+(.+)$/i)){const e=soil.get(m[1]);if(!e||!Array.isArray(e.value))return false;const rv=evalExpr(m[3],soil);return e.value.every(el=>cmpOp(el,m[2].toUpperCase(),rv));}
  if(m=expr.match(/^HAS\s+(\w+)\s+"([^"]+)"$/i)){const e=soil.get(m[1]);return e&&e.value&&typeof e.value==='object'&&m[2] in e.value;}
  if(m=expr.match(/^IS_A\s+(\w+)\s+"(\w+)"$/i)){const e=soil.get(m[1]);if(!e||!e.value||!e.value.__species)return false;return e.value.__species===m[2]||e.value.__parent===m[2];}
  if(m=expr.match(/^EMPTY\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e)return true;const v=e.value;return v===null||v===''||(Array.isArray(v)&&v.length===0);}
  if(m=expr.match(/^TEST\s+(\w+)$/i)){const e=soil.get(m[1]);if(!e)return false;return e.value!==null&&e.value!==''&&e.value!==false&&e.value!==0;}
  if(m=expr.match(/^(.+?)\s+BETWEEN\s+\(([^,]+),\s*([^)]+)\)$/i)){const v=+evalExpr(m[1],soil);return v>=+evalExpr(m[2],soil)&&v<=+evalExpr(m[3],soil);}
  if(m=expr.match(/^(.+?)\s+IS\s+BETWEEN\s+(\S+)\s+(\S+)$/i)){const v=+evalExpr(m[1],soil);return v>=+evalExpr(m[2],soil)&&v<=+evalExpr(m[3],soil);}
  if(m=expr.match(/^(.+?)\s+(GREATER THAN OR EQUAL|LESS THAN OR EQUAL|GREATER THAN|LESS THAN|IS NOT|IS|>=|<=|>|<|==|!=)\s+(.+)$/i))return cmpOp(evalExpr(m[1],soil),m[2].toUpperCase().trim(),evalExpr(m[3],soil));
  return !!evalExpr(expr,soil);
}

function cmpOp(lv,op,rv){
  switch(op){
    case'GREATER THAN OR EQUAL':case'>=':return lv>=rv;
    case'LESS THAN OR EQUAL':case'<=':return lv<=rv;
    case'GREATER THAN':case'>':return lv>rv;
    case'LESS THAN':case'<':return lv<rv;
    case'IS NOT':case'!=':return lv!=rv;
    case'IS':case'==':return lv==rv;
    default:return false;
  }
}
module.exports={evalExpr,evalCond};
