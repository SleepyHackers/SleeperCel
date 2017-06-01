.section .multiboot_header
header_start:		
dd 0xe85250d6
dd 0
dd header_end -header_start
dd 0x100000000 -(0xe85250d6 + 0 + (header_end - header_start))
;; tags
dw 0
dw 4
dw 8
header_end:
