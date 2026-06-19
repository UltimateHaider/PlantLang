#!/usr/bin/env node
'use strict';
const fs=require('fs');
const path=require('path');
const readline=require('readline');
const{Interpreter}=require('./core/interpreter');
const{lex}=require('./core/lexer');

const C={reset:'\x1b[0m',green:'\x1b[32m',red:'\x1b[31m',yellow:'\x1b[33m',cyan:'\x1b[36m',gray:'\x1b[90m',bold:'\x1b[1m'};

function icon(type){return type==='ok'?'✓':type==='error'?'✕':type==='warn'?'⚠':type==='verify_pass'?'  ✓':type==='verify_fail'?'  ✗':type==='verify_hr'?'':type==='verify_win'?'':'›';}
function col(type){return type==='ok'?C.green:type==='error'?C.red:type==='warn'?C.yellow:type==='muted'?C.gray:type==='verify_pass'?C.green:type==='verify_fail'?C.red:type==='verify_detail'?C.yellow:type==='verify_hr'?C.gray:type==='verify_win'?C.green:type==='suite'?'\x1b[1m'+C.cyan:C.cyan;}
function printLine(text,type){
  if(type==='verify_pass'||type==='verify_fail'||type==='verify_detail'||type==='verify_hr'||type==='verify_win'||type==='suite'){
    console.log(`${col(type)}${text}${C.reset}`);
  }else{
    console.log(`${col(type)}${icon(type)}${C.reset} ${text}`);
  }
}

function banner(){
  console.log(`\n${C.green}${C.bold}🌿 PlantLang — Chloroplast v0.6${C.reset}`);
  console.log(`${C.gray}   المفسر الحقيقي لملفات .plnt${C.reset}\n`);
}

function runFile(filePath,opts={}){
  if(!fs.existsSync(filePath)){console.error(`${C.red}✕ الملف غير موجود: ${filePath}${C.reset}`);process.exit(1);}
  const source=fs.readFileSync(filePath,'utf8');
  const mission=opts.mission||'SAFE';
  if(opts.verbose)console.log(`${C.gray}  mission: ${mission}  |  ${filePath}${C.reset}\n`);
  const interp=new Interpreter({mission,rootDir:path.dirname(path.resolve(filePath)),emit:(text,type)=>printLine(text,type)});
  const t0=Date.now();
  try{
    interp.run(source);
    if(opts.verbose)console.log(`\n${C.gray}  اكتمل في ${Date.now()-t0}ms — MISSION: ${mission}${C.reset}`);
  }catch(e){
    console.error(`\n${C.red}✕ ${e.name||'خطأ'}: ${e.message}${C.reset}`);
    if(e.line)console.error(`${C.gray}  السطر: ${e.line}${C.reset}`);
    process.exit(1);
  }
}

function verifyFile(filePath,opts={}){
  if(!fs.existsSync(filePath)){console.error(`${C.red}✕ File not found: ${filePath}${C.reset}`);process.exit(1);}
  // Append SHOW_VERIFY_SUMMARY to source
  const source=fs.readFileSync(filePath,'utf8')+'\nSHOW_VERIFY_SUMMARY.';
  const mission=opts.mission||'SAFE';
  console.log(`${C.cyan}Testing: ${filePath}${C.reset}\n`);
  const interp=new Interpreter({mission,rootDir:path.dirname(path.resolve(filePath)),emit:(text,type)=>printLine(text,type)});
  try{
    interp.run(source);
    process.exit(interp.verifyStats.failed>0?1:0);
  }catch(e){
    console.error(`\n${C.red}✕ ${e.name||'Error'}: ${e.message}${C.reset}`);
    if(e.line)console.error(`${C.gray}  line: ${e.line}${C.reset}`);
    process.exit(1);
  }
}

function checkFile(filePath){
  if(!fs.existsSync(filePath)){console.error(`${C.red}✕ غير موجود: ${filePath}${C.reset}`);process.exit(1);}
  try{
    const stmts=lex(fs.readFileSync(filePath,'utf8'));
    console.log(`${C.green}✓ ${filePath} — ${stmts.length} جملة — لا أخطاء${C.reset}`);
  }catch(e){
    console.error(`${C.red}✕ خطأ: ${e.message}${C.reset}`);process.exit(1);
  }
}

