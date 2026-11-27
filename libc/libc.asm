;; libc for femtocc

; --- memcpy n-bytes relative to FP start --- ;

; R0 = n (byte count)
; R1 = src offset relative to FP
; R2 = dst offset relative to FP
; Clobbers: R0, R1, R2, R3
smemcpy:
    JZ smemcpy_exit, R0
    DEC R0

    LOAD R3, FP, R1
    STORE R3, FP, R2

    INC R1
    INC R2
    JMP smemcpy

smemcpy_exit:
    RET

; ---  memcpy n-bytes relative to FP end  --- ;
