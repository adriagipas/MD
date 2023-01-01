#!/usr/bin/env python3

from array import array
import MD
import sys

class Z80Inst:
    
    __BYTE_OPTS= set([MD.Z80_BYTE, MD.Z80_pBYTE])
    __DESP_OPTS= set([MD.Z80_pIXd, MD.Z80_pIYd])
    __ADDR_WORD_OPTS= set([MD.Z80_ADDR, MD.Z80_WORD])
    
    __MNEMONIC= {
        MD.Z80_UNK : 'UNK ',
        MD.Z80_LD : 'LD  ',
        MD.Z80_PUSH : 'PUSH',
        MD.Z80_POP : 'POP ',
        MD.Z80_EX : 'EX  ',
        MD.Z80_EXX : 'EXX ',
        MD.Z80_LDI : 'LDI ',
        MD.Z80_LDIR : 'LDIR',
        MD.Z80_LDD : 'LDD ',
        MD.Z80_LDDR : 'LDDR',
        MD.Z80_CPI : 'CPI ',
        MD.Z80_CPIR : 'CPIR',
        MD.Z80_CPD : 'CPD ',
        MD.Z80_CPDR : 'CPDR',
        MD.Z80_ADD : 'ADD ',
        MD.Z80_ADC : 'ADC ',
        MD.Z80_SUB : 'SUB ',
        MD.Z80_SBC : 'SBC ',
        MD.Z80_AND : 'AND ',
        MD.Z80_OR : 'OR  ',
        MD.Z80_XOR : 'XOR ',
        MD.Z80_CP : 'CP  ',
        MD.Z80_INC : 'INC ',
        MD.Z80_DEC : 'DEC ',
        MD.Z80_DAA : 'DAA ',
        MD.Z80_CPL : 'CPL ',
        MD.Z80_NEG : 'NEG ',
        MD.Z80_CCF : 'CCF ',
        MD.Z80_SCF : 'SCF ',
        MD.Z80_NOP : 'NOP ',
        MD.Z80_HALT : 'HALT',
        MD.Z80_DI : 'DI  ',
        MD.Z80_EI : 'EI  ',
        MD.Z80_IM0 : 'IM   0',
        MD.Z80_IM1 : 'IM   1',
        MD.Z80_IM2 : 'IM   2',
        MD.Z80_RLCA : 'RLCA',
        MD.Z80_RLA : 'RLA ',
        MD.Z80_RRCA : 'RRCA',
        MD.Z80_RRA : 'RRA ',
        MD.Z80_RLC : 'RLC ',
        MD.Z80_RL : 'RL  ',
        MD.Z80_RRC : 'RRC ',
        MD.Z80_RR : 'RR  ',
        MD.Z80_SLA : 'SLA ',
        MD.Z80_SRA : 'SRA ',
        MD.Z80_SRL : 'SRL ',
        MD.Z80_RLD : 'RLD ',
        MD.Z80_RRD : 'RRD ',
        MD.Z80_BIT : 'BIT ',
        MD.Z80_SET : 'SET ',
        MD.Z80_RES : 'RES ',
        MD.Z80_JP : 'JP  ',
        MD.Z80_JR : 'JR  ',
        MD.Z80_DJNZ : 'DJNZ',
        MD.Z80_CALL : 'CALL',
        MD.Z80_RET : 'RET ',
        MD.Z80_RETI : 'RETI',
        MD.Z80_RETN : 'RETN',
        MD.Z80_RST00 : 'RST  00H',
        MD.Z80_RST08 : 'RST  08H',
        MD.Z80_RST10 : 'RST  10H',
        MD.Z80_RST18 : 'RST  18H',
        MD.Z80_RST20 : 'RST  20H',
        MD.Z80_RST28 : 'RST  28H',
        MD.Z80_RST30 : 'RST  30H',
        MD.Z80_RST38 : 'RST  38H',
        MD.Z80_IN : 'IN  ',
        MD.Z80_INI : 'INI ',
        MD.Z80_INIR : 'INIR',
        MD.Z80_IND : 'IND ',
        MD.Z80_INDR : 'INDR',
        MD.Z80_OUT : 'OUT ',
        MD.Z80_OUTI : 'OUTI',
        MD.Z80_OTIR : 'OTIR',
        MD.Z80_OUTD : 'OUTD',
        MD.Z80_OTDR : 'OTDR' }
    
    def __set_extra(self,op,extra,i):
        if op in Z80Inst.__BYTE_OPTS: self.extra[i]= extra[0]
        elif op in Z80Inst.__DESP_OPTS: self.extra[i]= extra[1]
        elif op in Z80Inst.__ADDR_WORD_OPTS: self.extra[i]= extra[2]
        elif op == MD.Z80_BRANCH: self.extra[i]= (extra[3][0],extra[3][1])
        
    def __init__(self,args):
        self.id= args[1]
        self.bytes= args[4]
        self.addr= (args[0]-len(self.bytes))&0xFFFF
        self.op1= args[2]
        self.op2= args[3]
        self.extra= [None,None]
        self.__set_extra(self.op1,args[5],0)
        self.__set_extra(self.op2,args[6],1)
        
    def __str_op ( self, op, i ):
        if op == MD.Z80_A : return ' A'
        elif op == MD.Z80_B : return ' B'
        elif op == MD.Z80_C : return ' C'
        elif op == MD.Z80_D : return ' D'
        elif op == MD.Z80_E : return ' E'
        elif op == MD.Z80_H : return ' H'
        elif op == MD.Z80_L : return ' L'
        elif op == MD.Z80_I : return ' I'
        elif op == MD.Z80_R : return ' R'
        elif op == MD.Z80_BYTE : return ' %02XH'%self.extra[i]
        elif op == MD.Z80_pHL : return ' (HL)'
        elif op == MD.Z80_pBC : return ' (BC)'
        elif op == MD.Z80_pDE : return ' (DE)'
        elif op == MD.Z80_pSP : return ' (SP)'
        elif op == MD.Z80_pIX : return ' (IX)'
        elif op == MD.Z80_pIY : return ' (IY)'
        elif op == MD.Z80_pIXd : return ' (IX%+d)'%self.extra[i]
        elif op == MD.Z80_pIYd : return ' (IY%+d)'%self.extra[i]
        elif op == MD.Z80_ADDR : return ' (%04XH)'%self.extra[i]
        elif op == MD.Z80_BC : return ' BC'
        elif op == MD.Z80_DE : return ' DE'
        elif op == MD.Z80_HL : return ' HL'
        elif op == MD.Z80_SP : return ' SP'
        elif op == MD.Z80_IX : return ' IX'
        elif op == MD.Z80_IXL : return ' IXL'
        elif op == MD.Z80_IXH : return ' IXH'
        elif op == MD.Z80_IY : return ' IY'
        elif op == MD.Z80_IYL : return ' IYL'
        elif op == MD.Z80_IYH : return ' IYH'
        elif op == MD.Z80_AF : return ' AF'
        elif op == MD.Z80_AF2 : return " AF'"
        elif op == MD.Z80_B0 : return ' 0'
        elif op == MD.Z80_B1 : return ' 1'
        elif op == MD.Z80_B2 : return ' 2'
        elif op == MD.Z80_B3 : return ' 3'
        elif op == MD.Z80_B4 : return ' 4'
        elif op == MD.Z80_B5 : return ' 5'
        elif op == MD.Z80_B6 : return ' 6'
        elif op == MD.Z80_B7 : return ' 7'
        elif op == MD.Z80_WORD : return ' %04XH'%self.extra[i]
        elif op == MD.Z80_F_NZ : return ' NZ'
        elif op == MD.Z80_F_Z : return ' Z'
        elif op == MD.Z80_F_NC : return ' NC'
        elif op == MD.Z80_F_C : return ' C'
        elif op == MD.Z80_F_PO : return ' PO'
        elif op == MD.Z80_F_PE : return ' PE'
        elif op == MD.Z80_F_P : return ' P'
        elif op == MD.Z80_F_M : return ' M'
        elif op == MD.Z80_BRANCH : return ' $%+d (%04XH)'%(self.extra[i][0],
                                                           self.extra[i][1])
        elif op == MD.Z80_pB : return ' (B)'
        elif op == MD.Z80_pC : return ' (C)'
        elif op == MD.Z80_pD : return ' (D)'
        elif op == MD.Z80_pE : return ' (E)'
        elif op == MD.Z80_pH : return ' (H)'
        elif op == MD.Z80_pL : return ' (L)'
        elif op == MD.Z80_pA : return ' (A)'
        elif op == MD.Z80_pBYTE : return ' (%02XH)'%self.extra[i]
        else: return str(op)

    def __str__(self):
        ret= '%04X   '%self.addr
        for b in self.bytes: ret+= ' %02x'%b
        for n in range(len(self.bytes),4): ret+= '   '
        ret+= '    '+Z80Inst.__MNEMONIC[self.id]
        if self.op1 != MD.Z80_NONE :
            ret+= self.__str_op ( self.op1, 0 )
        if self.op2 != MD.Z80_NONE :
            ret+= ','+self.__str_op ( self.op2, 1 )
        return ret
        

