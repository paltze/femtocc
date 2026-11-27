; --- memset n-bytes relative to FP start --- ;

; R0 = n (byte count)
; R1 = x (value to set)
; R2 = dst offset relative to FP
; Clobbers: R0, R2

__femto_smemset:
    JZ __femto_smemset_exit, R0

__femto_smemset_loop:
    STORE R1, FP, R2
    INC R2
    DEC R0
    JNZ __femto_smemset_loop, R0

__femto_smemset_exit:
    RET

; ---  memset n-bytes relative to FP end  --- ;
