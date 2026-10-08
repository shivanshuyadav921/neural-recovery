// Use standard CommonJS require to hook Module._load in Node.js
const Module = require("module");
const mockVscode = require("./mockVscode").default;

const originalLoad = Module._load;
Module._load = function (request: string, parent: any, isMain: boolean) {
  if (request === "vscode") {
    return mockVscode;
  }
  return originalLoad.apply(this, arguments);
};

import { run } from "./suite/index";

async function main() {
  try {
    console.log("Running NeuroParse VS Code Extension Unit Tests...");
    await run();
    console.log("All Extension Unit Tests Passed!");
    process.exit(0);
  } catch (err) {
    console.error("Failed to run tests:", err);
    process.exit(1);
  }
}

main();
