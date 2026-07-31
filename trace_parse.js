const path = require('path');
const storm = require('./core/storm');
const {Parser, parseFile} = require('./core/parser');

const origParseAction = Parser.prototype.parseActionDeclaration;
Parser.prototype.parseActionDeclaration = function(coords) {
  this.consume('KEYWORD', 'ACTION', '"ACTION"');
  
  const nameTok = this.current();
  if (nameTok.type !== 'IDENT') {
    storm.storm('SYNTAX_STORM', 'Expected action name after ACTION, found "' + nameTok.value + '"', nameTok.line, nameTok.column);
  }
  const name = this.advance().value;
  
  console.log('ACTION:', name, 'at line', coords.line);
  
  this.consume('PUNCT', '(', '"("');
  const params = this.parseParamList();
  this.consume('PUNCT', ')', '")"');
  
  const arrow = this.peek(0);
  const extKw = this.peek(1);
  if (arrow && arrow.type === 'PUNCT' && arrow.value === '->' &&
      extKw && extKw.type === 'KEYWORD' && extKw.value.toUpperCase() === 'EXTERNAL') {
    this.advance(); this.advance();
    if (this.match('PUNCT', '.')) this.advance();
    return;
  }
  
  this.consume('PUNCT', ',', '","');
  const headerDepth = coords.depth;
  
  const isActionCloser = (ft, st) =>
    ft && ft.type === 'PUNCT' && ft.value === '/' &&
    st && st.type === 'KEYWORD' && st.value === 'ACTION';
  
  try {
    const body = this.collectNestedBody(headerDepth, isActionCloser);
    
    if (this.match('DEPTH')) this.advance();
    this.consume('PUNCT', '/', '"/" in /ACTION.');
    this.consume('KEYWORD', 'ACTION', '"ACTION" in /ACTION.');
    if (this.match('PUNCT', '.')) this.advance();
    
    console.log('  END action', name);
  } catch(e) {
    console.log('  ERROR in', name, ':', e.message ? e.message.substring(0, 300) : e);
    throw e;
  }
};

try {
  parseFile('src/plantc/codegen_c.plant');
  console.log('SUCCESS');
} catch(e) {
  console.log('ERROR:', e.message ? e.message.substring(0, 300) : e);
}