class SVPInst:

    __MNEMONIC= {
        MD.SVP_UNK  : 'unk ',
        MD.SVP_SUB  : 'sub ',
        MD.SVP_CMP  : 'cmp ',
        MD.SVP_ADD  : 'add ',
        MD.SVP_AND  : 'and ',
        MD.SVP_OR   : 'or  ',
        MD.SVP_EOR  : 'eor ',
        MD.SVP_MOD  : 'mod ',
        MD.SVP_LD   : 'ld  ',
        MD.SVP_CALL : 'call',
        MD.SVP_BRA  : 'bra ',
        MD.SVP_MLD  : 'mld ',
        MD.SVP_MPYA : 'mpya',
        MD.SVP_MPYS : 'mpys'
        }

    def __set_extra(self,args):
        self.ri= [args[6],args[7]]
        self.cond_f= args[8]!=0
        self.simm= args[9]
        self.adr= args[10]
        
    def __init__(self,args):
        self.addr= args[0]
        self.id= args[1]
        self.op1= args[2]
        self.op2= args[3]
        self.words= args[4][0:args[5]]
        self.__set_extra(args)

    def __str_op ( self, op, i ):
        if op == MD.SVP__ : return ' -'
        elif op == MD.SVP_X : return ' X'
        elif op == MD.SVP_Y : return ' Y'
        elif op == MD.SVP_A : return ' A'
        elif op == MD.SVP_ST : return ' ST'
        elif op == MD.SVP_STACK : return ' STACK'
        elif op == MD.SVP_PC : return ' PC'
        elif op == MD.SVP_P : return ' P'
        elif op == MD.SVP_PM0 : return ' PM0'
        elif op == MD.SVP_PM1 : return ' PM1'
        elif op == MD.SVP_PM2 : return ' PM2'
        elif op == MD.SVP_XST : return ' XST'
        elif op == MD.SVP_PM4 : return ' PM4'
        elif op == MD.SVP_EXT5 : return ' EXT5'
        elif op == MD.SVP_PMC : return ' PMC'
        elif op == MD.SVP_AL : return ' AL'
        elif op == MD.SVP_PRI: return ' (r%d)'%self.ri[i][1]
        elif op == MD.SVP_PRI_MVAL:
            return ' (r%d|%d)'%(self.ri[i][1],self.ri[i][0])
        elif op == MD.SVP_PRI_INC: return ' (r%d+!)'%self.ri[i][1]
        elif op == MD.SVP_PRI_MODINC: return ' (r%d+)'%self.ri[i][1]
        elif op == MD.SVP_PRI_MODDEC: return ' (r%d-)'%self.ri[i][1]
        elif op == MD.SVP_ADR: return ' RAM%d[%02X]'%(self.adr[1],self.adr[0])
        elif op == MD.SVP_IMM: return ' %04X'%(self.words[1])
        elif op == MD.SVP_PPRI: return ' ((r%d))'%self.ri[i][1]
        elif op == MD.SVP_PPRI_MVAL:
            return ' ((r%d|%d))'%(self.ri[i][1],self.ri[i][0])
        elif op == MD.SVP_PPRI_INC: return ' ((r%d+!))'%self.ri[i][1]
        elif op == MD.SVP_PPRI_MODINC: return ' ((r%d+))'%self.ri[i][1]
        elif op == MD.SVP_PPRI_MODDEC: return ' ((r%d-))'%self.ri[i][1]
        elif op == MD.SVP_RI: return ' r%d'%self.ri[i][1]
        elif op == MD.SVP_SIMM: return ' %02X'%self.simm
        elif op == MD.SVP_COND_UNK: return ' con_unk'
        elif op == MD.SVP_COND_TRUE: return ' true'
        elif op == MD.SVP_COND_Z:
            return ' is_zero' if self.cond_f else ' !is_zero'
        elif op == MD.SVP_COND_N:
            return ' is_neg' if self.cond_f else ' !is_neg'
        elif op == MD.SVP_ADDR: return ' %04X'%self.words[1]
        elif op == MD.SVP_PA: return ' (A)'
        elif op == MD.SVP_OP_SR: return ' op [A >>= 1]'
        elif op == MD.SVP_OP_SL: return ' op [A <<= 1]'
        elif op == MD.SVP_OP_NEG: return ' op [A = -A]'
        elif op == MD.SVP_OP_ABS: return ' op [A = ABS(A)]'
        elif op == MD.SVP_OP_UNK: return ' op [¿?]'
        else: return str(op)
        
    def __str__(self):
        ret= '%04X   '%self.addr
        for w in self.words: ret+= ' %04x'%w
        for n in range(len(self.words),2): ret+= '     '
        ret+= '    '+SVPInst.__MNEMONIC[self.id]
        if self.op1 != MD.SVP_NONE :
            ret+= self.__str_op ( self.op1, 0 )
        if self.op2 != MD.SVP_NONE :
            ret+= ','+self.__str_op ( self.op2, 1 )
        return ret
    
