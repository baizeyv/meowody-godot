import ctypes as c,struct,subprocess,os,site,time
from pathlib import Path
k=c.WinDLL('kernel32',use_last_error=True)
k.WaitForDebugEvent.argtypes=[c.c_void_p,c.c_uint32];k.ContinueDebugEvent.argtypes=[c.c_uint32,c.c_uint32,c.c_uint32]
k.GetFinalPathNameByHandleW.argtypes=[c.c_void_p,c.c_wchar_p,c.c_uint32,c.c_uint32]
k.GetThreadContext.argtypes=[c.c_void_p,c.c_void_p]
k.ReadProcessMemory.argtypes=[c.c_void_p,c.c_void_p,c.c_void_p,c.c_size_t,c.c_void_p]
k.OpenThread.argtypes=[c.c_uint32,c.c_bool,c.c_uint32];k.OpenThread.restype=c.c_void_p
k.CloseHandle.argtypes=[c.c_void_p]
p=Path('tests/workspace').resolve();e=os.environ.copy();e['APPDATA']=str(p/'profile');e['PYTHONUSERBASE']=site.USER_BASE;e['_NO_DEBUG_HEAP']='1'
(p/'meow/tests/meowody_binding/editor_test.gd').write_text('''@tool
extends RefCounted
func run(plugin: EditorPlugin) -> void:
    await plugin.get_tree().create_timer(1).timeout
    await plugin._generate()
    await plugin.get_tree().create_timer(1).timeout
    print("quitting")
    plugin.get_tree().quit()
''')
log=(p/'debug-exit.log').open('w');proc=subprocess.Popen(['D:/Develop Tools/Godot_v4.7.2-stable_win64.exe/Godot_v4.7.2-stable_win64.exe','--headless','--editor','--path',str(p/'meow'),'--','--test-binding-editor'],stdout=log,stderr=subprocess.STDOUT,env=e,creationflags=2)
mods=[];hp=0;last=time.monotonic();main_thread=0

def module(base,h):
 b=c.create_unicode_buffer(4096);k.GetFinalPathNameByHandleW(h,b,4096,0)
 try:
  data=Path(b.value).read_bytes();pe=struct.unpack_from('<I',data,60)[0];size=struct.unpack_from('<I',data,pe+80)[0]
 except: size=0x1000000
 mods.append([base,size,b.value,True]);k.CloseHandle(h)

def desc(addr):
 for base,size,name,alive in reversed(mods):
  if base<=addr<base+size:return f'{name}+{addr-base:x} ({"loaded" if alive else "UNLOADED"})'
 return hex(addr)

while True:
 if time.monotonic()-last>25:
  last=time.monotonic()
  th=k.OpenThread(0x4a,False,main_thread)
  k.SuspendThread.argtypes=[c.c_void_p];k.ResumeThread.argtypes=[c.c_void_p]
  k.SuspendThread(th)
  ctx=c.create_string_buffer(1232);struct.pack_into('<I',ctx,48,0x100003)
  if k.GetThreadContext(th,ctx):
   rsp=struct.unpack_from('<Q',ctx.raw,152)[0];rip=struct.unpack_from('<Q',ctx.raw,248)[0];print('LIVE',desc(rip),flush=True);buf=c.create_string_buffer(2048)
   if k.ReadProcessMemory(hp,rsp,buf,2048,None):
    hits=[]
    for x in struct.unpack('<256Q',buf.raw):
     d=desc(x)
     if not d.startswith('0x') and d not in hits:hits.append(d)
    print('\n'.join(hits[:25]),flush=True)
  k.ResumeThread(th);k.CloseHandle(th)
 ev=c.create_string_buffer(176)
 if not k.WaitForDebugEvent(ev,1000):
  err=c.get_last_error()
  if err!=121: print('WAIT ERROR',err,flush=True);break
  continue
 code,pid,tid=struct.unpack_from('<III',ev.raw);cont=0x10002
 if code==3:
  main_thread=tid
  hf,hp,ht,base=struct.unpack_from('<QQQQ',ev.raw,16);module(base,hf)
 elif code==6:
  hf,base=struct.unpack_from('<QQ',ev.raw,16);module(base,hf)
 elif code==7:
  base=struct.unpack_from('<Q',ev.raw,16)[0]
  for m in mods:
   if m[0]==base:m[3]=False
 elif code==1:
  ex=struct.unpack_from('<I',ev.raw,16)[0];addr=struct.unpack_from('<Q',ev.raw,32)[0];chance=struct.unpack_from('<I',ev.raw,168)[0]
  print('EVENT EX',hex(ex),hex(addr),'first',chance,flush=True)
  if ex!=0x80000003:
   cont=0x80010001
   if ex==0xc0000005:
    print('EXCEPTION',hex(ex),desc(addr),'first',chance,flush=True)
    th=k.OpenThread(0x48,False,tid);ctx=c.create_string_buffer(1232);struct.pack_into('<I',ctx,48,0x100003)
    if k.GetThreadContext(th,ctx):
     rsp=struct.unpack_from('<Q',ctx.raw,152)[0];buf=c.create_string_buffer(4096)
     if k.ReadProcessMemory(hp,rsp,buf,4096,None):
      hits=[]
      for x in struct.unpack('<512Q',buf.raw):
       d=desc(x)
       if not d.startswith('0x') and d not in hits:hits.append(d)
      print('\n'.join(hits[:35]),flush=True)
    k.CloseHandle(th)
 elif code==5:
  print('EXIT',struct.unpack_from('<I',ev.raw,16)[0],flush=True);k.ContinueDebugEvent(pid,tid,cont);break
 k.ContinueDebugEvent(pid,tid,cont)
