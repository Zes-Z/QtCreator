
import os
ROOT = r"D:\QtpDav"
import shutil
src = r"D:\QtpDav\_sample.pdf"
def pdf_bytes(pages=3):
    objs=[]; n=3; pn=[]
    for i in range(pages):
        p=n; n+=1; c=n; n+=1; pn.append((p,c))
    fn=n
    kids=" ".join("%d 0 R"%p for p,c in pn)
    objs.append((1,b"<< /Type /Catalog /Pages 2 0 R >>"))
    objs.append((2,("<< /Type /Pages /Kids [%s] /Count %d >>"%(kids,pages)).encode()))
    for i,(p,c) in enumerate(pn):
        ct=("BT /F1 24 Tf 72 120 Td (Page %d) Tj ET"%(i+1)).encode()
        objs.append((p,("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Resources << /Font << /F1 %d 0 R >> >> /Contents %d 0 R >>"%(fn,c)).encode()))
        objs.append((c,b"<< /Length "+str(len(ct)).encode()+b" >>\nstream\n"+ct+b"\nendstream"))
    objs.append((fn,b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"))
    objs.sort(key=lambda x:x[0])
    out=bytearray(b"%PDF-1.4\n"); off={}
    for num,body in objs:
        off[num]=len(out); out+=("%d 0 obj\n"%num).encode()+body+b"\nendobj\n"
    xp=len(out); mx=max(off)+1
    out+=("xref\n0 %d\n"%mx).encode()+b"0000000000 65535 f \n"
    for k in range(1,mx):
        out+=(("%010d 00000 n \n"%off[k]).encode() if k in off else b"0000000000 65535 f \n")
    out+=("trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n"%(mx,xp)).encode()
    return bytes(out)
DATA = pdf_bytes(3)
for rel in [r"zesbks\paper_one\paper_one.pdf",
            r"zesbks\group\group me\group_paper\group_paper.pdf"]:
    dst = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst,'wb').write(DATA)
    print("planted", rel)