class Inst:
    
    __MNEMONIC= {
        MD.UNK     : "UNK     ",
        MD.ABCD    : "ABCD    ",
        MD.ADDb    : "ADD.b   ",
        MD.ADDw    : "ADD.w   ",
        MD.ADDl    : "ADD.l   ",
        MD.ADDIb   : "ADDI.b  ",
        MD.ADDIw   : "ADDI.w  ",
        MD.ADDIl   : "ADDI.l  ",
        MD.ADDQb   : "ADDQ.b  ",
        MD.ADDQl   : "ADDQ.l  ",
        MD.ADDQw   : "ADDQ.w  ",
        MD.ADDXb   : "ADDX.b  ",
        MD.ADDXw   : "ADDX.w  ",
        MD.ADDXl   : "ADDX.l  ",
        MD.ANDb    : "AND.b   ",
        MD.ANDw    : "AND.w   ",
        MD.ANDl    : "AND.l   ",
        MD.ANDIb   : "ANDI.b  ",
        MD.ANDIw   : "ANDI.w  ",
        MD.ANDIl   : "ANDI.l  ",
        MD.ASLb    : "ASL.b   ",
        MD.ASLw    : "ASL.w   ",
        MD.ASLl    : "ASL.l   ",
        MD.ASRb    : "ASR.b   ",
        MD.ASRw    : "ASR.w   ",
        MD.ASRl    : "ASR.l   ",
        MD.BCC     : "BCC     ",
        MD.BCS     : "BCS     ",
        MD.BEQ     : "BEQ     ",
        MD.BGE     : "BGE     ",
        MD.BGT     : "BGT     ",
        MD.BHI     : "BHI     ",
        MD.BLE     : "BLE     ",
        MD.BLS     : "BLS     ",
        MD.BLT     : "BLT     ",
        MD.BMI     : "BMI     ",
        MD.BNE     : "BNE     ",
        MD.BPL     : "BPL     ",
        MD.BVC     : "BVC     ",
        MD.BVS     : "BVS     ",
        MD.BCHG    : "BCHG    ",
        MD.BCLR    : "BCLR    ",
        MD.BRA     : "BRA     ",
        MD.BSET    : "BSET    ",
        MD.BSR     : "BSR     ",
        MD.BTST    : "BTST    ",
        MD.CHK     : "CHK     ",
        MD.CLRb    : "CLR.b   ",
        MD.CLRw    : "CLR.w   ",
        MD.CLRl    : "CLR.l   ",
        MD.CMPb    : "CMP.b   ",
        MD.CMPw    : "CMP.w   ",
        MD.CMPl    : "CMP.l   ",
        MD.CMPIb   : "CMPI.b  ",
        MD.CMPIw   : "CMPI.w  ",
        MD.CMPIl   : "CMPI.l  ",
        MD.CMPMb   : "CMPM.b  ",
        MD.CMPMw   : "CMPM.w  ",
        MD.CMPMl   : "CMPM.l  ",
        MD.DBCC    : "DBCC    ",
        MD.DBCS    : "DBCS    ",
        MD.DBEQ    : "DBEQ    ",
        MD.DBF     : "DBF     ",
        MD.DBGE    : "DBGE    ",
        MD.DBGT    : "DBGT    ",
        MD.DBHI    : "DBHI    ",
        MD.DBLE    : "DBLE    ",
        MD.DBLS    : "DBLS    ",
        MD.DBLT    : "DBLT    ",
        MD.DBMI    : "DBMI    ",
        MD.DBNE    : "DBNE    ",
        MD.DBPL    : "DBPL    ",
        MD.DBT     : "DBT     ",
        MD.DBVC    : "DBVC    ",
        MD.DBVS    : "DBVS    ",
        MD.DIVS    : "DIVS    ",
        MD.DIVU    : "DIVU    ",
        MD.EORb    : "EOR.b   ",
        MD.EORw    : "EOR.w   ",
        MD.EORl    : "EOR.l   ",
        MD.EORIb   : "EORI.b  ",
        MD.EORIw   : "EORI.w  ",
        MD.EORIl   : "EORI.l  ",
        MD.EXG     : "EXG     ",
        MD.EXTl    : "EXT.l   ",
        MD.EXTw    : "EXT.w   ",
        MD.ILLEGAL : "ILLEGAL ",
        MD.JMP     : "JMP     ",
        MD.JSR     : "JSR     ",
        MD.LEA     : "LEA     ",
        MD.LINK    : "LINK    ",
        MD.LSLb    : "LSL.b   ",
        MD.LSLw    : "LSL.w   ",
        MD.LSLl    : "LSL.l   ",
        MD.LSRb    : "LSR.b   ",
        MD.LSRw    : "LSR.w   ",
        MD.LSRl    : "LSR.l   ",
        MD.MOVEb   : "MOVE.b  ",
        MD.MOVEw   : "MOVE.w  ",
        MD.MOVEl   : "MOVE.l  ",
        MD.MOVEMl  : "MOVEM.l ",
        MD.MOVEMw  : "MOVEM.w ",
        MD.MOVEPw  : "MOVEP.w ",
        MD.MOVEPl  : "MOVEP.l ",
        MD.MOVEQ   : "MOVEQ   ",
        MD.MULS    : "MULS    ",
        MD.MULU    : "MULU    ",
        MD.NBCD    : "NBCD    ",
        MD.NEGb    : "NEG.b   ",
        MD.NEGl    : "NEG.l   ",
        MD.NEGw    : "NEG.w   ",
        MD.NEGXb   : "NEGX.b  ",
        MD.NEGXl   : "NEGX.l  ",
        MD.NEGXw   : "NEGX.w  ",
        MD.NOP     : "NOP     ",
        MD.NOTb    : "NOT.b   ",
        MD.NOTl    : "NOT.l   ",
        MD.NOTw    : "NOT.w   ",
        MD.ORb     : "OR.b    ",
        MD.ORl     : "OR.l    ",
        MD.ORw     : "OR.w    ",
        MD.ORIb    : "ORI.b   ",
        MD.ORIl    : "ORI.l   ",
        MD.ORIw    : "ORI.w   ",
        MD.PEA     : "PEA     ",
        MD.RESET   : "RESET   ",
        MD.ROLb    : "ROL.b   ",
        MD.ROLw    : "ROL.w   ",
        MD.ROLl    : "ROL.l   ",
        MD.RORb    : "ROR.b   ",
        MD.RORw    : "ROR.w   ",
        MD.RORl    : "ROR.l   ",
        MD.ROXLb   : "ROXL.b  ",
        MD.ROXLw   : "ROXL.w  ",
        MD.ROXLl   : "ROXL.l  ",
        MD.ROXRb   : "ROXR.b  ",
        MD.ROXRw   : "ROXR.w  ",
        MD.ROXRl   : "ROXR.l  ",
        MD.RTE     : "RTE     ",
        MD.RTR     : "RTR     ",
        MD.RTS     : "RTS     ",
        MD.SBCD    : "SBCD    ",
        MD.SCC     : "SCC     ",
        MD.SCS     : "SCS     ",
        MD.SEQ     : "SEQ     ",
        MD.SF      : "SF      ",
        MD.SGE     : "SGE     ",
        MD.SGT     : "SGT     ",
        MD.SHI     : "SHI     ",
        MD.SLE     : "SLE     ",
        MD.SLS     : "SLS     ",
        MD.SLT     : "SLT     ",
        MD.SMI     : "SMI     ",
        MD.SNE     : "SNE     ",
        MD.SPL     : "SPL     ",
        MD.ST      : "ST      ",
        MD.SVC     : "SVC     ",
        MD.SVS     : "SVS     ",
        MD.STOP    : "STOP    ",
        MD.SUBb    : "SUB.b   ",
        MD.SUBw    : "SUB.w   ",
        MD.SUBl    : "SUB.l   ",
        MD.SUBIb   : "SUBI.b  ",
        MD.SUBIw   : "SUBI.w  ",
        MD.SUBIl   : "SUBI.l  ",
        MD.SUBQb   : "SUBQ.b  ",
        MD.SUBQl   : "SUBQ.l  ",
        MD.SUBQw   : "SUBQ.w  ",
        MD.SUBXb   : "SUBX.b  ",
        MD.SUBXl   : "SUBX.l  ",
        MD.SUBXw   : "SUBX.w  ",
        MD.SWAP    : "SWAP    ",
        MD.TRAP    : "TRAP    ",
        MD.TSTb    : "TST.b   ",
        MD.TSTw    : "TST.w   ",
        MD.TSTl    : "TST.l   ",
        MD.UNLK    : "UNLK    " }
    
    __REG_OPS= set([MD.DN,MD.AN,MD.pAN,MD.pANi,MD.pANd,])
    __D16AN_OPS= set([MD.pD16AN,MD.pD16PC])
    __D8ANXN_OPS= set([MD.pD8ANXN,MD.pD8PCXN])
    __WORD_OPS= set([MD.INMw,MD.pW])
    __LONG_OPS= set([MD.INMl,MD.pL])

    def __set_extra(self,op,extra,i):
        if op==MD.INMb: self.extra[i]= extra[0]
        elif op in Inst.__REG_OPS: self.extra[i]= extra[1]
        elif op in Inst.__D16AN_OPS: self.extra[i]= extra[2]
        elif op in Inst.__D8ANXN_OPS: self.extra[i]= extra[3]
        elif op in Inst.__WORD_OPS: self.extra[i]= extra[4]
        elif op in Inst.__LONG_OPS: self.extra[i]= extra[5]
        elif op==MD.VECTOR: self.extra[i]= extra[6]
        elif op==MD.DIS16: self.extra[i]= extra[7]
        elif op==MD.LIST: self.extra[i]= extra[8]
        elif op==MD.LABEL: self.extra[i]= extra[9]
        elif op==MD.COUNT: self.extra[i]= extra[10]
        
    def __init__(self,args):
        self.id= args[1]
        self.bytes= args[4]
        self.addr=(args[0]-len(self.bytes))&0xFFFFFFFF
        self.op1= args[2]
        self.op2= args[3]
        self.extra= [None,None]
        self.__set_extra(self.op1,args[5],0)
        self.__set_extra(self.op2,args[6],1)
        
    def __str_op(self,op,i):
        if op==MD.INMb: return ' $%x'%self.extra[i]
        elif op==MD.INMw: return ' $%x'%self.extra[i]
        elif op==MD.INMl: return ' $%x'%self.extra[i]
        elif op==MD.DN: return ' D%d'%self.extra[i]
        elif op==MD.AN: return ' A%d'%self.extra[i]
        elif op==MD.pAN: return ' (A%d)'%self.extra[i]
        elif op==MD.pANi: return ' (A%d)+'%self.extra[i]
        elif op==MD.pANd: return ' -(A%d)'%self.extra[i]
        elif op==MD.pD16AN: return ' (%d,A%d)'%(self.extra[i][1],
                                               self.extra[i][0])
        elif op==MD.pD8ANXN:
            aux= '%s%d.%s'%('D' if self.extra[i][3] else 'A',
                               self.extra[i][1],
                               'w' if self.extra[i][4] else 'l')
            return ' (%d,A%d,%s)'%(self.extra[i][2],self.extra[i][0],aux)
        elif op==MD.pW: return ' ($%04X).W'%self.extra[i]
        elif op==MD.pL: return ' ($%08X).L'%self.extra[i]
        elif op==MD.pD16PC: return ' (%d,PC)'%self.extra[i][1]
        elif op==MD.pD8PCXN:
            aux= '%s%d.%s'%('D' if self.extra[i][3] else 'A',
                            self.extra[i][1],
                            'w' if self.extra[i][4] else 'l')
            return ' (%d,PC,%s)'%(self.extra[i][2],aux)
        elif op==MD.SR: return ' SR'
        elif op==MD.VECTOR: return ' %d'%self.extra[i]
        elif op==MD.DIS16: return ' %d'%self.extra[i]
        elif op==MD.USP: return ' USP'
        elif op==MD.LIST:
            aux= []
            if self.extra[i]&0x0001: aux.append('D0')
            if self.extra[i]&0x0002: aux.append('D1')
            if self.extra[i]&0x0004: aux.append('D2')
            if self.extra[i]&0x0008: aux.append('D3')
            if self.extra[i]&0x0010: aux.append('D4')
            if self.extra[i]&0x0020: aux.append('D5')
            if self.extra[i]&0x0040: aux.append('D6')
            if self.extra[i]&0x0080: aux.append('D7')
            if self.extra[i]&0x0100: aux.append('A0')
            if self.extra[i]&0x0200: aux.append('A1')
            if self.extra[i]&0x0400: aux.append('A2')
            if self.extra[i]&0x0800: aux.append('A3')
            if self.extra[i]&0x1000: aux.append('A4')
            if self.extra[i]&0x2000: aux.append('A5')
            if self.extra[i]&0x4000: aux.append('A6')
            if self.extra[i]&0x8000: aux.append('A7')
            return ' '+','.join(aux)
        elif op==MD.CCR: return ' CCR'
        elif op==MD.LABEL: return ' %d ($%08X)'%(self.extra[i][1],
                                                self.extra[i][0])
        elif op==MD.COUNT: return ' %d'%self.extra[i]
        else: return str(op)
        
    def __str__ ( self ):
        ret= '%08X   '%self.addr
        for b in self.bytes: ret+= ' %02x'%b
        for n in range(len(self.bytes),10): ret+= '   '
        ret+= '    '+Inst.__MNEMONIC[self.id]
        if self.op1 != MD.NONE : ret+= self.__str_op(self.op1,0)
        if self.op2 != MD.NONE : ret+= ','+self.__str_op(self.op2,1)
        return ret

