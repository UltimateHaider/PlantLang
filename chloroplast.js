#!/usr/bin/env node
'use strict';
const fs=require('fs');
const path=require('path');
const readline=require('readline');
const{Interpreter}=require('./core/interpreter');
const{lex}=require('./core/lexer');
const{formatStormDiagnostic}=require('./core/diagnostics');

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
  console.log(`\n${C.green}${C.bold}🌿 PlantLang — Chloroplast v0.22${C.reset}`);
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
    interp.runSource(source);
    if(opts.verbose)console.log(`\n${C.gray}  اكتمل في ${Date.now()-t0}ms — MISSION: ${mission}${C.reset}`);
  }catch(e){
    console.error('\n'+formatStormDiagnostic(e,filePath,source));
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
    interp.runSource(source);
    process.exit(interp.verifyStats.failed>0?1:0);
  }catch(e){
    console.error('\n'+formatStormDiagnostic(e,filePath,source));
    process.exit(1);
  }
}

function checkFile(filePath){
  if(!fs.existsSync(filePath)){console.error(`${C.red}✕ File not found: ${filePath}${C.reset}`);process.exit(1);}
  const source=fs.readFileSync(filePath,'utf8');
  const {parse}=require('./core/parser');
  const {typecheck}=require('./core/typechecker');

  let prog;
  try{
    prog=parse(source);
  }catch(e){
    console.error(`${C.red}✕ Parse error: ${e.message}${C.reset}`);
    process.exit(1);
  }

  const diags=typecheck(prog,source);
  const errors=diags.filter(d=>d.severity==='error');
  const warns =diags.filter(d=>d.severity==='warning');

  if(diags.length===0){
    console.log(`${C.green}✓ ${filePath} — no issues found${C.reset}`);
    return;
  }

  const lines=source.split('\n');
  for(const d of diags){
    const icon=d.severity==='error'?`${C.red}✕`:d.severity==='warning'?`${C.yellow}⚠`:`${C.cyan}ℹ`;
    console.log(`\n${icon} [${d.code}] ${d.message}${C.reset}`);
    console.log(`${C.gray}  --> ${filePath}:${d.line}:${d.column}${C.reset}`);
    if(d.line>0&&lines[d.line-1]!==undefined){
      console.log(`${C.gray}  ${d.line} \\${C.reset} ${lines[d.line-1]}`);
      const arrow=' '.repeat(Math.max(0,d.column))+'↑';
      console.log(`${C.gray}       ${d.severity==='error'?C.red:C.yellow}${arrow}${C.reset}`);
    }
  }

  console.log('');
  if(errors.length>0){
    console.log(`${C.red}✕ ${errors.length} error(s), ${warns.length} warning(s) — ${filePath}${C.reset}`);
    process.exit(1);
  }else{
    console.log(`${C.yellow}⚠ 0 errors, ${warns.length} warning(s) — ${filePath}${C.reset}`);
  }
}

function printCodegenErrors(errors,source,filePath,backendLabel){
  console.error(`\n${C.red}✕ Cannot compile with ${backendLabel} backend — ${errors.length} unsupported construct(s):${C.reset}\n`);
  const lines=source.split('\n');
  for(const e of errors){
    console.error(`${C.red}  ✕ ${e.message}${C.reset}`);
    if(e.line>0&&lines[e.line-1]!==undefined){
      console.error(`${C.gray}    --> ${filePath}:${e.line}:${e.column}${C.reset}`);
      console.error(`${C.gray}    ${e.line} \\${C.reset} ${lines[e.line-1].trim()}`);
    }
  }
  console.error(`\n${C.yellow}ℹ These constructs work in "chloroplast run" (interpreter) but can't yet be compiled to native code.${C.reset}`);
  console.error(`${C.gray}  Supported for compile: CREATE/SET/INCREASE/DECREASE (NUM/SCL/TX/FACT), SHOW, IF/ORIF/ELSE, CYCLE (numeric), SEASON.${C.reset}`);
}

function findLLC(){
  const {execFileSync}=require('child_process');
  const candidates=['llc','llc-18','llc-17','llc-16','llc-15','llc-14'];
  for(const bin of candidates){
    try{
      execFileSync(bin,['--version'],{stdio:'pipe'});
      return bin;
    }catch(_){ /* try next */ }
  }
  return null;
}

function findOpt(llcBin){
  // Prefer the same version suffix as the detected llc (e.g. llc-18 → opt-18)
  // so the two tools are guaranteed to speak the same IR/bitcode version.
  const {execFileSync}=require('child_process');
  const suffix=llcBin&&llcBin.includes('-')?llcBin.slice(llcBin.indexOf('-')):'';
  const candidates=suffix?[`opt${suffix}`,'opt']:['opt','opt-18','opt-17','opt-16'];
  for(const bin of candidates){
    try{
      execFileSync(bin,['--version'],{stdio:'pipe'});
      return bin;
    }catch(_){ /* try next */ }
  }
  return null;
}

