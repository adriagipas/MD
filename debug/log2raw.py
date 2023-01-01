import sys,struct

if len(sys.argv)!=3:
    sys.exit('%s <log> <out>'%sys.argv[0])
log_fn= sys.argv[1]
out_fn= sys.argv[2]

with open(log_fn) as f:
    with open(out_fn,'wb') as f2:
        for l in f:
            l= l.strip().split()
            rep,val= int(l[0]),int(l[-1])
            valb= struct.pack('<h',val)
            for r in range(rep):
                f2.write(valb)

