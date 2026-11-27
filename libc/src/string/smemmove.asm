; --- memmove n-bytes relative to FP start --- ;

; R0 = n (byte count)
; R1 = src offset relative to FP
; R2 = dst offset relative to FP
; Overlap between the two allowed
; Clobbers: R0, R1, R2, R3

__femto_smemmove:
    JZ  __femto_smemmove_exit, R0       ; n == 0 -> nothing to copy
    JE  __femto_smemmove_exit, R1, R2   ; src == dst -> nothing to copy
    JLT __femto_smemmove_bwd, R1, R2    ; if src < dst, copy backward

__femto_smemmove_fwd:
    LOAD  R3, FP, R1                    ; R3 <- mem[FP + src]
    STORE R3, FP, R2                    ; mem[FP + dst] <- R3
    INC R1
    INC R2
    DEC R0
    JNZ __femto_smemmove_fwd, R0
    RET

__femto_smemmove_bwd:
    DEC R0                              ; R0 = n - 1
    ADD R1, R1, R0                      ; R1 = src + (n - 1)
    ADD R2, R2, R0                      ; R2 = dst + (n - 1)
    INC R0                              ; R0 = n (remaining byte count)

__femto_smemmove_bwd_loop:
    LOAD  R3, FP, R1                    ; R3 <- mem[FP + src]
    STORE R3, FP, R2                    ; mem[FP + dst] <- R3
    DEC R1
    DEC R2
    DEC R0
    JNZ __femto_smemmove_bwd_loop, R0

__femto_smemmove_exit:
    RET

; ---  memmove n-bytes relative to FP end  --- ;
