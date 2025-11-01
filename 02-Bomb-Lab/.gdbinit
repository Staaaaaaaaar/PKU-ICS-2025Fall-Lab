set args psol.txt
b explode_bomb

layout asm
layout regs

b phase_1
b phase_2
b phase_3
b phase_4
b phase_5
b phase_6
b phase_defused
b abracadabra
b alohomora
b secret_phase

b *(explode_bomb + 0x44)
command
j *(explode_bomb + 0x81)
end

r