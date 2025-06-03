import sys

savedata = bytearray(open("base.sav", "rb").read())
sz = 3968

stub = bytearray(open("stub.dmp", "rb").read())
for i in range(0, len(stub)):
    savedata[0xf238+i] = stub[i]
    
data = bytearray(open("8086_full.bin", "rb").read())
for i in range(0, len(data)):
    savedata[i] = data[i]

checksum = 0

for i in range(0xf000, 0xf000 + sz, 4):
    r = savedata[i+3] << 24
    r |= savedata[i+2] << 16
    r |= savedata[i+1] << 8
    r |= savedata[i]
    checksum += r

checksum = (checksum & 0xffff) + ((checksum >> 16) & 0xffff)
checksum = checksum & 0xffff

savedata[0xfff7] = checksum >> 8
savedata[0xfff6] = checksum & 0xff

# -------------------

checksum = 0
sz = 3848

for i in range(0x12000, 0x12000 + sz, 4):
    r = savedata[i+3] << 24
    r |= savedata[i+2] << 16
    r |= savedata[i+1] << 8
    r |= savedata[i]
    checksum += r

checksum = (checksum & 0xffff) + ((checksum >> 16) & 0xffff)
checksum = checksum & 0xffff

savedata[0x12ff7] = checksum >> 8
savedata[0x12ff6] = checksum & 0xff

with open("main.sav", "wb") as fp:
    fp.write(savedata)