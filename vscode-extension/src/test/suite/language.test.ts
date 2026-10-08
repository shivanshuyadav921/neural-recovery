import * as assert from "assert";
import * as fs from "fs";
import * as path from "path";

suite("Language & Syntax Configuration Tests", () => {
  test("Language configuration JSON is valid and structured", () => {
    const configPath = path.resolve(__dirname, "../../../language-configuration.json");
    assert.ok(fs.existsSync(configPath));
    const content = JSON.parse(fs.readFileSync(configPath, "utf8"));
    assert.strictEqual(content.comments.lineComment, "//");
    assert.deepStrictEqual(content.comments.blockComment, ["/*", "*/"]);
    assert.ok(content.brackets.length >= 3);
    assert.ok(content.autoClosingPairs.length >= 4);
  });

  test("TextMate grammar JSON contains all NeuroLang keywords & operators", () => {
    const grammarPath = path.resolve(__dirname, "../../../syntaxes/neurolang.tmLanguage.json");
    assert.ok(fs.existsSync(grammarPath));
    const grammar = JSON.parse(fs.readFileSync(grammarPath, "utf8"));
    assert.strictEqual(grammar.scopeName, "source.neurolang");
    assert.strictEqual(grammar.name, "NeuroLang");

    const repo = grammar.repository;
    assert.ok(repo.keywords !== undefined);
    assert.ok(repo.operators !== undefined);
    assert.ok(repo.strings !== undefined);
    assert.ok(repo.numbers !== undefined);
    assert.ok(repo.comments !== undefined);
    assert.ok(repo.delimiters !== undefined);
    assert.ok(repo.identifiers !== undefined);
  });
});