function startREPL(opts={}){
  banner();
  console.log(`${C.gray}اكتب كود PlantLang مباشرة. .help للمساعدة. .exit للخروج.${C.reset}\n`);
  const mission=opts.mission||'SAFE';
  const interp=new Interpreter({mission,emit:(text,type)=>printLine(text,type)});
  const rl=readline.createInterface({input:process.stdin,output:process.stdout,prompt:`${C.green}🌿${C.reset} `});
  rl.prompt();
  let buffer='';
  rl.on('line',(line)=>{
    const t=line.trim();
    if(t==='.exit'||t==='exit'){console.log(`${C.green}إلى اللقاء 🌿${C.reset}`);process.exit(0);}
    if(t==='.help'){
      console.log(`${C.gray}  .soil    — عرض الذاكرة\n  .funcs   — الأفعال المعرّفة\n  .species — الكائنات\n  .clear   — مسح الذاكرة\n  .exit    — خروج${C.reset}`);
      rl.prompt();return;
    }
    if(t==='.soil'||t==='.memory'){
      const snap=interp.soil.snapshot();
      const keys=Object.keys(snap);
      if(!keys.length){console.log(`${C.gray}  التربة فارغة${C.reset}`);}
      else keys.forEach(k=>{const e=snap[k];const v=Array.isArray(e.value)?`[${e.value.join(', ')}]`:String(e.value);console.log(`  ${C.cyan}${k}${C.reset} ${C.gray}(${e.type})${C.reset} = ${v}${e.locked?' 🔒':''}${e.pulse?' ⚡':''}`);});
      rl.prompt();return;
    }
    if(t==='.funcs'){interp.funcs.forEach((fn,name)=>console.log(`  ${C.cyan}ACTION ${name}${C.reset}(${fn.params.map(p=>p.name).join(', ')})`));rl.prompt();return;}
    if(t==='.species'){interp.species.forEach((sp,name)=>console.log(`  ${C.cyan}SPECIES ${name}${C.reset}${sp.parent?' PARENT '+sp.parent:''}`));rl.prompt();return;}
    if(t==='.clear'){interp.soil=new(require('./core/runtime').Soil)();interp.funcs.clear();interp.species.clear();console.log(`${C.gray}  تمت إعادة التهيئة${C.reset}`);rl.prompt();return;}
    buffer+=(buffer?'\n':'')+t;
    if(buffer.endsWith(',')){process.stdout.write(`${C.gray}...${C.reset} `);return;}
    const toRun=buffer.endsWith('.')?buffer:buffer+'.';
    buffer='';
    try{
      const stmts=lex(toRun);
      interp._firstPass(stmts);
      interp._execBlock(stmts,0,stmts.length,interp.soil);
    }catch(e){
      if(e.stormType)console.error(`${C.red}  ⚡ ${e.stormType}: ${e.message}${C.reset}`);
      else console.error(`${C.red}  ✕ ${e.message}${C.reset}`);
    }
    rl.prompt();
  });
  rl.on('close',()=>{console.log(`\n${C.green}إلى اللقاء 🌿${C.reset}`);});
}

function main(){
  const args=process.argv.slice(2);
  if(!args.length||args[0]==='--help'||args[0]==='-h'){
    banner();
    console.log(`Usage:`);
    console.log(`  ${C.cyan}chloroplast run${C.reset}     <file.plnt> [--mission FAST|SAFE|SMART] [--verbose]`);
    console.log(`  ${C.cyan}chloroplast verify${C.reset}  <file.plnt> [--mission FAST|SAFE|SMART]`);
    console.log(`  ${C.cyan}chloroplast repl${C.reset}    [--mission FAST|SAFE|SMART]`);
    console.log(`  ${C.cyan}chloroplast check${C.reset}   <file.plnt>`);
    return;
  }
  const cmd=args[0];
  const mi=args.indexOf('--mission');
  const mission=mi!==-1?args[mi+1].toUpperCase():'SAFE';
  const verbose=args.includes('--verbose')||args.includes('-v');
  switch(cmd){
    case'run':{const file=args[1];if(!file){console.error(`${C.red}✕ specify a .plnt file${C.reset}`);process.exit(1);}runFile(file,{mission,verbose});break;}
    case'verify':{const file=args[1];if(!file){console.error(`${C.red}✕ specify a .plnt file${C.reset}`);process.exit(1);}verifyFile(file,{mission});break;}
    case'repl':startREPL({mission});break;
    case'check':{const file=args[1];if(!file){console.error(`${C.red}✕ specify a .plnt file${C.reset}`);process.exit(1);}checkFile(file);break;}
    default:
      if(cmd.endsWith('.plnt')||fs.existsSync(cmd))runFile(cmd,{mission,verbose});
      else{console.error(`${C.red}✕ unknown command: ${cmd}${C.reset}`);process.exit(1);}
  }
}
main();