function compileFile(filePath,opts={}){
  if(!fs.existsSync(filePath)){console.error(`${C.red}✕ File not found: ${filePath}${C.reset}`);process.exit(1);}
  const {execFileSync}=require('child_process');
  const source=fs.readFileSync(filePath,'utf8');
  const {parse}=require('./core/parser');

  const llcBin=opts.backend==='c'?null:findLLC();
  const useLLVM=!!llcBin&&opts.backend!=='c';

  console.log(`${C.cyan}Compiling: ${filePath}${C.reset} ${C.gray}(backend: ${useLLVM?'LLVM via '+llcBin:'C via gcc'})${C.reset}`);

  let prog;
  try{
    prog=parse(source);
  }catch(e){
    console.error(`${C.red}✕ Parse error: ${e.message}${C.reset}`);
    process.exit(1);
  }

  const base=path.basename(filePath,'.plnt');
  const dir=path.dirname(path.resolve(filePath));
  const binPath=opts.outPath?path.resolve(opts.outPath):path.join(dir,base);

  if(useLLVM){
    const {generate}=require('./core/llvm_codegen');
    const {ir,errors}=generate(prog);

    if(errors.length>0){
      printCodegenErrors(errors,source,filePath,'LLVM');
      process.exit(1);
    }

    const llPath=path.join(dir,`${base}.ll`);
    const optPath=path.join(dir,`${base}.opt.ll`);
    const sPath=path.join(dir,`${base}.s`);
    fs.writeFileSync(llPath,ir,'utf8');
    console.log(`${C.gray}  → generated ${llPath}${C.reset}`);

    const optBin=findOpt(llcBin);
    let lowerInput=llPath;
    if(optBin){
      try{
        // -O2 here runs LLVM's real optimization pipeline (mem2reg, GVN,
        // inlining, loop optimizations, ...) — without this, our alloca/
        // load/store-heavy hand-emitted IR only gets llc's backend-level
        // optimizations, missing most of what makes LLVM fast.
        execFileSync(optBin,[llPath,'-O2','-S','-o',optPath],{stdio:'pipe'});
        lowerInput=optPath;
      }catch(e){
        console.log(`${C.yellow}⚠ opt pass failed, continuing with unoptimized IR${C.reset}`);
      }
    }

    try{
      execFileSync(llcBin,[lowerInput,'-O2','-o',sPath],{stdio:'inherit'});
    }catch(e){
      console.error(`${C.red}✕ llc failed to lower the IR${C.reset}`);
      if(!opts.keepC){try{fs.unlinkSync(llPath);}catch(_){}try{fs.unlinkSync(optPath);}catch(_){}}
      process.exit(1);
    }

    try{
      execFileSync('gcc',[sPath,'-no-pie','-lm','-o',binPath],{stdio:'inherit'});
    }catch(e){
      console.error(`${C.red}✕ gcc linking failed${C.reset}`);
      if(!opts.keepC){try{fs.unlinkSync(llPath);}catch(_){}try{fs.unlinkSync(optPath);}catch(_){}try{fs.unlinkSync(sPath);}catch(_){}}
      process.exit(1);
    }

    if(!opts.keepC){
      try{fs.unlinkSync(llPath);}catch(_){}
      try{fs.unlinkSync(optPath);}catch(_){}
      try{fs.unlinkSync(sPath);}catch(_){}
    }else{
      console.log(`${C.gray}  → kept ${llPath}${optBin?', '+optPath:''}, and ${sPath}${C.reset}`);
    }

  }else{
    const {generate}=require('./core/codegen');
    const {code,errors}=generate(prog);

    if(errors.length>0){
      printCodegenErrors(errors,source,filePath,'C');
      process.exit(1);
    }

    const cPath=path.join(dir,`${base}.c`);
    fs.writeFileSync(cPath,code,'utf8');
    console.log(`${C.gray}  → generated ${cPath}${C.reset}`);

    try{
      execFileSync('gcc',[cPath,'-O2','-lm','-o',binPath],{stdio:'inherit'});
    }catch(e){
      console.error(`${C.red}✕ gcc compilation failed${C.reset}`);
      if(!opts.keepC)try{fs.unlinkSync(cPath);}catch(_){}
      process.exit(1);
    }

    if(!opts.keepC){
      try{fs.unlinkSync(cPath);}catch(_){}
    }
  }

  console.log(`${C.green}✓ Compiled → ${binPath}${C.reset}`);

  if(opts.runAfter){
    console.log(`${C.cyan}─── running ───${C.reset}`);
    try{
      execFileSync(binPath,[],{stdio:'inherit'});
    }catch(e){
      process.exit(e.status||1);
    }
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
      if(e.stormType)console.error('\n'+formatStormDiagnostic(e,null,toRun));
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
    console.log(`  ${C.cyan}chloroplast compile${C.reset} <file.plnt> [--output <path>] [--run] [--keep-c] [--backend llvm|c]`);
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
    case'compile':{
      const file=args[1];
      if(!file){console.error(`${C.red}✕ specify a .plnt file${C.reset}`);process.exit(1);}
      const oi=args.indexOf('--output');
      const outPath=oi!==-1?args[oi+1]:null;
      const keepC=args.includes('--keep-c');
      const runAfter=args.includes('--run');
      const bi=args.indexOf('--backend');
      const backend=bi!==-1?args[bi+1]:null; // 'llvm' (default if available) or 'c'
      compileFile(file,{outPath,keepC,runAfter,backend});
      break;
    }
    default:
      if(cmd.endsWith('.plnt')||fs.existsSync(cmd))runFile(cmd,{mission,verbose});
      else{console.error(`${C.red}✕ unknown command: ${cmd}${C.reset}`);process.exit(1);}
  }
}
main();
