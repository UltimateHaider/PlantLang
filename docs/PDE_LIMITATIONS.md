# PDE Solver Limitations

## MATH_SOLVE_PDE — Supported Forms

The PDE classifier recognizes three standard equation forms:

| PDE Type | Form | Variables |
|----------|------|-----------|
| Wave | d²u/dt² = c² · d²u/dx² | time t, space x |
| Heat | du/dt = α · d²u/dx² | time t, space x |
| Laplace | d²u/dx² + d²u/dy² = 0 | spatial x, y |

**Note**: `MATH_SOLVE_PDE` returns general solution structures, not specific solutions. Boundary and initial conditions are not processed.

## MATH_VERIFY_PDE — Supported Functions

Verification uses numerical evaluation at test points. These functions are supported:

| Function | Example |
|----------|---------|
| Polynomials | `x^2 + 3*x + 1` |
| Trigonometric | `sin(x)`, `cos(t)`, `tan(x)` |
| Exponential | `exp(x)` |
| Logarithmic | `log(x)` |
| Square root | `sqrt(x)` |
| Absolute value | `abs(x)` |
| Constants | `PI`, `E` |

## Unsupported Functions

The following functions are **not supported** by PDE verification:

- Arbitrary functions: `f(x)`, `g(t)`, `h(x)`
- Bessel functions: `J_0(x)`, `Y_n(x)`
- Error function: `erf(x)`, `erfc(x)`
- Gamma function: `gamma(x)`, `lgamma(x)`
- Special functions: `Ai(x)`, `Gi(x)`

### Behavior

**MATH_VERIFY_PDE** (standard mode):
- Detects unsupported functions
- Prints a warning to stderr
- Returns `0` (FALSE) — the result may be incorrect

**MATH_VERIFY_PDE_STRICT** (strict mode):
- Detects unsupported functions
- Returns `ERROR` with a descriptive message
- Never returns potentially incorrect results

### Example

```plantlang
# Standard mode — warns and returns 0
SHOW MATH_VERIFY_PDE("d2u/dt2 = d2u/dx2", "f(x-ct)", "u", "t", "x").  # 0

# Strict mode — returns error
SHOW MATH_VERIFY_PDE_STRICT("d2u/dt2 = d2u/dx2", "f(x-ct)", "u", "t", "x").
# ERROR: Unsupported function 'f' in MATH_VERIFY_PDE_STRICT mode.
```

## Accuracy

Verification evaluates the PDE residual at 8 test points with values in the range [-1.0, 3.0]. The residual must be < 1e-4 at all test points to pass.

**False negatives** are possible: a valid solution may fail if it has special structure at the test points (e.g., a polynomial that happens to evaluate to 0 at test points but doesn't satisfy the PDE identically).

**False positives** are theoretically impossible for standard functions (the numerical check is exact for polynomials and analytic for transcendental functions in the supported domain).

## Future Plans

- Support for Bessel functions via runtime Bessel library
- Support for arbitrary user-defined functions via symbolic substitution
- Adaptive test point selection
- Symbolic verification option (requires full CAS simplification)
