const assert = require("node:assert/strict");
const { spawn } = require("node:child_process");
const { readFile } = require("node:fs/promises");
const http = require("node:http");
const path = require("node:path");
const test = require("node:test");

const htmlRoot = path.resolve(__dirname, "../../html");
const mimeTypes = {
  ".css": "text/css",
  ".gif": "image/gif",
  ".html": "text/html",
  ".ico": "image/x-icon",
  ".js": "text/javascript",
  ".json": "application/json",
  ".png": "image/png",
  ".webp": "image/webp",
  ".woff2": "font/woff2",
};

function listen(server) {
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => resolve(server.address().port));
  });
}

async function waitForDevTools(chrome) {
  return new Promise((resolve, reject) => {
    let stderr = "";
    const timeout = setTimeout(() => reject(new Error(`Chrome did not start:\n${stderr}`)), 10000);

    chrome.stderr.setEncoding("utf8");
    chrome.stderr.on("data", (chunk) => {
      stderr += chunk;
      const match = stderr.match(/DevTools listening on (ws:\/\/\S+)/);
      if (match) {
        clearTimeout(timeout);
        resolve(match[1]);
      }
    });
    chrome.once("exit", (code) => {
      clearTimeout(timeout);
      reject(new Error(`Chrome exited before DevTools was ready (${code}):\n${stderr}`));
    });
  });
}

async function waitForPageDevTools(browserUrl) {
  const { hostname, port } = new URL(browserUrl);
  for (let attempt = 0; attempt < 100; attempt += 1) {
    const targets = await fetch(`http://${hostname}:${port}/json/list`).then((response) => response.json());
    const page = targets.find((target) => target.type === "page" && target.url.includes("management.html"));
    if (page) return page.webSocketDebuggerUrl;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw new Error("Chrome did not expose the management page target");
}

function connectDevTools(url) {
  const socket = new WebSocket(url);
  const pending = new Map();
  let nextId = 0;

  const opened = new Promise((resolve, reject) => {
    socket.addEventListener("open", resolve, { once: true });
    socket.addEventListener("error", reject, { once: true });
  });
  socket.addEventListener("message", ({ data }) => {
    const message = JSON.parse(data);
    if (!message.id || !pending.has(message.id)) return;
    const { resolve, reject } = pending.get(message.id);
    pending.delete(message.id);
    if (message.error) reject(new Error(message.error.message));
    else resolve(message.result);
  });

  return {
    async send(method, params = {}) {
      await opened;
      const id = ++nextId;
      const response = new Promise((resolve, reject) => pending.set(id, { resolve, reject }));
      socket.send(JSON.stringify({ id, method, params }));
      return response;
    },
    close() {
      socket.close();
    },
  };
}

async function evaluate(client, expression) {
  const { result, exceptionDetails } = await client.send("Runtime.evaluate", {
    expression,
    awaitPromise: true,
    returnByValue: true,
  });
  if (exceptionDetails) throw new Error(exceptionDetails.text);
  return result.value;
}

test("settings validation reveals hidden tabs and ignores inactive sections", async (t) => {
  const server = http.createServer(async (request, response) => {
    try {
      const pathname = new URL(request.url, "http://localhost").pathname;
      const relativePath = pathname === "/" ? "management.html" : pathname.slice(1);
      const filePath = path.resolve(htmlRoot, relativePath);
      if (!filePath.startsWith(`${htmlRoot}${path.sep}`)) throw new Error("Invalid path");
      const content = await readFile(filePath);
      response.writeHead(200, { "Content-Type": mimeTypes[path.extname(filePath)] || "application/octet-stream" });
      response.end(content);
    } catch {
      response.writeHead(404);
      response.end();
    }
  });
  const port = await listen(server);
  t.after(() => server.close());

  const chrome = spawn("google-chrome", [
    "--headless=new",
    "--no-sandbox",
    "--disable-gpu",
    "--remote-debugging-port=0",
    `http://127.0.0.1:${port}/management.html`,
  ], { stdio: ["ignore", "ignore", "pipe"] });
  t.after(() => chrome.kill("SIGKILL"));

  const client = connectDevTools(await waitForPageDevTools(await waitForDevTools(chrome)));
  t.after(() => client.close());
  await client.send("Runtime.enable");

  for (let attempt = 0; attempt < 100; attempt += 1) {
    if (await evaluate(client, "typeof validateSettingsForm === 'function'")) break;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }

  const result = await evaluate(client, `(async () => {
    const form = document.querySelector('#generalConfig form');
    Array.from(form.elements).forEach((control) => {
      if (control.willValidate) control.disabled = true;
    });

    const testControl = document.createElement('input');
    testControl.id = 'validationTestControl';
    testControl.name = 'validationTestControl';
    testControl.required = true;
    document.getElementById('general-power').append(testControl);
    const fieldset = document.createElement('fieldset');
    testControl.replaceWith(fieldset);
    fieldset.append(testControl);
    const fieldsetMatchedInvalid = fieldset.matches(':invalid');
    const accepted = validateSettingsForm(form);
    await new Promise((resolve) => setTimeout(resolve, 350));

    const tabResult = {
      accepted,
      fieldsetMatchedInvalid,
      focusedId: document.activeElement.id,
      powerTabActive: document.getElementById('general-power').classList.contains('active'),
      mainTabActive: document.getElementById('nav-general').classList.contains('active'),
    };

    const battery = document.getElementById('batteryConfig');
    const criticalSection = document.getElementById('criticalVoltageSection');
    const criticalVoltage = document.getElementById('criticalVoltage');
    const initiallyExcluded = !criticalVoltage.willValidate;
    setSettingsSectionActive(battery, true);
    const excludedWhileFeatureOff = !criticalVoltage.willValidate;
    setSettingsSectionActive(criticalSection, true);
    const includedWhenFeatureOn = criticalVoltage.willValidate;

    return { tabResult, initiallyExcluded, excludedWhileFeatureOff, includedWhenFeatureOn };
  })()`);

  assert.deepEqual(result.tabResult, {
    accepted: false,
    fieldsetMatchedInvalid: true,
    focusedId: "validationTestControl",
    powerTabActive: true,
    mainTabActive: true,
  });
  assert.equal(result.initiallyExcluded, true);
  assert.equal(result.excludedWhileFeatureOff, true);
  assert.equal(result.includedWhenFeatureOn, true);
});
