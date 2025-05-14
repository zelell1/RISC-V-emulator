#data hits 

# 5 data miss
lw x30, 0(x2) # x2 = 0
lw x30, 0(x3) # x3 = 1024
lw x30, 0(x4) # x4 = 1024*2
lw x30, 0(x5) # x5  1024*3
lw x30, 0(x6) # x6 = 1024*4

# 15 data hits

lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)
lw x30, 0(x6)

# goal to instruction hits
addi x30 zero 1024
ecall