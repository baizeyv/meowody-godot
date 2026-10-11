import { promises as fs } from "node:fs";
import path from "node:path";

export function normalizedPath(value: string): string {
  const resolved = path.resolve(value);
  return process.platform === "win32" ? resolved.toLowerCase() : resolved;
}

const projectArg = process.argv.indexOf("--project");
if (projectArg < 0 || !process.argv[projectArg + 1]) {
  throw new Error("Usage: node dist/index.js --project <Godot project folder>");
}

export const projectRoot = await fs.realpath(process.argv[projectArg + 1]);
if (!(await fs.stat(path.join(projectRoot, "project.godot"))).isFile()) {
  throw new Error(`No project.godot found in ${projectRoot}`);
}

export async function assertProject(projectPath: string): Promise<void> {
  if (normalizedPath(await fs.realpath(projectPath)) !== normalizedPath(projectRoot)) {
    throw new Error(`This MCP server is bound to ${projectRoot}; refusing a different project`);
  }
}

function assertContained(candidate: string): void {
  const relative = path.relative(projectRoot, candidate);
  if (relative === ".." || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
    throw new Error(`Refusing a path outside the configured project: ${candidate}`);
  }
}

/** Check the existing target or nearest existing parent, including Windows junctions. */
export async function resolveInProject(projectPath: string, value: string): Promise<string> {
  await assertProject(projectPath);
  const full = path.resolve(projectRoot, value.startsWith("res://") ? value.slice(6) : value);
  assertContained(full);
  let existing = full;
  for (;;) {
    try {
      assertContained(await fs.realpath(existing));
      return full;
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
      const parent = path.dirname(existing);
      if (parent === existing) throw error;
      existing = parent;
    }
  }
}

export function toResPath(projectPath: string, fullPath: string): string {
  return `res://${path.relative(projectPath, fullPath).split(path.sep).join("/")}`;
}
