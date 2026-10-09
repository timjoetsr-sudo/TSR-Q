# rende l'HTML del plugin puro ASCII (alcune WebView ignorano il charset delle risorse locali)
import re,sys
src,dst=sys.argv[1],sys.argv[2]; s=open(src,encoding='utf-8').read()
def js(t): return ''.join(c if ord(c)<128 else ''.join('\\u%04x'%u for u in (c.encode('utf-16-be') and [int.from_bytes(c.encode('utf-16-be')[i:i+2],'big') for i in range(0,len(c.encode('utf-16-be')),2)])) for c in t)
def css(t): return ''.join(c if ord(c)<128 else '\\%x '%ord(c) for c in t)
def html(t): return ''.join(c if ord(c)<128 else '&#x%x;'%ord(c) for c in t)
out=[];i=0
for m in re.finditer(r'(<script\b[^>]*>)(.*?)(</script>)|(<style\b[^>]*>)(.*?)(</style>)',s,re.S):
    out.append(html(s[i:m.start()]))
    if m.group(1): out.append(m.group(1)+js(m.group(2))+m.group(3))
    else: out.append(m.group(4)+css(m.group(5))+m.group(6))
    i=m.end()
out.append(html(s[i:])); r=''.join(out); assert all(ord(c)<128 for c in r); open(dst,'w',encoding='ascii').write(r); print('ok',len(r))