class Z80Inst:
    
    __BYTE_OPTS= set([MD.Z80_BYTE, MD.Z80_pBYTE])
    __DESP_OPTS= set([MD.Z80_pIXd, MD.Z80_pIYd])
    __ADDR_WORD_OPTS= set([MD.Z80_ADDR, MD.Z80_WORD])
    
    __MNEMONIC= {
        MD.Z80_UNK : 'UNK ',
        MD.Z80_LD : 'LD  ',
        MD.Z80_PUSH : 'PUSH',
        MD.Z80_POP : 'POP ',
        MD.Z80_EX : 'EX  ',
        MD.Z80_EXX : 'EXX ',
        MD.Z80_LDI : 'LDI ',
        MD.Z80_LDIR : 'LDIR',
        MD.Z80_LDD : 'LDD ',
        MD.Z80_LDDR : 'LDDR',
        MD.Z80_CPI : 'CPI ',
        MD.Z80_CPIR : 'CPIR',
        MD.Z80_CPD : 'CPD ',
        MD.Z80_CPDR : 'CPDR',
        MD.Z80_ADD : 'ADD ',
        MD.Z80_ADC : 'ADC ',
        MD.Z80_SUB : 'SUB ',
        MD.Z80_SBC : 'SBC ',
        MD.Z80_AND : 'AND ',
        MD.Z80_OR : 'OR  ',
        MD.Z80_XOR : 'XOR ',
        MD.Z80_CP : 'CP  ',
        MD.Z80_INC : 'INC ',
        MD.Z80_DEC : 'DEC ',
        MD.Z80_DAA : 'DAA ',
        MD.Z80_CPL : 'CPL ',
        MD.Z80_NEG : 'NEG ',
        MD.Z80_CCF : 'CCF ',
        MD.Z80_SCF : 'SCF ',
        MD.Z80_NOP : 'NOP ',
        MD.Z80_HALT : 'HALT',
        MD.Z80_DI : 'DI  ',
        MD.Z80_EI : 'EI  ',
        MD.Z80_IM0 : 'IM   0',
        MD.Z80_IM1 : 'IM   1',
        MD.Z80_IM2 : 'IM   2',
        MD.Z80_RLCA : 'RLCA',
        MD.Z80_RLA : 'RLA ',
        MD.Z80_RRCA : 'RRCA',
        MD.Z80_RRA : 'RRA ',
        MD.Z80_RLC : 'RLC ',
        MD.Z80_RL : 'RL  ',
        MD.Z80_RRC : 'RRC ',
        MD.Z80_RR : 'RR  ',
        MD.Z80_SLA : 'SLA ',
        MD.Z80_SRA : 'SRA ',
        MD.Z80_SRL : 'SRL ',
        MD.Z80_RLD : 'RLD ',
        MD.Z80_RRD : 'RRD ',
        MD.Z80_BIT : 'BIT ',
        MD.Z80_SET : 'SET ',
        MD.Z80_RES : 'RES ',
        MD.Z80_JP : 'JP  ',
        MD.Z80_JR : 'JR  ',
        MD.Z80_DJNZ : 'DJNZ',
        MD.Z80_CALL : 'CALL',
        MD.Z80_RET : 'RET ',
        MD.Z80_RETI : 'RETI',
        MD.Z80_RETN : 'RETN',
        MD.Z80_RST00 : 'RST  00H',
        MD.Z80_RST08 : 'RST  08H',
        MD.Z80_RST10 : 'RST  10H',
        MD.Z80_RST18 : 'RST  18H',
        MD.Z80_RST20 : 'RST  20H',
        MD.Z80_RST28 : 'RST  28H',
        MD.Z80_RST30 : 'RST  30H',
        MD.Z80_RST38 : 'RST  38H',
        MD.Z80_IN : 'IN  ',
        MD.Z80_INI : 'INI ',
        MD.Z80_INIR : 'INIR',
        MD.Z80_IND : 'IND ',
        MD.Z80_INDR : 'INDR',
        MD.Z80_OUT : 'OUT ',
        MD.Z80_OUTI : 'OUTI',
        MD.Z80_OTIR : 'OTIR',
        MD.Z80_OUTD : 'OUTD',
        MD.Z80_OTDR : 'OTDR' }
    
    def __set_extra ( self, op, extra, i ):
        if op in Z80Inst.__BYTE_OPTS: self.extra[i]= extra[0]
        elif op in Z80Inst.__DESP_OPTS: self.extra[i]= extra[1]
        elif op in Z80Inst.__ADDR_WORD_OPTS: self.extra[i]= extra[2]
        elif op == MD.Z80_BRANCH: self.extra[i]= (extra[3][0],extra[3][1])
    
    def __init__ ( self, args ):
        self.id= args[1]
        self.bytes= args[4]
        self.addr= (args[0]-len(self.bytes))&0xFFFF
        self.op1= args[2]
        self.op2= args[3]
        self.extra= [None,None]
        self.__set_extra ( self.op1, args[5], 0 )
        self.__set_extra ( self.op2, args[6], 1 )
        
    def __str_op ( self, op, i ):
        if op == MD.Z80_A : return ' A'
        elif op == MD.Z80_B : return ' B'
        elif op == MD.Z80_C : return ' C'
        elif op == MD.Z80_D : return ' D'
        elif op == MD.Z80_E : return ' E'
        elif op == MD.Z80_H : return ' H'
        elif op == MD.Z80_L : return ' L'
        elif op == MD.Z80_I : return ' I'
        elif op == MD.Z80_R : return ' R'
        elif op == MD.Z80_BYTE : return ' %02XH'%self.extra[i]
        elif op == MD.Z80_pHL : return ' (HL)'
        elif op == MD.Z80_pBC : return ' (BC)'
        elif op == MD.Z80_pDE : return ' (DE)'
        elif op == MD.Z80_pSP : return ' (SP)'
        elif op == MD.Z80_pIX : return ' (IX)'
        elif op == MD.Z80_pIY : return ' (IY)'
        elif op == MD.Z80_pIXd : return ' (IX%+d)'%self.extra[i]
        elif op == MD.Z80_pIYd : return ' (IY%+d)'%self.extra[i]
        elif op == MD.Z80_ADDR : return ' (%04XH)'%self.extra[i]
        elif op == MD.Z80_BC : return ' BC'
        elif op == MD.Z80_DE : return ' DE'
        elif op == MD.Z80_HL : return ' HL'
        elif op == MD.Z80_SP : return ' SP'
        elif op == MD.Z80_IX : return ' IX'
        elif op == MD.Z80_IXL : return ' IXL'
        elif op == MD.Z80_IXH : return ' IXH'
        elif op == MD.Z80_IY : return ' IY'
        elif op == MD.Z80_IYL : return ' IYL'
        elif op == MD.Z80_IYH : return ' IYH'
        elif op == MD.Z80_AF : return ' AF'
        elif op == MD.Z80_AF2 : return " AF'"
        elif op == MD.Z80_B0 : return ' 0'
        elif op == MD.Z80_B1 : return ' 1'
        elif op == MD.Z80_B2 : return ' 2'
        elif op == MD.Z80_B3 : return ' 3'
        elif op == MD.Z80_B4 : return ' 4'
        elif op == MD.Z80_B5 : return ' 5'
        elif op == MD.Z80_B6 : return ' 6'
        elif op == MD.Z80_B7 : return ' 7'
        elif op == MD.Z80_WORD : return ' %04XH'%self.extra[i]
        elif op == MD.Z80_F_NZ : return ' NZ'
        elif op == MD.Z80_F_Z : return ' Z'
        elif op == MD.Z80_F_NC : return ' NC'
        elif op == MD.Z80_F_C : return ' C'
        elif op == MD.Z80_F_PO : return ' PO'
        elif op == MD.Z80_F_PE : return ' PE'
        elif op == MD.Z80_F_P : return ' P'
        elif op == MD.Z80_F_M : return ' M'
        elif op == MD.Z80_BRANCH : return ' $%+d (%04XH)'%(self.extra[i][0],
                                                       self.extra[i][1])
        elif op == MD.Z80_pB : return ' (B)'
        elif op == MD.Z80_pC : return ' (C)'
        elif op == MD.Z80_pD : return ' (D)'
        elif op == MD.Z80_pE : return ' (E)'
        elif op == MD.Z80_pH : return ' (H)'
        elif op == MD.Z80_pL : return ' (L)'
        elif op == MD.Z80_pA : return ' (A)'
        elif op == MD.Z80_pBYTE : return ' (%02XH)'%self.extra[i]
        else: return str(op)
        
    def __str__ ( self ):
        ret= '%04X   '%self.addr
        for b in self.bytes: ret+= ' %02x'%b
        for n in range(len(self.bytes),4): ret+= '   '
        ret+= '    '+Z80Inst.__MNEMONIC[self.id]
        if self.op1 != MD.Z80_NONE :
            ret+= self.__str_op ( self.op1, 0 )
        if self.op2 != MD.Z80_NONE :
            ret+= ','+self.__str_op ( self.op2, 1 )
        
        return ret

