sects = []

files = (
    '8086_init.bin',
    'pad',
    '8086_text.bin',
    '8086_fini.bin',
    '8086_rodata.bin',
    '8086_eh_frame.bin',
    '8086_data.bin',
    '8086_init_array.bin',
    '8086_fini_array.bin'   
    'pad' 
)

for i in files:
    if i == 'pad':
        sects.append(b'\0\0\0\0')
    else:
        with open(i, 'rb') as f:
            sects.append(f.read())

with open('8086_abs.txt', 'rb') as f:
    lines = f.readlines()

abs_stream = b''

for l in lines:
    if b'*ABS*' in l and l[9] == ord('g'):
        abs_stream += l[6:8] + l[4:6] + l[2:4] + l[0:2]

sects.append(bytes.fromhex(abs_stream.decode('ascii')))

with open('8086_full.bin', 'wb') as f:
    f.write(b''.join(sects))