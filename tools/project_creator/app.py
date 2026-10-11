"""Desktop UI; workers report through a queue, never touch Tk widgets."""
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from creator import build_and_verify, create_project, inspect_engine, preflight, release_versions


class Application:
    def __init__(self, window):
        self.window = window
        window.title("Meowody 项目创建器")
        window.geometry("820x640")
        self.cache = Path(os.environ.get("LOCALAPPDATA", str(Path.home()))) / "MeowodyProjectCreator"
        self.events = queue.Queue()
        self.busy = False
        self.created = None
        self.engine = None
        self.settings = self.cache / "settings.json"
        try:
            saved = json.loads(self.settings.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            saved = {}
        self.name = tk.StringVar(value="MyGame")
        self.destination = tk.StringVar()
        self.ref = tk.StringVar(value=saved.get("ref", "main"))
        self.mode = tk.StringVar(value=saved.get("mode", "download"))
        self.version = tk.StringVar(value=saved.get("version", "4.7.2-stable"))
        self.local = tk.StringVar(value=saved.get("local", ""))
        self.status = tk.StringVar(value="选择版本与目录，然后创建。需要本机 C++ 构建工具。")
        frame = ttk.Frame(window, padding=16)
        frame.pack(fill="both", expand=True)
        frame.columnconfigure(1, weight=1)
        self.fields = []
        for row, label, variable in ((0, "项目名称", self.name), (1, "目标目录（新建或空目录）", self.destination),
                                     (2, "框架分支 / 标签 / 提交", self.ref)):
            ttk.Label(frame, text=label).grid(row=row, column=0, sticky="w", pady=5)
            entry = ttk.Entry(frame, textvariable=variable)
            entry.grid(row=row, column=1, sticky="ew", padx=8)
            self.fields.append(entry)
        self.browse_destination = ttk.Button(frame, text="选择", command=self.choose_destination)
        self.browse_destination.grid(row=1, column=2)
        ttk.Label(frame, text="框架来源：github.com/baizeyv/meowody-godot").grid(row=3, column=0, columnspan=3, sticky="w", pady=8)
        for row, text, value in ((4, "下载官方 Godot", "download"), (5, "使用本地 Godot", "local")):
            radio = ttk.Radiobutton(frame, text=text, value=value, variable=self.mode)
            radio.grid(row=row, column=0, sticky="w", pady=5)
            self.fields.append(radio)
        self.versions = ttk.Combobox(frame, textvariable=self.version)
        self.versions.grid(row=4, column=1, sticky="ew", padx=8)
        self.refresh_button = ttk.Button(frame, text="获取版本", command=self.refresh_versions)
        self.refresh_button.grid(row=4, column=2)
        local_entry = ttk.Entry(frame, textvariable=self.local)
        local_entry.grid(row=5, column=1, sticky="ew", padx=8)
        self.browse_engine = ttk.Button(frame, text="选择 EXE", command=self.choose_engine)
        self.browse_engine.grid(row=5, column=2)
        self.fields += [self.versions, local_entry, self.browse_destination, self.browse_engine, self.refresh_button]
        ttk.Label(frame, text="新工程使用 project/；4.7 为验证基线，其他 Godot 4 版本可尝试。", wraplength=760).grid(row=6, column=0, columnspan=3, sticky="w", pady=8)
        actions = ttk.Frame(frame)
        actions.grid(row=7, column=0, columnspan=3, sticky="w", pady=8)
        self.create_button = ttk.Button(actions, text="创建并验证", command=self.create)
        self.create_button.pack(side="left")
        self.rebuild_button = ttk.Button(actions, text="重新编译并验证", command=self.rebuild, state="disabled")
        self.rebuild_button.pack(side="left", padx=8)
        self.open_button = ttk.Button(actions, text="打开 Godot 项目", command=self.open_project, state="disabled")
        self.open_button.pack(side="left")
        self.folder_button = ttk.Button(actions, text="打开目录", command=self.open_folder, state="disabled")
        self.folder_button.pack(side="left", padx=8)
        ttk.Label(frame, textvariable=self.status, wraplength=760).grid(row=8, column=0, columnspan=3, sticky="w")
        progress = ttk.Progressbar(frame, mode="indeterminate")
        progress.grid(row=9, column=0, columnspan=3, sticky="ew", pady=8)
        self.progress = progress
        self.log = tk.Text(frame, wrap="word", height=18, state="disabled")
        self.log.grid(row=10, column=0, columnspan=3, sticky="nsew")
        frame.rowconfigure(10, weight=1)
        window.protocol("WM_DELETE_WINDOW", self.close)
        window.after(100, self.poll)

    def choose_destination(self):
        parent = filedialog.askdirectory(title="选择父目录，将创建项目名称对应的子目录")
        if parent:
            name = self.name.get().strip()
            if not name or any(character in name for character in '<>:"/\\|?*'):
                messagebox.showerror("目录名称", "请输入合法的 Windows 项目名称。")
                return
            self.destination.set(str(Path(parent) / name))

    def choose_engine(self):
        path = filedialog.askopenfilename(title="选择 Godot 引擎", filetypes=[("Godot", "*.exe")])
        if path:
            self.local.set(path)
            self.mode.set("local")

    def start(self, work):
        if self.busy:
            return
        self.busy = True
        self.open_button.configure(state="disabled")
        self.rebuild_button.configure(state="disabled")
        self.create_button.configure(state="disabled")
        for widget in self.fields:
            widget.configure(state="disabled")
        self.progress.start()
        def worker():
            try:
                work()
            except Exception as error:
                self.events.put(("error", str(error)))
            finally:
                self.events.put(("idle", None))
        threading.Thread(target=worker, daemon=True).start()

    def refresh_versions(self):
        self.start(lambda: self.events.put(("versions", release_versions())))

    def create(self):
        name, directory, ref = self.name.get().strip(), self.destination.get().strip(), self.ref.get().strip()
        local, mode, version = self.local.get().strip(), self.mode.get(), self.version.get().strip()
        if not name or not directory or (mode == "local" and not local):
            messagebox.showerror("缺少信息", "请输入项目名称、目标目录和引擎版本或路径。")
            return
        self.created = None
        self.engine = None
        self.folder_button.configure(state="disabled")
        self.status.set("正在准备依赖、创建工程和编译…")
        def work():
            self.cache.mkdir(parents=True, exist_ok=True)
            self.settings.write_text(json.dumps({"ref": ref, "mode": mode, "version": version, "local": local}), encoding="utf-8")
            executable = create_project(name, Path(directory), ref, Path(local) if mode == "local" else version,
                                        self.cache, lambda line: self.events.put(("log", line)))
            self.events.put(("created", (Path(directory).resolve(), executable)))
        self.start(work)

    def rebuild(self):
        directory = Path(self.destination.get()).resolve()
        engine = self.engine or (Path(self.local.get()) if self.mode.get() == "local" else None)
        if engine is None:
            messagebox.showerror("引擎路径", "请先选择本地 Godot EXE，再重新编译。")
            return
        def work():
            preflight()
            version, _ = inspect_engine(engine)
            config = json.loads((directory / "meowody-project.json").read_text(encoding="utf-8"))
            if config["godot_version"] != version:
                raise ValueError("请使用创建项目时的 Godot 版本。")
            if not (directory / "godot-cpp/SConstruct").is_file():
                raise ValueError("godot-cpp 下载尚未完成，请使用新的空目录重新创建。")
            log_path = directory / "build/creator/rebuild.log"
            log_path.parent.mkdir(parents=True, exist_ok=True)
            with log_path.open("w", encoding="utf-8") as file:
                def report(line):
                    file.write(line + "\n")
                    file.flush()
                    self.events.put(("log", line))
                build_and_verify(directory, engine.resolve(), report)
            self.events.put(("created", (directory, engine.resolve())))
        self.start(work)

    def open_project(self):
        subprocess.Popen([str(self.engine), "--editor", "--path", str(self.created / "project")])

    def open_folder(self):
        os.startfile(str(self.created or Path(self.destination.get()).resolve()))

    def poll(self):
        try:
            while True:
                event, value = self.events.get_nowait()
                if event == "log":
                    self.log.configure(state="normal")
                    self.log.insert("end", value + "\n")
                    self.log.see("end")
                    self.log.configure(state="disabled")
                elif event == "versions":
                    self.versions.configure(values=value)
                    self.status.set("已获取官方稳定版本；也可以手动输入版本。")
                elif event == "created":
                    self.created, self.engine = value
                    self.status.set("创建成功，编译、插件导入和框架加载均通过。")
                    self.open_button.configure(state="normal")
                    self.folder_button.configure(state="normal")
                elif event == "error":
                    self.status.set("未完成：" + value)
                    messagebox.showerror("未完成", value + "\n生成过的工程与日志会保留。")
                elif event == "idle":
                    self.busy = False
                    self.progress.stop()
                    self.create_button.configure(state="normal")
                    for widget in self.fields:
                        widget.configure(state="normal")
                    if self.created and self.engine:
                        self.open_button.configure(state="normal")
                    directory = self.destination.get().strip()
                    if directory and (Path(directory) / "meowody-project.json").is_file():
                        self.rebuild_button.configure(state="normal")
                        self.folder_button.configure(state="normal")
        except queue.Empty:
            pass
        self.window.after(100, self.poll)

    def close(self):
        if self.busy:
            messagebox.showinfo("任务正在运行", "请等待下载或编译完成后关闭窗口。")
        else:
            self.window.destroy()


if __name__ == "__main__":
    window = tk.Tk()
    if len(sys.argv) == 3 and sys.argv[1] == "--self-test":
        window.withdraw()
        application = Application(window)
        window.update()
        Path(sys.argv[2]).write_text(json.dumps({"tk": tk.TkVersion, "default_ref": application.ref.get(), "ui_loaded": True}), encoding="utf-8")
        window.destroy()
    else:
        Application(window)
        window.mainloop()