class Record:
    
    def __init__(self,inst):
        self.inst= inst
        self.nexec= 0

class Tracer:
    
    MA_ROM= 0x01
    MA_RAM= 0x02
    MA_PORTS= 0x04
    MA_68MEM= 0x08
    MA_VDP= 0x10
    
    def __init__(self):
        self.svp_print_insts= False
        self.svp_next_addr= 0
        self.svp_last_addr= None
        self.svp_records= None
        self.svp_max_nexec= 0
        self.z80_next_addr= 0
        self.z80_last_addr= None
        self.z80_print_insts= False
        self.z80_records= None
        self.z80_max_nexec= 0
        self.next_addr= 0
        self.records= None
        self.print_insts= False
        self.last_addr= None
        self.max_nexec= 0
        self.print_mem_access= 0
        self.print_mem_access_z80= 0
        self.last_mem_addr= None
        
    def enable_print_insts(self,enabled):
        self.print_insts= enabled
        
    def enable_print_insts_z80(self,enabled):
        self.z80_print_insts= enabled

    def enable_print_insts_svp(self,enabled):
        self.svp_print_insts= enabled
        
    def enable_print_mem_access ( self, mask ):
        self.print_mem_access= mask
        
    def enable_print_mem_access_z80(self,mask):
        self.print_mem_access_z80= mask
        
    def dump_insts ( self ):
        def get_prec ( val ):
            prec= 0
            while val != 0:
                prec+= 1
                val//= 10
            return prec
        prec= get_prec ( self.max_nexec )
        i= 0
        prev= None
        while i <  0x1000000:
        #while i < 0x400000:
            rec= self.records[i]
            if rec == None :
                prev= False
                i+= 1
            else:
                if not prev: print ( '\n' )
                print ( ('[%'+str(prec)+'d] %s')%(rec.nexec,rec.inst) )
                prev= True
                i+= len(rec.inst.bytes)
                
    def dump_insts_z80(self):
        def get_prec ( val ):
            prec= 0
            while val != 0:
                prec+= 1
                val//= 10
            return prec
        prec= get_prec(self.z80_max_nexec)
        i= 0
        prev= None
        while i < 0x2000:
            rec= self.z80_records[i]
            if rec == None :
                prev= False
                i+= 1
            else:
                if not prev: print ( '\n' )
                print ( ('[Z80] [%'+str(prec)+'d] %s')%(rec.nexec,rec.inst) )
                prev= True
                i+= len(rec.inst.bytes)

    def dump_insts_svp(self):
        def get_prec ( val ):
            prec= 0
            while val != 0:
                prec+= 1
                val//= 10
            return prec
        prec= get_prec(self.svp_max_nexec)
        i= 0
        prev= None
        while i < 0x10000:
            rec= self.svp_records[i]
            if rec == None :
                prev= False
                i+= 1
            else:
                if not prev: print ( '\n' )
                print ( ('[SVP] [%'+str(prec)+'d] %s')%(rec.nexec,rec.inst) )
                prev= True
                i+= len(rec.inst.words)
    
    def cpu_step(self,*args):
        if args[0]==MD.STEP_STOP:
            print('STOP')
            return
        elif args[0]==MD.STEP_RESET:
            print('RESET')
            return
        elif args[0]==MD.STEP_AUTOVECTOR:
            print('AUTOVECTOR %X (%d)'%(args[1],args[2]))
            return
        self.next_addr= args[1]
        inst= Inst(args[1:])
        self.last_addr= inst.addr
        inst.addr&= 0xFFFFFF
        #if inst.addr < 0x400000:
        if inst.addr <  0x1000000:
            if self.records==None:
                #self.records= [None]*0x400000
                self.records=  [None]*0x1000000
            addr= inst.addr
            aux2= self.records[addr]
            if aux2==None:
                aux2= self.records[addr]= Record(inst)
            aux2.nexec+= 1
            if aux2.nexec > self.max_nexec:
                self.max_nexec= aux2.nexec
        if self.print_insts:
            print(inst)
    
    def cpu_step_svp(self,*args):
        inst= SVPInst(args)
        self.svp_next_addr= (inst.addr+len(inst.words))&0xFFFF
        self.svp_last_addr= inst.addr
        if self.svp_records==None:
            self.svp_records= [None]*0x10000
        addr= inst.addr
        aux2= self.svp_records[addr]
        if aux2==None:
            aux2= self.svp_records[addr]= Record(inst)
        aux2.inst= inst # Maxaca
        aux2.nexec+= 1
        if aux2.nexec > self.svp_max_nexec:
            self.svp_max_nexec= aux2.nexec
        if self.svp_print_insts: print('[SVP] %s'%inst)

    def cpu_step_z80(self,*args):
        if args[0]==MD.Z80_IRQ:
            if self.z80_print_insts: print('[Z80] IRQ 00%02X'%args[1])
            return
        elif args[0]==MD.Z80_NMI:
            if self.print_insts_z80: print('[Z80] NMI')
            return
        self.z80_next_addr= args[1]
        inst= Z80Inst(args[1:])
        self.z80_last_addr= inst.addr
        if inst.addr < 0x2000:
            if self.z80_records==None:
                self.z80_records= [None]*0x2000
            addr= inst.addr
            aux2= self.z80_records[addr]
            if aux2==None:
                aux2= self.z80_records[addr]= Record(inst)
            aux2.inst= inst # Maxaca
            aux2.nexec+= 1
            if aux2.nexec > self.z80_max_nexec:
                self.z80_max_nexec= aux2.nexec
        if self.z80_print_insts: print('[Z80] %s'%inst)
    
    def __check_mem_access_z80(self,addr):
        if addr<0x2000:
            if self.print_mem_access_z80&Tracer.MA_RAM==0: return False
        elif addr<0x4000: return False
        elif addr<0x4004:
            if self.print_mem_access_z80&Tracer.MA_PORTS==0: return False
        elif addr<0x6000: return False
        elif addr==0x6000:
            if self.print_mem_access_z80&Tracer.MA_PORTS==0: return False
        elif addr<0x7F11: return False
        elif addr==0x7F11:
            if self.print_mem_access_z80&Tracer.MA_PORTS==0: return False
        elif addr<0x8000: return False
        else:
            if self.print_mem_access_z80&Tracer.MA_68MEM==0: return False
        return True
        
    def mem_access_z80(self,typ,addr,data):
        if not self.__check_mem_access_z80(addr): return
        if typ==MD.READ :
            print('MEMZ80[%04X] -> %02X'%(addr,data))
        else:
            print('MEMZ80[%04X]= %02X'%(addr,data))
    
    def __check_mem_access(self,addr):
        if addr==0xA01FFD: return True
        if addr < 0x400000:
            if self.print_mem_access&Tracer.MA_ROM==0: return False
        elif addr>=0xC00000 and addr<0xC00008:
            if self.print_mem_access&Tracer.MA_VDP==0: return False
        elif addr < 0xFF0000: return False# Altres
        else:
            if self.print_mem_access&Tracer.MA_RAM==0: return False
        return True
            
    def mem_access(self,typ,addr,data):
        if not self.__check_mem_access(addr): return
        self.last_mem_access_addr= addr
        self.last_mem_access_data= data
        self.last_mem_access_typ= typ
        if typ==MD.READ :
            print('MEM[%08X] -> %04X'%(addr,data))
        else:
            print('MEM[%08X]= %04X'%(addr,data))

    def mem_access8(self,typ,addr,data):
        if not self.__check_mem_access(addr): return
        if typ==MD.READ :
            print('MEM8[%08X] -> %02X'%(addr,data))
        else:
            print('MEM8[%08X]= %02X'%(addr,data))

