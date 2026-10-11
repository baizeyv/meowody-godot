import assert from 'node:assert/strict';
import {readFile, writeFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
import {Client} from '../tools/godot_mcp/node_modules/@modelcontextprotocol/sdk/dist/esm/client/index.js';
import {StdioClientTransport} from '../tools/godot_mcp/node_modules/@modelcontextprotocol/sdk/dist/esm/client/stdio.js';
import WebSocket from '../tools/godot_mcp/node_modules/ws/wrapper.mjs';

const project = path.resolve(process.argv[2]);
const visual = process.argv.includes('--visual');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const client = new Client({name: 'meowody-mcp-test', version: '1.0'});
const transport = new StdioClientTransport({command: process.execPath,
  args: [path.join(root, 'tools/godot_mcp/dist/index.js'), '--project', project], stderr: 'pipe'});
await client.connect(transport);
async function call(name, args = {}, fails = false) {
  const response = await client.callTool({name, arguments: args});
  assert.equal(!!response.isError, fails, `${name}: ${JSON.stringify(response)}`);
  if (fails) return response;
  return response.content[0].type === 'text' ? JSON.parse(response.content[0].text) : response;
}
async function properties(node_path, keys) {
  return (await call('editor_get_node_properties', {node_path, properties: keys})).values;
}
async function rawRequest(overrides) {
  const endpoint = JSON.parse(await readFile(path.join(project, '.godot/meowody_mcp/bridge.json'), 'utf8'));
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(endpoint.url);
    const timer = setTimeout(() => {ws.terminate(); reject(new Error('Auth probe timeout'));}, 4000);
    ws.on('error', reject);
    ws.on('open', () => ws.send(JSON.stringify({id: 999, command: 'status', args: {},
      token: endpoint.token, project_path: project.replaceAll('\\', '/'), ...overrides})));
    ws.on('message', data => {clearTimeout(timer); ws.close(); resolve(JSON.parse(data.toString()));});
  });
}
try {
  const tools = await client.listTools();
  assert.ok(tools.tools.some(t => t.name === 'editor_get_status'));
  if (process.argv.includes('--offline')) {
    await call('editor_get_status', {}, true);
    console.log('[MCPTest] PASS offline initialize/list/error');
  } else {
    const status = await call('editor_get_status');
    assert.equal(status.editor, 'meowody_mcp');
    assert.equal((await rawRequest({token: 'wrong'})).ok, false);
    assert.equal((await rawRequest({project_path: path.join(project, 'wrong')})).ok, false);
    await call('editor_open_scene', {scene_path: 'res://scene.tscn'});
    await call('editor_add_node', {parent_path: '.', name: 'Coins', type: 'NumberLabel',
      properties: {value: '1234', prefix: '"金币："'}});
    assert.equal((await properties('Coins', ['value', 'prefix'])).value, 1234);
    assert.equal((await properties('Coins', ['prefix'])).prefix, '金币：');
    await call('editor_set_properties', {node_path: 'Coins', properties: {value: '2222'}});
    await call('editor_undo');
    assert.equal((await properties('Coins', ['value'])).value, 1234);
    await call('editor_redo');
    assert.equal((await properties('Coins', ['value'])).value, 2222);
    await call('editor_set_properties', {node_path: 'Coins', properties: {value: '3333', unknown_field: '1'}}, true);
    assert.equal((await properties('Coins', ['value'])).value, 2222);
    await call('editor_set_properties', {node_path: 'Coins', scene_path: 'res://wrong.tscn', properties: {value: '4444'}}, true);
    await call('editor_get_node_properties', {node_path: '../', properties: ['name']}, true);
    await call('editor_get_node_properties', {node_path: '/root', properties: ['name']}, true);
    await call('editor_select_node', {node_path: 'Coins'});
    assert.deepEqual((await call('editor_get_selection')).selected, ['Coins']);
    await call('editor_remove_node', {node_path: 'Coins'});
    await call('editor_get_node_properties', {node_path: 'Coins'}, true);
    await call('editor_undo');
    assert.equal((await properties('Coins', ['prefix'])).prefix, '金币：');
    await call('editor_add_node', {parent_path: '.', name: 'Patch', type: 'MultiPatchRect',
      properties: {sprite: 'load("res://sprite.tres")'}});
    assert.equal((await properties('Patch', ['sprite'])).sprite.class_name, 'MultiPatchSprite');
    await call('editor_set_properties', {node_path: 'Patch', properties: {sprite: 'node("Coins")'}}, true);
    await call('set_node_properties', {project_path: project, scene_path: 'scene.tscn', node_path: 'Coins', properties: {value: '0'}}, true);
    await call('read_scene', {project_path: root, scene_path: 'meow/ttt.tscn'}, true);
    await call('read_script', {project_path: project, script_path: '../outside.gd'}, true);
    await call('editor_save_scene');
    assert.match(await readFile(path.join(project, 'scene.tscn'), 'utf8'), /1234|2222/);
    await call('editor_open_scene', {scene_path: 'res://binding.tscn'});
    await call('editor_add_node', {parent_path: '.', name: 'Color', type: 'ColorRect'});
    await call('editor_set_properties', {node_path: '.', properties: {m_color_rect: 'node("Color")'}});
    assert.equal((await properties('.', ['m_color_rect'])).m_color_rect.node_path, 'Color');
    await call('editor_open_scene', {scene_path: 'res://scene.tscn'});
    await call('editor_run_scene', {scene_path: 'res://scene.tscn'});
    let playing = false;
    for (let i = 0; i < 30; ++i) {
      playing = (await call('editor_get_status')).playing;
      if (playing) break;
      await new Promise(r => setTimeout(r, 100));
    }
    assert.equal(playing, true);
    await call('editor_stop');
    for (let i = 0; i < 30 && (await call('editor_get_status')).playing; ++i)
      await new Promise(r => setTimeout(r, 100));
    assert.equal((await call('editor_get_status')).playing, false);
    if (visual) {
      const screenshot = await call('editor_get_screenshot', {viewport: '2d'});
      assert.equal(screenshot.content[0].type, 'image');
      const bytes = Buffer.from(screenshot.content[0].data, 'base64');
      assert.equal(bytes.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
      assert.ok(bytes.length > 1000);
      await writeFile(path.join(project, 'mcp-viewport.png'), bytes);
    } else await call('editor_get_screenshot', {}, true);
    const output = await call('editor_get_output', {lines: 10});
    assert.ok(Array.isArray(output.lines));
    console.log(`[MCPTest] PASS ${tools.tools.length} tools; auth, native edits, undo, resources, references, play, capture`);
  }
} finally {await client.close();}
