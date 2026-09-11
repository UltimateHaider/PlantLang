'use strict';

// ═══════════════════════════════════════════════════════════════
//  src/interpreter/bloom_evaluator.js
//  v0.38.0 — BLOOM AS Visual Governance & Security Guard
//  Supports GRAPH, TABLE, CHART output with restricted env detection.
// ═══════════════════════════════════════════════════════════════

/**
 * Check if the current environment is restricted (e.g. CodeWords Service).
 * Restricted environments cannot render visual payloads.
 */
function isRestrictedEnvironment() {
  // Detect restricted environments:
  // 1. CODEPLANT_RESTRICTED env var explicitly set
  // 2. Running inside the CodeWords service (no window/document, specific env)
  // 3. No stdout is a TTY
  if (typeof process !== 'undefined') {
    if (process.env.CODEPLANT_RESTRICTED === '1' || process.env.CODEPLANT_RESTRICTED === 'true') {
      return true;
    }
    // Check if we're in a service-like environment (no TTY)
    try {
      if (!process.stdout.isTTY) return true;
    } catch (_) { return true; }
  }
  return false;
}

/**
 * Evaluate a BloomAsStatementNode.
 *
 * Grammar: BLOOM data_expr AS [GRAPH|TABLE|CHART] { config_map }
 *
 * @param {object} node        - BloomAsStatementNode
 * @param {object} interpreter - Interpreter instance
 * @param {object} soil        - current Soil scope
 * @returns {null}
 */
function evaluateBloomAsStatement(node, interpreter, soil) {
  const data = interpreter.evaluateExpressionNode(node.dataExpr, soil);
  const targetType = (node.targetType || 'TABLE').toUpperCase();

  if (isRestrictedEnvironment()) {
    // Restricted: emit safe text dump instead of visual payload
    const dump = JSON.stringify(
      { _dryRun: true, type: targetType, data, config: node.configMap },
      null, 2
    );
    const lines = dump.split('\n');
    for (const line of lines) {
      interpreter._emit('[Dry-Run Dump] ' + line);
    }
    return null;
  }

  // Unrestricted environment — produce structured visual output
  switch (targetType) {
    case 'GRAPH':
      return _renderGraph(data, node.configMap, interpreter);
    case 'CHART':
      return _renderChart(data, node.configMap, interpreter);
    case 'TABLE':
    default:
      return _renderTable(data, node.configMap, interpreter);
  }
}

function _renderTable(data, config, interpreter) {
  const rows = Array.isArray(data) ? data : [data];
  if (rows.length === 0) {
    interpreter._emit('[TABLE] (empty)');
    return null;
  }

  // Extract headers from first row keys
  const first = rows[0];
  const headers = typeof first === 'object' && first !== null ? Object.keys(first) : ['value'];

  // Calculate column widths
  const widths = headers.map(h => {
    let maxW = h.length;
    for (const row of rows) {
      const val = (typeof row === 'object' && row !== null) ? row[h] : row;
      maxW = Math.max(maxW, String(val ?? '').length);
    }
    return maxW;
  });

  // Build separator
  const sep = '+-' + widths.map(w => '-'.repeat(w)).join('-+-') + '-+';

  // Render header
  interpreter._emit('[TABLE]');
  interpreter._emit(sep);
  interpreter._emit('| ' + headers.map((h, i) => h.padEnd(widths[i])).join(' | ') + ' |');
  interpreter._emit(sep);

  // Render rows
  for (const row of rows) {
    const vals = headers.map((h, i) => {
      const v = (typeof row === 'object' && row !== null) ? row[h] : row;
      return String(v ?? '').padEnd(widths[i]);
    });
    interpreter._emit('| ' + vals.join(' | ') + ' |');
  }
  interpreter._emit(sep);
  return null;
}

function _renderGraph(data, config, interpreter) {
  const values = Array.isArray(data) ? data : [data];
  const labelKey = config.labelKey || 'label';
  const valueKey = config.valueKey || 'value';
  const maxWidth = config.maxWidth || 40;

  interpreter._emit('[GRAPH]');

  // Find max value for scaling
  let maxVal = 1;
  const parsed = values.map(v => {
    const label = (typeof v === 'object' ? v[labelKey] : v) ?? '';
    const val = parseFloat(typeof v === 'object' ? v[valueKey] : v) || 0;
    if (val > maxVal) maxVal = val;
    return { label: String(label), val };
  });

  for (const item of parsed) {
    const barLen = Math.max(1, Math.round((item.val / maxVal) * maxWidth));
    const bar = '█'.repeat(barLen);
    interpreter._emit(`  ${item.label.padEnd(12)} ${bar} ${item.val}`);
  }
  return null;
}

function _renderChart(data, config, interpreter) {
  const values = Array.isArray(data) ? data : [data];
  const labelKey = config.labelKey || 'label';
  const valueKey = config.valueKey || 'value';
  const chartHeight = config.chartHeight || 8;

  interpreter._emit('[CHART]');

  const parsed = values.map(v => ({
    label: String(typeof v === 'object' ? v[labelKey] : v ?? ''),
    val: parseFloat(typeof v === 'object' ? v[valueKey] : v) || 0
  }));

  const maxVal = Math.max(...parsed.map(p => p.val), 1);

  // Print bars top-down
  for (let row = chartHeight; row >= 0; row--) {
    const threshold = (row / chartHeight) * maxVal;
    const line = parsed.map(p => (p.val >= threshold ? ' ███ ' : '     ')).join('');
    const label = row === 0 ? ' 0' : '';
    if (line.trim()) interpreter._emit(`  ${line}`);
  }

  // Labels
  const labelLine = parsed.map(p => ' ' + p.label.padEnd(3) + ' ').join('');
  interpreter._emit(`  ${labelLine}`);
  return null;
}

module.exports = { evaluateBloomAsStatement, isRestrictedEnvironment };
