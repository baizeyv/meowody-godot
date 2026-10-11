#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";
import { promises as fs } from "node:fs";
import path from "node:path";
import { spawn } from "node:child_process";
import {
  parseTscn,
  serializeTscn,
  getSceneTree,
  addNode,
  removeNode,
  setNodeProperties,
  addExtResource,
  createEmptyScene,
} from "./tscn.js";
import { callBridge, type BridgeRequest } from "./bridge.js";
import { assertProject, resolveInProject, toResPath } from "./project.js";

const server = new McpServer({
  name: "meowody-godot",
  version: "1.0.0",
});

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function text(s: string) {
  return { content: [{ type: "text" as const, text: s }] };
}

function errText(e: unknown) {
  return {
    content: [{ type: "text" as const, text: `Error: ${e instanceof Error ? e.message : String(e)}` }],
    isError: true,
  };
}

async function findGodotBinary(): Promise<string> {
  return process.env.GODOT_BIN ?? "godot4";
}

/** GDScript-only syntax such as `RectangleShape2D.new()` would corrupt a .tscn file. */
function assertSceneFileValues(properties: Record<string, string> | undefined): void {
  for (const [key, value] of Object.entries(properties ?? {})) {
    if (/\.new\s*\(|\b(?:load|node)\s*\(/.test(value)) {
      throw new Error(
        `Property "${key}": \`${value}\` is GDScript, not scene-file syntax, and would corrupt the .tscn. ` +
          `Resources in a scene file must be SubResource(...) entries. To create one (e.g. a collision shape), ` +
          `open the scene in the Godot editor and use editor_add_node / editor_set_properties, which accept "RectangleShape2D.new()".`
      );
    }
  }
}

/**
 * Refuses an on-disk edit of a scene that the running Godot editor has open: the editor keeps its
 * own in-memory copy, so the two would silently diverge and one save would overwrite the other.
 * If the editor isn't running (or has a different project open) the edit goes ahead.
 */
async function assertSceneNotOpenInEditor(projectPath: string, scenePath: string): Promise<void> {
  try {
    await fs.access(path.join(projectPath, ".godot/meowody_mcp/bridge.json"));
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") return;
    throw error;
  }
  const res = await callBridge({command: "get_open_scenes"}, 3000);
  if (!res.ok) throw new Error(res.error ?? "Cannot verify the editor's open scenes");
  const info = res.result as {project_path?: string; open?: string[]};
  if (!info?.project_path || !Array.isArray(info.open)) return;

  const norm = (p: string) => (process.platform === "win32" ? path.resolve(p).toLowerCase() : path.resolve(p));
  if (norm(info.project_path) !== norm(projectPath)) return; // the editor has a different project open

  const resPath = toResPath(projectPath, await resolveInProject(projectPath, scenePath));
  if (info.open.includes(resPath)) {
    throw new Error(
      `${resPath} is open in the Godot editor, so editing the file on disk would desync it from the editor ` +
        `(and one of the two saves would overwrite the other). Use editor_add_node / editor_set_properties / ` +
        `editor_remove_node to edit it live in the editor, or close the scene's tab in Godot first.`
    );
  }
}

// ---------------------------------------------------------------------------
// Phase 1 — file-based tools
// ---------------------------------------------------------------------------

server.tool(
  "list_project",
  "Lists scenes (.tscn), scripts (.gd), and resources (.tres/.res) in a Godot project folder, relative to project_path.",
  { project_path: z.string().describe("Absolute path to the Godot project folder (containing project.godot)") },
  async ({ project_path }) => {
    try {
      await assertProject(project_path);
      const results: Record<string, string[]> = { scenes: [], scripts: [], resources: [] };
      const skipDirs = new Set([".godot", ".git", "node_modules"]);

      async function walk(dir: string) {
        const entries = await fs.readdir(dir, { withFileTypes: true });
        for (const entry of entries) {
          const full = path.join(dir, entry.name);
          if (entry.isDirectory()) {
            if (!skipDirs.has(entry.name)) await walk(full);
          } else {
            const rel = path.relative(project_path, full).split(path.sep).join("/");
            const ext = path.extname(entry.name);
            if (ext === ".tscn") results.scenes.push(rel);
            else if (ext === ".gd") results.scripts.push(rel);
            else if (ext === ".tres" || ext === ".res") results.resources.push(rel);
          }
        }
      }
      await walk(project_path);
      for (const list of Object.values(results)) list.sort();
      return text(JSON.stringify(results, null, 2));
    } catch (e) {
      return errText(e);
    }
  }
);

server.tool(
  "read_scene",
  "Parses a .tscn file and returns its node tree as JSON (name, type, parent path, properties).",
  {
    project_path: z.string(),
    scene_path: z.string().describe("Path to the .tscn file, relative to project_path or absolute"),
  },
  async ({ project_path, scene_path }) => {
    try {
      const full = await resolveInProject(project_path, scene_path);
      const raw = await fs.readFile(full, "utf-8");
      const file = parseTscn(raw);
      const tree = getSceneTree(file);
      return text(JSON.stringify(tree, null, 2));
    } catch (e) {
      return errText(e);
    }
  }
);

server.tool(
  "create_scene",
  "Creates a new .tscn file with a single root node. Use add_node afterward to build out the tree.",
  {
    project_path: z.string(),
    scene_path: z.string().describe("Where to write the new scene, e.g. scenes/Player.tscn"),
    root_name: z.string(),
    root_type: z.string().describe('Godot node class, e.g. "Node2D", "CharacterBody2D", "Control"'),
    overwrite: z.boolean().optional().default(false),
  },
  async ({ project_path, scene_path, root_name, root_type, overwrite }) => {
    try {
      const full = await resolveInProject(project_path, scene_path);
      if (!overwrite) {
        const exists = await fs.stat(full).then(() => true).catch(() => false);
        if (exists) throw new Error(`${scene_path} already exists (pass overwrite: true to replace it)`);
      } else {
        await assertSceneNotOpenInEditor(project_path, scene_path);
      }
      await fs.mkdir(path.dirname(full), { recursive: true });
      const file = createEmptyScene(root_name, root_type);
      await fs.writeFile(full, serializeTscn(file), "utf-8");
      return text(`Created ${scene_path} with root node "${root_name}" (${root_type}).`);
    } catch (e) {
      return errText(e);
    }
  }
);

server.tool(
  "add_node",
  "Adds a child node to an existing scene at the given parent path (use \".\" for the scene root).",
  {
    project_path: z.string(),
    scene_path: z.string(),
    parent_path: z.string().describe('Path of the parent node, e.g. "." or "Player" or "Player/Sprite2D"'),
    name: z.string(),
    type: z.string().describe('Godot node class, e.g. "Sprite2D", "CollisionShape2D"'),
    properties: z
      .record(z.string())
      .optional()
      .describe('Raw property assignments, e.g. { "position": "Vector2(10, 20)" }'),
    script_path: z
      .string()
      .optional()
      .describe("If set, attaches this .gd script to the new node (path relative to project_path)"),
  },
  async ({ project_path, scene_path, parent_path, name, type, properties, script_path }) => {
    try {
      assertSceneFileValues(properties);
      const full = await resolveInProject(project_path, scene_path);
      await assertSceneNotOpenInEditor(project_path, scene_path);
      const raw = await fs.readFile(full, "utf-8");
      const file = parseTscn(raw);

      let scriptExtResourceId: string | undefined;
      if (script_path) {
        const scriptFull = await resolveInProject(project_path, script_path);
        scriptExtResourceId = addExtResource(file, "Script", toResPath(project_path, scriptFull));
      }

      addNode(file, { name, type, parentPath: parent_path, properties, scriptExtResourceId });
      await fs.writeFile(full, serializeTscn(file), "utf-8");
      return text(`Added node "${name}" (${type}) under "${parent_path}" in ${scene_path}.`);
    } catch (e) {
      return errText(e);
    }
  }
);

server.tool(
  "remove_node",
  "Removes a node (and its descendants) from a scene.",
  {
    project_path: z.string(),
    scene_path: z.string(),
    node_path: z.string(),
  },
  async ({ project_path, scene_path, node_path }) => {
    try {
      const full = await resolveInProject(project_path, scene_path);
      await assertSceneNotOpenInEditor(project_path, scene_path);
      const raw = await fs.readFile(full, "utf-8");
      const file = parseTscn(raw);
      removeNode(file, node_path);
      await fs.writeFile(full, serializeTscn(file), "utf-8");
      return text(`Removed "${node_path}" from ${scene_path}.`);
    } catch (e) {
      return errText(e);
    }
  }
);

server.tool(
  "set_node_properties",
  "Sets or overwrites one or more properties on an existing node in a scene.",
  {
    project_path: z.string(),
    scene_path: z.string(),
    node_path: z.string(),
    properties: z.record(z.string()).describe('e.g. { "position": "Vector2(100, 50)", "visible": "false" }'),
  },
  async ({ project_path, scene_path, node_path, properties }) => {
    try {
      assertSceneFileValues(properties);
      const full = await resolveInProject(project_path, scene_path);
      await assertSceneNotOpenInEditor(project_path, scene_path);
      const raw = await fs.readFile(full, "utf-8");
      const file = parseTscn(raw);
      setNodeProperties(file, node_path, properties);
      await fs.writeFile(full, serializeTscn(file), "utf-8");
      return text(`Updated properties on "${node_path}" in ${scene_path}.`);
    } catch (e) {
      return errText(e);
    }
  }
);

server.tool(
  "write_script",
  "Creates or fully overwrites a GDScript (.gd) file.",
  {
    project_path: z.string(),
    script_path: z.string().describe("e.g. scripts/Player.gd"),
    content: z.string(),
  },
  async ({ project_path, script_path, content }) => {
    try {
      const full = await resolveInProject(project_path, script_path);
      await fs.mkdir(path.dirname(full), { recursive: true });
      await fs.writeFile(full, content, "utf-8");
      return text(`Wrote ${script_path} (${content.split("\n").length} lines).`);
    } catch (e) {
      return errText(e);
    }
  }
);

server.tool(
  "read_script",
  "Reads a GDScript (.gd) file's contents.",
  { project_path: z.string(), script_path: z.string() },
  async ({ project_path, script_path }) => {
    try {
      const full = await resolveInProject(project_path, script_path);
      const content = await fs.readFile(full, "utf-8");
      return text(content);
    } catch (e) {
      return errText(e);
    }
  }
);

server.tool(
  "edit_script",
  "Replaces one exact occurrence of old_text with new_text in a GDScript file (like a find-and-replace patch).",
  {
    project_path: z.string(),
    script_path: z.string(),
    old_text: z.string(),
    new_text: z.string(),
  },
  async ({ project_path, script_path, old_text, new_text }) => {
    try {
      const full = await resolveInProject(project_path, script_path);
      const content = await fs.readFile(full, "utf-8");
      const count = content.split(old_text).length - 1;
      if (count === 0) throw new Error("old_text not found in file");
      if (count > 1) throw new Error(`old_text matched ${count} times; make it unique`);
      // replacer function: a string would have `$&`, `$1`, ... interpreted
      const updated = content.replace(old_text, () => new_text);
      await fs.writeFile(full, updated, "utf-8");
      return text(`Edited ${script_path}.`);
    } catch (e) {
      return errText(e);
    }
  }
);

server.tool(
  "run_headless",
  "Runs the Godot project headlessly (no window) for a fixed duration to catch parse/runtime errors, or runs a specific scene headlessly. Requires the `godot4` binary (or GODOT_BIN env var) to be installed.",
  {
    project_path: z.string(),
    scene_path: z.string().optional().describe("Specific scene to run instead of the project's main scene"),
    timeout_ms: z.number().optional().default(5000),
  },
  async ({ project_path, scene_path, timeout_ms }) => {
    try {
      await assertProject(project_path);
      const godot = await findGodotBinary();
      const args = ["--headless", "--path", project_path];
      if (scene_path) args.push(await resolveInProject(project_path, scene_path));

      const output = await new Promise<string>((resolve, reject) => {
        const child = spawn(godot, args, { timeout: Math.max(100, Math.min(timeout_ms, 60000)), windowsHide: true });
        let out = "";
        child.stdout.on("data", (d) => (out += d.toString()));
        child.stderr.on("data", (d) => (out += d.toString()));
        child.on("error", (err) =>
          reject(new Error(`Could not launch "${godot}": ${err.message}. Set GODOT_BIN to the Godot 4 executable path.`))
        );
        child.on("close", (code, signal) => {
          const status = signal ? `stopped by ${signal} after the ${timeout_ms}ms timeout` : `exit code ${code}`;
          resolve(`${out}${out && !out.endsWith("\n") ? "\n" : ""}[${status}]`);
        });
      });
      return text(output);
    } catch (e) {
      return errText(e);
    }
  }
);

// ---------------------------------------------------------------------------
// Phase 2 — live editor bridge tools (require the companion EditorPlugin)
// ---------------------------------------------------------------------------

/** Sends one command to the editor bridge; a bridge-reported failure becomes an isError result. */
async function bridgeTool(req: BridgeRequest) {
  try {
    const res = await callBridge(req);
    if (!res.ok) throw new Error(res.error ?? "the Godot editor bridge reported an error");
    return text(JSON.stringify(res.result ?? null, null, 2));
  } catch (e) {
    return errText(e);
  }
}

const sceneGuard = z
  .string()
  .optional()
  .describe("Optional safety check: fail unless this is the scene currently open in the editor");

server.tool(
  "editor_ping",
  "Checks whether the Godot editor is running with the meowody_mcp plugin enabled.",
  {},
  () => bridgeTool({ command: "ping" })
);

server.tool(
  "editor_run_scene",
  "Tells the running Godot editor to play a specific scene (like pressing the Play Scene button).",
  { scene_path: z.string().describe('e.g. "res://scenes/Main.tscn"') },
  ({ scene_path }) => bridgeTool({ command: "run_scene", args: { scene_path } })
);

server.tool(
  "editor_stop",
  "Tells the running Godot editor to stop the currently playing scene.",
  {},
  () => bridgeTool({ command: "stop" })
);

server.tool(
  "editor_get_selection",
  "Returns the currently selected node(s) in the Godot editor.",
  {},
  () => bridgeTool({ command: "get_selection" })
);

// --- Live editing: these change the scene open in the editor itself, so the user watches each
// --- edit appear and can undo it with Ctrl+Z. Nothing is written to disk until the scene is saved.

server.tool(
  "editor_get_scene_tree",
  "Returns the scene currently open in the Godot editor as a node tree, plus the list of open scenes.",
  {},
  () => bridgeTool({ command: "get_scene_tree" })
);

server.tool(
  "editor_open_scene",
  "Opens a scene in the Godot editor (switching to its tab) so it can be edited live with editor_add_node, editor_set_properties and editor_remove_node.",
  { scene_path: z.string().describe('e.g. "res://scenes/Main.tscn" or "scenes/Main.tscn"') },
  ({ scene_path }) => bridgeTool({ command: "open_scene", args: { scene_path } })
);

server.tool(
  "editor_add_node",
  "Adds a node to the scene open in the Godot editor. It appears live and is selected; undo with Ctrl+Z. The scene is not saved until editor_save_scene (or the user saves). Prefer this over add_node while the editor is open.",
  {
    parent_path: z.string().describe('Path of the parent node in the open scene, e.g. "." or "Player"'),
    name: z.string(),
    type: z.string().describe('Godot Node class, e.g. "Sprite2D"'),
    properties: z
      .record(z.string())
      .optional()
      .describe(
        'Godot-syntax values, e.g. { "position": "Vector2(10, 20)", "shape": "RectangleShape2D.new()", "shape:size": "Vector2(32, 48)" }. ' +
          'Strings need quotes ("\\"hi\\""); ClassName.new() creates a Resource; "prop:subprop" reaches into a resource. Applied in order.'
      ),
    script_path: z.string().optional().describe("Attach this script, e.g. res://scripts/coin.gd"),
    scene_path: sceneGuard,
  },
  ({ parent_path, name, type, properties, script_path, scene_path }) =>
    bridgeTool({ command: "add_node", args: { parent_path, name, type, properties, script_path, scene_path } })
);

server.tool(
  "editor_set_properties",
  "Sets properties on a node in the scene open in the Godot editor (live, undoable with Ctrl+Z, not saved until editor_save_scene).",
  {
    node_path: z.string().describe('Path in the open scene, "." for the root'),
    properties: z.record(z.string()).describe('Godot-syntax values, e.g. { "position": "Vector2(100, 50)", "visible": "false" }'),
    scene_path: sceneGuard,
  },
  ({ node_path, properties, scene_path }) =>
    bridgeTool({ command: "set_properties", args: { node_path, properties, scene_path } })
);

server.tool(
  "editor_remove_node",
  "Removes a node (and its descendants) from the scene open in the Godot editor (live, undoable with Ctrl+Z).",
  { node_path: z.string(), scene_path: sceneGuard },
  ({ node_path, scene_path }) => bridgeTool({ command: "remove_node", args: { node_path, scene_path } })
);

server.tool(
  "editor_save_scene",
  "Saves the scene currently open in the Godot editor to disk (like Ctrl+S).",
  {},
  () => bridgeTool({ command: "save_scene" })
);

server.tool("editor_get_status", "Read the configured project editor status and play state.", {}, () => bridgeTool({command: "status"}));
server.tool("editor_get_node_properties", "Read properties of a node owned by the current scene.",
  {node_path: z.string(), properties: z.array(z.string()).optional()},
  ({node_path, properties}) => bridgeTool({command: "get_node_properties", args: {node_path, properties}}));
server.tool("editor_select_node", "Select a node owned by the current scene.",
  {node_path: z.string()}, ({node_path}) => bridgeTool({command: "select_node", args: {node_path}}));
server.tool("editor_undo", "Undo the latest action in the currently edited scene.", {}, () => bridgeTool({command: "undo"}));
server.tool("editor_redo", "Redo the latest action in the currently edited scene.", {}, () => bridgeTool({command: "redo"}));
server.tool("editor_get_output", "Read recent text from the Godot editor Output panel, falling back to its file log.",
  {lines: z.number().int().min(1).max(1000).optional()}, ({lines}) => bridgeTool({command: "get_editor_output", args: {lines}}));
server.tool("editor_get_screenshot", "Capture the editor 2D/3D viewport as PNG. Requires a graphical editor.",
  {viewport: z.enum(["2d", "3d"]).optional()}, async ({viewport}) => {
    try {
      const response = await callBridge({command: "screenshot", args: {viewport: viewport ?? "2d"}});
      if (!response.ok) throw new Error(response.error ?? "Screenshot failed");
      const result = response.result as {mime_type: string; data: string};
      return {content: [{type: "image" as const, mimeType: result.mime_type, data: result.data}]};
    } catch (error) { return errText(error); }
  });

const transport = new StdioServerTransport();
await server.connect(transport);

transport.onclose = () => { void server.close(); };