class Color:
    
    @staticmethod
    def get ( r, g, b ):
        return (r<<16)|(g<<8)|b
    
    @staticmethod
    def get_components ( color ):
        return (color>>16,(color>>8)&0xff,color&0xff)

class Img:
    
    WHITE= Color.get ( 255, 255, 255 )
    
    def __init__ ( self, width, height ):
        self._width= width
        self._height= height
        self._v= []
        for i in range(0,height):
            self._v.append ( array('i',[Img.WHITE]*width) )
    
    def __getitem__ ( self, ind ):
        return self._v[ind]
    
    def write ( self, to ):
        if type(to) == str :
            to= open ( to, 'wt' )
        to.write ( 'P3\n ')
        to.write ( '%d %d\n'%(self._width,self._height) )
        to.write ( '255\n' )
        for r in self._v:
            for c in r:
                aux= Color.get_components ( c )
                to.write ( '%d %d %d\n'%(aux[0],aux[1],aux[2]) )

def bytes2color(b0,b1,b2,b3,pal16):
    ret= array('i',[0]*8)
    ret[0]= pal16[(b0>>4)&0xF]
    ret[1]= pal16[b0&0xF]
    ret[2]= pal16[(b1>>4)&0xF]
    ret[3]= pal16[b1&0xF]
    ret[4]= pal16[(b2>>4)&0xF]
    ret[5]= pal16[b2&0xF]
    ret[6]= pal16[(b3>>4)&0xF]
    ret[7]= pal16[b3&0xF]
    return ret

