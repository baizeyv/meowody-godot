import { promises as fs } from "node:fs";
import WebSocket from "ws";
import { normalizedPath, projectRoot, resolveInProject } from "./project.js";

export interface BridgeRequest {
  command: string;
  args?: Record<string, unknown>;
}

export interface BridgeResponse {
  id: number;
  ok: boolean;
  result?: unknown;
  error?: string;
}

let nextId = 0;

export async function callBridge(req: BridgeRequest, timeoutMs = 10000): Promise<BridgeResponse> {
  let endpoint: { protocol: number; project_path: string; url: string; token: string };
  try {
    const file = await resolveInProject(projectRoot, ".godot/meowody_mcp/bridge.json");
    endpoint = JSON.parse(await fs.readFile(file, "utf8"));
  } catch {
    throw new Error("Open this Godot project with the meowody_mcp editor plugin enabled to use live editor tools");
  }
  const url = new URL(endpoint.url);
  if (endpoint.protocol !== 1 || typeof endpoint.project_path !== "string" ||
      normalizedPath(endpoint.project_path) !== normalizedPath(projectRoot) ||
      url.protocol !== "ws:" || url.hostname !== "127.0.0.1" || !url.port ||
      url.username || url.password || typeof endpoint.token !== "string" || !endpoint.token) {
    throw new Error("The editor bridge endpoint does not match this local Godot project");
  }
  const id = ++nextId;
  const ws = new WebSocket(url, { maxPayload: 32 * 1024 * 1024 });
  return new Promise((resolve, reject) => {
    let finished = false;
    const finish = (error?: Error, result?: BridgeResponse) => {
      if (finished) return;
      finished = true;
      clearTimeout(timer);
      ws.terminate();
      if (error) reject(error);
      else resolve(result!);
    };
    const timer = setTimeout(() => finish(new Error(`The Godot editor did not reply within ${timeoutMs} ms`)), timeoutMs);
    ws.on("error", (error) => finish(new Error(`Cannot connect to this project's editor: ${error.message}`)));
    ws.on("close", () => finish(new Error("The Godot editor disconnected before replying")));
    ws.on("open", () => {
      if (!finished) ws.send(JSON.stringify({ id, ...req, project_path: projectRoot.replaceAll("\\", "/"), token: endpoint.token }));
    });
    ws.on("message", (data) => {
      if (finished) return;
      try {
        const response = JSON.parse(data.toString()) as BridgeResponse;
        if (response.id !== id) return;
        if (typeof response.ok !== "boolean") throw new Error("Missing response status");
        finish(undefined, response);
      } catch (error) {
        finish(new Error(`Invalid editor bridge response: ${(error as Error).message}`));
      }
    });
  });
}
