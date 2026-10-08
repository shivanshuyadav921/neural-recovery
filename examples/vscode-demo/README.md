# NeuroParse VS Code Demonstration Workspace

This directory contains demonstration files for testing and verifying the NeuroParse VS Code extension.

## Files

1. `valid_sample.nl` — Clean NeuroLang file with valid syntax. Should show 0 diagnostics and clean status.
2. `missing_semicolon.nl` — Missing statement-terminating semicolon on line 2. Demonstrates:
   - Diagnostic error squiggly at line 2.
   - NeuroParse neural proposal attached to diagnostic.
   - Quick Fix action (`Ctrl+.` / `Cmd+.`): `NeuroParse (Neural): Insert ';'`.
3. `missing_paren.nl` — Missing closing parenthesis on function declaration. Demonstrates:
   - Neural proposal to insert `)`.
   - Quick Fix insertion.
   - Explanation command (`NeuroParse: Explain Syntax Error Under Cursor`).