aux= []
f= 255.0/15.0
for i in range(0,16):
    val= int(i*f)
    aux.append(Color.get(val,val,val))
PAL16= array('i',aux)
PALS16=[PAL16,PAL16,PAL16,PAL16]

def vramtiles2img(vram,pal=PAL16):
    ret= Img(64*8,32*8)
    offy= 0; p= 0
    for r in range(0,32):
        offx= 0
        for c in range(0,64):
            for j in range(0,8):
                line= bytes2color(vram[p],vram[p+1],vram[p+2],vram[p+3],pal)
                p+= 4
                r2= offy+j
                for k in range(0,8):
                    ret[r2][offx+k]= line[k]
            offx+= 8
        offy+= 8
    return ret

def cram2pals(cram):
    factor= 255.0/7.0
    ret= []; j= 0
    for i in range(0,4):
        aux= []
        for k in range(0,16):
            val= cram[j]
            aux.append(Color.get(int((val&0x7)*factor),
                                 int(((val>>3)&0x7)*factor),
                                 int(((val>>6)&0x7)*factor)))
            j+= 1
        ret.append(aux)
    return ret

def cram2img(cram):
    pals= cram2pals(cram)
    ret= Img(16*8,4*8)
    offy= 0; p= 0
    for r in range(0,4):
        offx= 0
        for c in range(0,16):
            color= pals[r][c]
            for j in range(0,8):
                r2= offy+j
                for k in range(0,8):
                    ret[r2][offx+k]= color
            offx+= 8
        offy+= 8
    return ret

def draw_nt(vram,width,height,addr,pals,imode=False):
    htile,desp= (16,6) if imode else (8,5)
    ret= Img(width*8,height*htile)
    pos= addr
    offy= 0
    for r in range(0,height):
        offx= 0
        for c in range(0,width):
            b0,b1= vram[pos],vram[pos+1]
            pos+= 2
            paddr= (((b0&0x7)<<8)|b1)<<desp
            hf= (b0&0x08)!=0
            pal= (b0>>5)&0x3
            if b0&0x10:
                paddr+= 7*4
                for j in range(0,htile):
                    line= bytes2color(vram[paddr],vram[paddr+1],
                                      vram[paddr+2],vram[paddr+3],
                                      pals[pal])
                    if hf: line.reverse()
                    paddr-= 4
                    r2= offy+j
                    for k in range(0,8):
                        ret[r2][offx+k]= line[k]
            else:
                for j in range(0,htile):
                    line= bytes2color(vram[paddr],vram[paddr+1],
                                      vram[paddr+2],vram[paddr+3],
                                      pals[pal])
                    if hf: line.reverse()
                    paddr+= 4
                    r2= offy+j
                    for k in range(0,8):
                        ret[r2][offx+k]= line[k]
            offx+= 8
        offy+= htile
    return ret

