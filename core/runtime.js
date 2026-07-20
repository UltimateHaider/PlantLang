'use strict';
const STORMS=['ZERO_STORM','TYPE_STORM','MISSING_STORM','SEED_STORM','LOST_STORM','LOCK_STORM','BOUND_STORM','STOP_STORM','PERM_STORM','NETWORK_STORM','SYNTAX_STORM','ANY_STORM'];
class PlantStorm extends Error {
  constructor(type,message,line,column){super(message);this.name=type||'ANY_STORM';this.stormType=type||'ANY_STORM';this.line=line;this.column=column;}
}
function storm(type,msg,line,column){if(!STORMS.includes(type))type='ANY_STORM';throw new PlantStorm(type,msg,line,column);}
function inferType(v){
  if(v===null||v===undefined)return'VOID';
  if(typeof v==='boolean')return'FACT';
  if(Array.isArray(v))return'LIST';
  if(v&&typeof v==='object')return'MAP';
  if(typeof v==='number')return Number.isInteger(v)?'NUM':'SCL';
  return'TX';
}
function coerce(raw){
  if(raw===null||raw===undefined)return null;
  const s=String(raw).trim();
  if(s==='TRUE'||s==='true')return true;
  if(s==='FALSE'||s==='false')return false;
  if((s.startsWith('"')&&s.endsWith('"'))||(s.startsWith("'")&&s.endsWith("'")))return s.slice(1,-1);
  const n=Number(s);return isNaN(n)?s:n;
}
class Soil {
  constructor(parent=null){this._vars=new Map();this._parent=parent;}
  set(name,value,type,opts={}){
    const entry={value,type:type||inferType(value),locked:opts.locked||false,pulse:opts.pulse||false};
    this._vars.set(name,entry);return entry;
  }
  get(name){
    if(this._vars.has(name))return this._vars.get(name);
    if(this._parent)return this._parent.get(name);
    return null;
  }
  has(name){return this._vars.has(name)||(this._parent?this._parent.has(name):false);}
  update(name,value){
    if(this._vars.has(name)){
      const e=this._vars.get(name);
      if(e.locked)storm('LOCK_STORM',`"${name}" is protected — cannot be modified`);
      e.value=value;e.type=inferType(value);return e;
    }
    if(this._parent)return this._parent.update(name,value);
    storm('MISSING_STORM',`"${name}" not found`);
  }
  delete(name){
    if(this._vars.has(name)){this._vars.delete(name);return true;}
    if(this._parent)return this._parent.delete(name);
    return false;
  }
  snapshot(){
    const out={};
    if(this._parent)Object.assign(out,this._parent.snapshot());
    for(const[k,v]of this._vars)out[k]=v;
    return out;
  }
  child(){return new Soil(this);}
}
class VeinFS {
  constructor(){this._files=new Map();}
  write(path,content){this._files.set(path,String(content));}
  read(path){return this._files.get(path)||'';}
  exists(path){return this._files.has(path);}
  append(path,line){const cur=this.read(path);this._files.set(path,cur?cur+'\n'+line:line);}
}
module.exports={PlantStorm,storm,STORMS,inferType,coerce,Soil,VeinFS};