def print_sprite_table(vram,table_addr,maxspr=64):
    n= next= 0
    while n==0 or (next>0 and next<maxspr):
        addr= table_addr+next*8
        vpos= ((vram[addr]<<8)|vram[addr+1])&0x3FF
        width= ((vram[addr+2]>>2)&0x3)+1
        height= (vram[addr+2]&0x3)+1
        next= vram[addr+3]
        print('NEXT: ',next)
        priority= (vram[addr+4]&0x80)!=0
        pal= (vram[addr+4]&0x60)>>5
        vflip= (vram[addr+4]&0x10)!=0
        hflip= (vram[addr+4]&0x08)!=0
        tile= ((vram[addr+4]<<8)|vram[addr+5])&0x7FF
        hpos= ((vram[addr+6]<<8)|vram[addr+7])&0x1FF
        print(('%d => HPOS: %d VPOS: %d WIDTH: %d'+
               ' HEIGHT: %d PRIO: %d PAL: %d'+
               ' VFLIP: %d HFLIP: %d TILE: %d')%(n,hpos,vpos,width,height,
                                                 priority,pal,vflip,hflip,
                                                 tile))
        n+= 1
        

MD.init()
MD.set_rom(open ("ROM.gen",'rb').read(),True)
rom= MD.get_rom()
print("Console: '%s'"%rom['console'])
print("Firm and build: '%s'"%rom['firm_build'])
try: print("Domestic name: '%s'"%rom['dom_name'].decode('shift-jis'))
except: print("Domestic name: '%s'"%rom['dom_name'])
try: print("International name: '%s'"%rom['int_name'].decode('ascii'))
except: print("International name: '%s'"%rom['int_name'])
print("Type and serial number: '%s'"%rom['type_snumber'])
print("Checksum: %04x"%rom['checksum'])
print("Checksum ok: %s"%str(rom['checksum_ok']))
print("I/O support: '%s'"%rom['io'])
print("Start ROM: %08x"%rom['start'])
print("End ROM: %08x"%rom['end'])
print("Start RAM: %08x"%rom['start_ram'])
print("End RAM: %08x"%rom['end_ram'])
print("SRAM id: '%s'"%rom['sramid'])
if rom['sram_flags']&MD.SRAMINFO_AVAILABLE:
    if rom['sram_flags']&MD.SRAMINFO_ISFORBACKUP: print("  BACKTUP")
    if rom['sram_flags']&MD.SRAMINFO_ODDBYTES: print("  ODDBYTES")
    if rom['sram_flags']&MD.SRAMINFO_EVENBYTES: print("  EVENBYTES")
    print("  Start SRAM: %08x"%rom['start_sram'])
    print("  End SRAM: %08x"%rom['end_sram'])
print("Modem: '%s'"%rom['modem'])
print("Notes: '%s'"%rom['notes'])
print("Country codes: '%s'"%rom['ccodes'])

####################################
#print(' '.join(['%02X'%x for x in rom['bytes'][0x453C:0x453C+32]]))
#print(' '.join(['%02X'%x for x in rom['bytes'][0x001D8726:0x001D8726+32]]))
#print(rom['bytes'][0x00200001:0x00203fff])
#print([int(x) for x in rom['bytes'][0x1B4:0x1B4+4]])
#sys.exit(0)
t= Tracer()
MD.set_tracer(t)
#t.enable_print_insts(True)
#t.enable_print_insts_svp(True)
#t.enable_print_mem_access(Tracer.MA_ROM|Tracer.MA_RAM|Tracer.MA_VDP)
#t.enable_print_mem_access(Tracer.MA_RAM|Tracer.MA_VDP)
MD.loop()
#for i in range(0,1000000): MD.trace()
#MD.trace()
#while (t.last_mem_access_addr!=0xFFFFFD60):
#       t.last_mem_access_typ==MD.READ):
#       t.last_mem_access_data!=0x817c):
#while True:
#    MD.trace()
#while True:
#    if t.last_mem_access_addr==0xFFFFFDF6:
#        print('IEP ',hex(t.last_mem_access_data))
#    MD.trace()
#for i in range(0,1000): MD.trace()
#t.dump_insts()
#t.dump_insts_svp()
MD.close()
sys.exit(0)
#ram= MD.get_ram()
#print('%02x'%ram[0xFD60^0x1])
vram= MD.get_vram()
cram= MD.get_cram()
pals= cram2pals(cram)
cram2img(cram).write(open('pal.pnm','w'))
vramtiles2img(vram).write(open('tiles.pnm','w'))
draw_nt(vram,64,32,0xA000,pals,False).write(open('sc1.pnm','w'))
draw_nt(vram,64,64,0xC000,pals,False).write(open('sc2.pnm','w'))
draw_nt(vram,64,64,0xE000,pals,False).write(open('sc3.pnm','w'))
draw_nt(vram,64,32,0xB000,pals,False).write(open('sc4.pnm','w'))
draw_nt(vram,64,64,0xC000,pals).write(open('nt.pnm','w'))
draw_nt(vram,64,64,0xE000,pals).write(open('nt2.pnm','w'))
draw_nt(vram,64,32,0xB000,pals).write(open('nt3.pnm','w'))
draw_nt(vram,64,32,0xA800,pals).write(open('nt4.pnm','w'))
print_sprite_table(vram,0xBC00)
MD.close()
sys.exit(0)
#t= Tracer()
#MD.set_tracer(t)
#MD.trace()
#while t.last_addr!=0x39668: MD.trace()
#t.enable_print_insts_z80(True)
#t.enable_print_insts(True)
#t.enable_print_mem_access(Tracer.MA_ROM|Tracer.MA_RAM)
#t.enable_print_mem_access(Tracer.MA_RAM)
#t.enable_print_mem_access(0)
#t.enable_print_mem_access_z80(Tracer.MA_PORTS)
#for i in range(0,2000000): MD.trace()
#for i in range(0,100):
#    MD.trace()
#    while t.last_addr!=0x2254: MD.trace()
#while t.last_addr!=0x001E3F40: MD.trace()
#t.enable_print_insts(True)
#while t.last_addr!=0x00FFFE00: MD.trace()
#for i in range(0,10): MD.trace()
#for i in range(0,100000): MD.trace()
#for i in range(0,2000000): MD.trace()
#n=0
#MD.trace()
#while t.last_addr!=0x00000BB0:
#for i in range(0,1000000):
#    MD.trace()
#    if n==10:
#        MD.check_signals()
#        n= 0
#    n+= 1
#for i in range(0,100): MD.trace()
#t.dump_insts()
#t.dump_insts_z80()
#sys.exit(0)
vram= MD.get_vram()
cram= MD.get_cram()
pals= cram2pals(cram)
cram2img(cram).write(open('pal.pnm','w'))
vramtiles2img(vram).write(open('tiles.pnm','w'))
draw_nt(vram,64,64,0xE000,pals).write(open('nt.pnm','w'))
draw_nt(vram,64,64,0xE000,PALS16).write(open('nt_pal16.pnm','w'))
MD.close()
